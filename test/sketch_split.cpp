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
  // maybe do something elese in the meantime
  document doc1 = future1.await();
  // Save this document, and start applying sharpen to the new document.
  auto future2 = concore2full::spawn([=] { save(doc1); });
  document doc2 = apply_sharpen(doc1);
  // Ensure that saving is done.
  future2.await();

  // Save the final document.
  save(doc2);
}
