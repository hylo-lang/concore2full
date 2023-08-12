#include "concore2full/concore2full.h"
#include <iostream>

void concore2full() {
#ifdef NDEBUG
  std::cout << "concore2full/0.1.0: Hello World Release!" << std::endl;
#else
  std::cout << "concore2full/0.1.0: Hello World Debug!" << std::endl;
#endif
}
