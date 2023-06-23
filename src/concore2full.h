#pragma once

#ifdef WIN32
  #define concore2full_EXPORT __declspec(dllexport)
#else
  #define concore2full_EXPORT
#endif

concore2full_EXPORT void concore2full();
