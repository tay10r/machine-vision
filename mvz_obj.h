#pragma once

#ifndef MVZ_BUILD
#error "This header is not meant to be included outside of the build."
#endif

#include <glad/glad.h>

#include <string>
#include <vector>

namespace mvz {

struct obj_material final
{
  float kd{ 1 };
};

struct obj_mesh final
{
  int material_index{ -1 };

  std::vector<float> vertices;

  int num_vertices{};

  auto has_material() const -> bool { return material_index >= 0; }
};

struct obj_shape final
{
  std::string name;

  std::vector<obj_mesh> meshes;
};

struct obj_file final
{
  std::vector<obj_shape> shapes;

  auto load(const char* path) -> bool;

  auto find_shape(const char* name) const -> int;
};

} // namespace mvz
