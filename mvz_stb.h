#pragma once

#ifndef MVZ_BUILD
#error "This header is not meant to be included outside of the build."
#endif

#include <memory>

namespace mvz {

struct image_deconstructor final
{
  void operator()(unsigned char* ptr);
};

using unique_image_ptr = std::unique_ptr<unsigned char, image_deconstructor>;

auto
open_rc_image(const char* path, int* w, int* h) -> unique_image_ptr;

} // namespace mvz
