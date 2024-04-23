#include "mvz_obj.h"

#include <algorithm>
#include <iterator>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace mvz {

namespace {

class mesh_builder final
{
public:
  using material_id = int;

  void add(const material_id mat, const float* pos, const float* uv, const float* norm)
  {
    auto& m = get_or_create(mat);

    const auto prev_size = m.vertices.size();

    m.vertices.resize(prev_size + 8);

    m.vertices[prev_size + 0] = pos[0];
    m.vertices[prev_size + 1] = pos[1];
    m.vertices[prev_size + 2] = pos[2];

    m.vertices[prev_size + 3] = uv[0];
    m.vertices[prev_size + 4] = uv[1];

    m.vertices[prev_size + 5] = norm[0];
    m.vertices[prev_size + 6] = norm[1];
    m.vertices[prev_size + 7] = norm[2];

    m.num_vertices++;
  }

  auto take_meshes() -> std::vector<obj_mesh>
  {
    std::vector<obj_mesh> output;

    for (auto& entry : m_meshes) {
      output.emplace_back(std::move(entry.second));
    }

    return output;
  }

protected:
  auto get_or_create(const material_id mat) -> obj_mesh&
  {
    auto it = m_meshes.find(mat);
    if (it == m_meshes.end()) {
      it = m_meshes.emplace(mat, obj_mesh{ mat, {} }).first;
    }
    return it->second;
  }

private:
  std::map<material_id, obj_mesh> m_meshes;
};

} // namespace

auto
obj_file::load(const char* path) -> bool
{
  tinyobj::ObjReaderConfig reader_config;

  reader_config.triangulate = true;

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path, reader_config)) {
    return false;
  }

  const auto& attrib = reader.GetAttrib();

  const auto& materials = reader.GetMaterials();

  const auto& input_shapes = reader.GetShapes();

  constexpr auto num_vertices_per_face{ 3 };

  for (std::size_t i = 0; i < input_shapes.size(); i++) {

    const auto& input_shape = input_shapes.at(i);

    const auto& input_mesh = input_shape.mesh;

    mesh_builder builder;

    for (std::size_t j = 0; j < input_mesh.indices.size(); j++) {

      const auto idx = input_mesh.indices.at(j);

      const auto v_idx = idx.vertex_index;
      const auto t_idx = idx.texcoord_index;
      const auto n_idx = idx.normal_index;

      const auto mat_id = input_mesh.material_ids.at(j / 3);
      const auto* pos = &attrib.vertices.at(v_idx * 3);
      const auto* uv = &attrib.texcoords.at(t_idx * 2);
      const auto* nrm = &attrib.normals.at(n_idx * 3);

      builder.add(mat_id, pos, uv, nrm);
    }

    obj_shape shape;
    shape.name = input_shape.name;
    shape.meshes = builder.take_meshes();

    shapes.emplace_back(std::move(shape));
  }

  auto cmp_shape_name = [](const obj_shape& a, const obj_shape& b) -> bool { return a.name < a.name; };

  std::sort(shapes.begin(), shapes.end(), cmp_shape_name);

  return true;
}

auto
obj_file::find_shape(const char* name) const -> int
{
  auto cmp = [](const obj_shape& shp, const char* n) -> bool { return shp.name < n; };

  auto it = std::lower_bound(shapes.begin(), shapes.end(), name, cmp);
  if ((it == shapes.end()) || (it->name != name)) {
    return -1;
  }

  return std::distance(shapes.begin(), it);
}

} // namespace mvz
