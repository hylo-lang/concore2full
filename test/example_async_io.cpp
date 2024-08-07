#include "concore2full/profiling.h"
#include "concore2full/suspend.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

namespace async_io {
//! The type to describe a file
using native_file_desc_t = int;

//! The type of operation we are registering
enum class oper_type {
  read,
  write,
};

//! Base class to represent the actions of an operation to be passed to an I/O loop
struct oper_body_base {
  ~oper_body_base() = default;
  //! Called to run the operation. If this returns false, the operation needs to be retried (for
  //! I/O operations).
  virtual auto try_run() noexcept -> bool = 0;
  //! Called to announce that the operation was cancelled.
  virtual auto set_stopped() noexcept -> void = 0;
};

//! Class that implements an I/O loop, using `pool()` for I/O.
//!
//! One can add I/O and non-I/O events into the loop, and let the loop execute them.
//! For the I/O operations, the given actions are executed multiple times until the operation
//! completes with success.
class poll_io_loop {
public:
  poll_io_loop();

  //! Run the loop once to execute maximum one operation.
  //! If there are no operations to execute (or, no I/O completed) this will return false.
  auto run_one() -> bool;

  //! Run the loop to process operations
  //! Stops after `stop()` is called, and all operations in our queues are drained.
  auto run() -> std::size_t;

  //! Stops processing any more operations
  auto stop() noexcept -> void;

  //! Check if we were told to stop
  auto is_stopped() const noexcept -> bool { return should_stop_.load(std::memory_order_acquire); }

  //! Add an I/O operation to be executed into our loop
  //! The body will be called multiple times, until the operation succeeds (body function returns
  //! true)
  auto add_io_oper(native_file_desc_t fd, oper_type t, oper_body_base* oper) -> void;

  //! Add a non-I/O operation in our loop
  //! The body will only be executed once.
  auto add_non_io_oper(oper_body_base* oper) -> void;

private:
  //! An operation that can be executed though this loop
  struct io_oper {
    native_file_desc_t fd_;
    short events_;
    oper_body_base* body_;

    io_oper(native_file_desc_t fd, short events, oper_body_base* body)
        : fd_(fd), events_(events), body_(body) {}
  };

  std::atomic<bool> should_stop_{false};

  // IO and non-IO operations created by various threads, not yet consumed by our loop
  std::vector<io_oper> in_opers_;
  std::mutex in_bottleneck_;

  // input operations for which we have ownership
  std::vector<io_oper> owned_in_opers_;
  std::vector<io_oper>::const_iterator next_op_to_process_;

  // data for which we call poll; the two vectors are kept in sync
  std::vector<pollfd> poll_data_;
  std::vector<oper_body_base*> poll_opers_;
  native_file_desc_t poll_wake_fd_[2];
  std::size_t check_completions_start_idx_{0};

  auto check_in_ops() -> void;
  auto handle_one_owned_in_op() -> bool;
  auto check_for_one_io_completion() -> bool;
  auto do_poll() -> bool;
};

inline poll_io_loop::poll_io_loop() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  static constexpr std::size_t expected_max_in_ops = 128;
  static constexpr std::size_t expected_max_pending_ops = 512;
  in_opers_.reserve(expected_max_in_ops);
  owned_in_opers_.reserve(expected_max_in_ops);
  poll_data_.reserve(expected_max_pending_ops);
  poll_opers_.reserve(expected_max_pending_ops);

  next_op_to_process_ = owned_in_opers_.begin();

  int rc = pipe(poll_wake_fd_);
  if (rc != 0)
    throw std::system_error(std::error_code(errno, std::system_category()));
  fcntl(poll_wake_fd_[0], F_SETFL, O_NONBLOCK);
  fcntl(poll_wake_fd_[1], F_SETFL, O_NONBLOCK);
  zone.set_param("read_fd", static_cast<int64_t>(poll_wake_fd_[0]));
  zone.set_param("write_fd", static_cast<int64_t>(poll_wake_fd_[1]));

  poll_data_.push_back(pollfd{poll_wake_fd_[0], POLLIN, 0});
  poll_opers_.push_back(nullptr);
}

inline auto poll_io_loop::run_one() -> bool {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Now try to execute some outstanding ops
  while (true) {
    if (should_stop_.load(std::memory_order_relaxed))
      return false;

    // If there are owned input ops, handle them before checking for new ops
    if (handle_one_owned_in_op())
      return true;

    // Check if we have new ops, to move them on the processing for this thread
    // PROFILING_PLOT_INT("I/O ops", int(poll_data_.size()) - 1);
    check_in_ops();
    // PROFILING_PLOT_INT("I/O ops", int(poll_data_.size()) - 1);

    // Check for newly added owned input ops
    if (handle_one_owned_in_op())
      return true;

    // Check if we have any completions
    if (check_for_one_io_completion())
      return true;
    // when calling do_poll, all events in poll_data_ are checked
    if (!do_poll())
      return false;
  }
}

inline auto poll_io_loop::run() -> std::size_t {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  std::size_t num_completed{0};
  // Run as many operations as possible, until the stop signal occurs
  while (!should_stop_.load(std::memory_order_acquire)) {
    if (run_one())
      num_completed++;
  }

  concore2full::profiling::zone z2{CURRENT_LOCATION_N("exiting I/O loop")};
  // If we have a stop signal, and still have outstanding operations, cancel them
  for (oper_body_base* op_body : poll_opers_) {
    if (op_body) {
      op_body->set_stopped();
      num_completed++;
    }
  }

  return num_completed;
}

inline auto poll_io_loop::stop() noexcept -> void {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  should_stop_.store(true, std::memory_order_release);
  // Wake up the loop, so it can see the stop signal
  const char msg = 1;
  auto w = write(poll_wake_fd_[1], &msg, 1);
  (void)w;
}

inline auto poll_io_loop::add_io_oper( //
    native_file_desc_t fd, oper_type t, oper_body_base* body) -> void {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  std::scoped_lock lock{in_bottleneck_};
  short event = t == oper_type::write ? POLLOUT : POLLIN;
  in_opers_.emplace_back(fd, event, body);
  const char msg = 1;
  auto w = write(poll_wake_fd_[1], &msg, 1);
  (void)w;
  zone.set_param("fd", static_cast<int64_t>(fd));
}

inline auto poll_io_loop::add_non_io_oper(oper_body_base* body) -> void {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  std::scoped_lock lock{in_bottleneck_};
  in_opers_.emplace_back(-1, 0, body);
  const char msg = 1;
  auto w = write(poll_wake_fd_[1], &msg, 1);
  (void)w;
}

inline auto poll_io_loop::check_in_ops() -> void {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  owned_in_opers_.clear();
  {
    // Quickly steal the items from the input vector, while under the lock
    std::scoped_lock lock{in_bottleneck_};
    owned_in_opers_.swap(in_opers_);
  }
  next_op_to_process_ = owned_in_opers_.begin();
}

inline auto poll_io_loop::handle_one_owned_in_op() -> bool {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  if (next_op_to_process_ != owned_in_opers_.end()) {
    const io_oper& op = *next_op_to_process_;
    next_op_to_process_++;
    if (op.fd_ < 0) {
      // Simply run the non-IO operations; don't care about the result
      op.body_->try_run();
    } else {
      // If the I/O operation did not complete instantly, add it to our lists used
      // when polling
      if (!op.body_->try_run()) {
        poll_data_.push_back(pollfd{op.fd_, op.events_, 0});
        poll_opers_.push_back(op.body_);
      }
    }
    return true;
  }
  return false;
}

inline auto poll_io_loop::check_for_one_io_completion() -> bool {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  char msg{0};
  while (read(poll_wake_fd_[0], &msg, 1) > 0)
    ;

  for (std::size_t i = check_completions_start_idx_; i < poll_data_.size(); i++) {
    pollfd& p = poll_data_[i];
    if ((p.events & p.revents) != 0) {
      oper_body_base* body = poll_opers_[i];
      if (body && body->try_run()) {
        // If the operation is finally complete, remove it from the list
        poll_data_.erase(poll_data_.begin() + i);
        poll_opers_.erase(poll_opers_.begin() + i);
        check_completions_start_idx_ = i;
        return true;
      }
    }
  }
  check_completions_start_idx_ = poll_data_.size();
  return false;
}
inline auto poll_io_loop::do_poll() -> bool {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Clear the output field in poll_data_
  for (pollfd& p : poll_data_)
    p.revents = 0;

  while (true) {
    zone.set_param("num_fds", static_cast<uint64_t>(poll_data_.size()));

    // Perform the poll on all the poll data that we have
    // int rc = poll(poll_data_.data(), poll_data_.size(), -1);
    int rc = poll(poll_data_.data(), poll_data_.size(), 10);

#if PROFILING_ENABLED
    char buf[256];
    char* pbuf = buf;
    std::size_t remaining = 256;
    auto n = std::snprintf(pbuf, remaining, "fds:");
    remaining -= n;
    pbuf += n;
    for (auto p : poll_data_) {
      if (remaining > 0) {
        n = std::snprintf(pbuf, remaining, " %d (%d)", p.fd, int(p.events));
        remaining -= n;
        pbuf += n;
      }
    }
    if (remaining > 0) {
      std::snprintf(pbuf, remaining, " => %d", rc);
    }
    PROFILING_SET_TEXT(buf);
#endif

    if (rc >= 0) {
      // Call to `poll()` succeeded
      check_completions_start_idx_ = 0; // skip notification file
      return true;
    }
    // Failure?
    if (errno != EINVAL)
      return false;
  }
}

struct receiver {
  virtual ~receiver() = default;
  virtual void set_value() = 0;
  virtual void set_error(std::exception_ptr) = 0;
  virtual void set_stopped() = 0;
};

class async_read_oper : oper_body_base {
  native_file_desc_t fd_;
  std::string& output_;
  poll_io_loop* ctx_;
  receiver* recv_;

  auto try_run() noexcept -> bool override {
    concore2full::profiling::zone zone{CURRENT_LOCATION()};

    constexpr size_t buffer_size = 64;
    char read_buffer[buffer_size + 1];
    int bytes_read = read(fd_, read_buffer, buffer_size);
    output_.append(read_buffer, bytes_read);
    zone.set_param("bytes_read", static_cast<int64_t>(bytes_read));

    if (bytes_read == 0) {
      recv_->set_value();
    }

    return bytes_read == 0;
  }
  auto set_stopped() noexcept -> void override { recv_->set_stopped(); }

public:
  async_read_oper(int fd, std::string& output, poll_io_loop* ctx, receiver* r)
      : fd_(fd), output_(output), ctx_(ctx), recv_(r) {}

  void start() noexcept {
    concore2full::profiling::zone zone{CURRENT_LOCATION()};
    try {
      ctx_->add_io_oper(fd_, oper_type::read, this);
    } catch (...) {
      auto err = std::make_error_code(std::errc::operation_not_permitted);
      recv_->set_error(std::make_exception_ptr(std::system_error(err)));
    }
  }
};

std::string async_read(poll_io_loop* io_ctx, int fd) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  struct my_receiver : receiver {
    concore2full::suspend_token token_;
    std::exception_ptr e_;
    bool stopped_{false};

    void set_value() override {
      concore2full::profiling::zone zone{CURRENT_LOCATION()};
      token_.notify();
    }
    void set_error(std::exception_ptr e) override {
      concore2full::profiling::zone zone{CURRENT_LOCATION()};
      e_ = e;
      token_.notify();
    }
    void set_stopped() override {
      concore2full::profiling::zone zone{CURRENT_LOCATION()};
      stopped_ = true;
      token_.notify();
    }
  } r;

  // Start the read operation
  std::string result;
  async_read_oper oper{fd, result, io_ctx, &r};
  oper.start();
  // Suspend, until the receiver tells us that we are done
  concore2full::suspend(r.token_);
  // Check result
  if (r.e_)
    std::rethrow_exception(r.e_);
  if (r.stopped_)
    throw std::runtime_error("operation was stopped");
  return result;
}

std::string sync_read(int fd) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  std::string result;
  constexpr size_t buffer_size = 64;
  char read_buffer[buffer_size + 1];
  while (true) {
    int bytes_read = read(fd, read_buffer, buffer_size);
    if (bytes_read == 0)
      break;
    result.append(read_buffer, bytes_read);
  }
  return result;
}

poll_io_loop* g_io_ctx{nullptr};

std::string async_read_file(const char* filename) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  assert(g_io_ctx);

  // Open the file
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf("Can't open file %s\n", filename);
    throw std::system_error(std::error_code(errno, std::system_category()));
  }

  // Read the content of the file asynchronously.
  std::string result = async_read(g_io_ctx, fd);
  // Close the file.
  close(fd);
  // Return the content of the file.
  return result;
}

// Sometimes sync, sometimes async
std::string read_file(const char* filename) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  assert(g_io_ctx);

  // Open the file
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf("Can't open file %s\n", filename);
    throw std::system_error(std::error_code(errno, std::system_category()));
  }

  std::string result;
  if (rand() % 2 == 0) {
    // Read the content of the file asynchronously.
    result = async_read(g_io_ctx, fd);
  } else {
    // Read the content syncrhonously.
    result = sync_read(fd);
  }

  // Close the file.
  close(fd);
  // Return the content of the file.
  return result;
}

} // namespace async_io

TEST_CASE("I/O read with suspend", "[suspend][suspend_io]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Arrange
  async_io::poll_io_loop io_ctx;
  async_io::g_io_ctx = &io_ctx;
  std::thread io_thread{[&io_ctx] {
    concore2full::profiling::emit_thread_name_and_stack("io_thread");
    io_ctx.run();
  }};

  // Act
  std::string content = async_io::async_read_file("/etc/profile");
  io_ctx.stop();
  async_io::g_io_ctx = nullptr;
  io_thread.join();

  // Assert
  REQUIRE(!content.empty());
}

TEST_CASE("Read file, sometimes sync, sometimes async", "[suspend][suspend_io]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Arrange
  async_io::poll_io_loop io_ctx;
  async_io::g_io_ctx = &io_ctx;
  std::thread io_thread{[&io_ctx] {
    concore2full::profiling::emit_thread_name_and_stack("io_thread");
    io_ctx.run();
  }};

  // Act
  std::string content = async_io::read_file("/etc/profile");
  io_ctx.stop();
  async_io::g_io_ctx = nullptr;
  io_thread.join();

  // Assert
  REQUIRE(!content.empty());
}
