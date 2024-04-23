#include "mvz_stb.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(mvz_assets);

namespace mvz {

void
image_deconstructor::operator()(unsigned char* ptr)
{
  stbi_image_free(ptr);
}

auto
open_rc_image(const char* path, int* w, int* h) -> unique_image_ptr
{
  const auto fs = cmrc::mvz_assets::get_filesystem();

  const auto file = fs.open(path);

  auto* px = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(file.begin()), file.size(), w, h, nullptr, 4);

  return unique_image_ptr(px);
}

} // namespace mvz
