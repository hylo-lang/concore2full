#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

struct document {
  std::string text;
};

document apply_gaussian_blur(document doc) { return document{"gaussian_blur(" + doc.text + ")"}; }

document apply_sharpen(document doc) { return document{"sharpen(" + doc.text + ")"}; }

bool save(document doc) {
  printf("saving document: %s\n", doc.text.c_str());
  return true;
}

TEST_CASE("sketch of implementing a P2300 `split` -- structured concurrency", "[examples]") {
  document initial_doc{"empty"};

  // Start applying gaussian blur to the document.
  auto future1 = concore2full::spawn([=] { return apply_gaussian_blur(initial_doc); });
  // maybe do something else in the meantime
  document doc1 = future1.await();
  // Save this document, and start applying sharpen to the new document.
  auto future2 = concore2full::spawn([=] { save(doc1); });
  document doc2 = apply_sharpen(doc1);
  // Ensure that saving is done.
  future2.await();

  // Save the final document.
  save(doc2);
}

document create_doc() { return document{"initial_state"}; }
using future_create_doc_t = decltype(concore2full::escaping_spawn(create_doc));

auto split_save_sharpen(future_create_doc_t future1) {
  // Do something only after the future is ready.
  document doc1 = future1.await();
  // Save this document, and start applying sharpen to the new document.
  auto future2 = concore2full::spawn([=] { save(doc1); });
  document doc2 = apply_sharpen(doc1);
  // Ensure that saving is done.
  future2.await();
  return doc2;
}

TEST_CASE("sketch of implementing a P2300 `split` -- weekly-structured concurrency", "[examples]") {

  // Get a future that creates a document.
  auto future1 = concore2full::escaping_spawn(create_doc);
  // Save the document resulting from `future1` and apply sharpen.
  auto doc = split_save_sharpen(future1);
  printf("resulting document: %s\n", doc.text.c_str());
}
