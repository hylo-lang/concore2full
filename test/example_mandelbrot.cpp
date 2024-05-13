#include "concore2full/spawn.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <complex>

using namespace std::chrono_literals;

static constexpr int max_x = 4096;
static constexpr int max_y = 2160;
static constexpr int depth = 1000;

std::complex<double> transform(int x, int y) {
  double x0 = (x - max_x / 2) * 4.0 / max_x;
  double y0 = (y - max_y / 2) * 4.0 / max_x;
  return std::complex<double>(x0, y0);
}

int mandelbrot_core(std::complex<double> c, int depth) {
  int count = 0;
  std::complex<double> z = 0;

  for (int i = 0; i < depth; i++) {
    if (abs(z) >= 2.0)
      break;
    z = z * z + c;
    count++;
  }

  return count;
}

void serial_mandelbrot(int* vals, int max_x, int max_y, int depth) {
  for (int y = 0; y < max_y; y++) {
    for (int x = 0; x < max_x; x++) {
      vals[y * max_x + x] = mandelbrot_core(transform(x, y), depth);
    }
  }
}

void concurrent_mandelbrot(int* vals, int max_x, int max_y, int depth) {
  concore2full::bulk_spawn(max_y, [=](int y) {
    for (int x = 0; x < max_x; x++) {
      vals[y * max_x + x] = mandelbrot_core(transform(x, y), depth);
    }
  }).await();
}

TEST_CASE("mandelbrot example", "[benchmark]") {
  std::vector<int> vals(max_x * max_y, 0);

  auto now = std::chrono::high_resolution_clock::now();
  concurrent_mandelbrot(vals.data(), max_x, max_y, depth);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - now);

  printf("Took %d ms\n", int(duration.count()));
}

TEST_CASE("mandelbrot example (serial)", "[benchmark]") {
  std::vector<int> vals(max_x * max_y, 0);

  auto now = std::chrono::high_resolution_clock::now();
  serial_mandelbrot(vals.data(), max_x, max_y, depth);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - now);

  printf("Took %d ms\n", int(duration.count()));
}
