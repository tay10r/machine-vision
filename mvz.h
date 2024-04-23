#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace mvz {

enum class image_type
{
  color,
  segmentation
};

struct vec3 final
{
  float x;
  float y;
  float z;
};

class runtime_error : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class open_gl_error final : public runtime_error
{
public:
  using runtime_error::runtime_error;
};

class glsl_error final : public runtime_error
{
public:
  glsl_error(std::string what, std::string path, std::string source);

  auto path() const -> std::string;

  auto source() const -> std::string;

private:
  std::string m_path;

  std::string m_source;
};

using gl_func = void (*)(void);

using gl_get_func = gl_func (*)(const char* name);

class session_impl;

struct camera final
{
  vec3 position{};

  vec3 rotation{};

  float aspect{ 1 };

  float fovy{ 0.5 /* approximately 30 degrees */ };

  float near{ 0.1 };

  float far{ 200.0 };

  int resolution[2]{ 640, 480 };
};

struct mesh_instance final
{
  vec3 scale{ 1, 1, 1 };

  vec3 rotation{ 1, 1, 1 };

  vec3 translation{ 1, 1, 1 };

  int obj_id{};

  int shape_index{};
};

class session final
{
public:
  session(gl_get_func getter);

  session(const session&) = delete;

  session(session&&) = delete;

  auto operator=(const session&) -> session& = delete;

  auto operator=(session&&) -> session& = delete;

  ~session();

  auto load_obj(const char* path) -> int;

  auto instance(int obj_id, const char* name) -> mesh_instance;

  void render(const camera& cam, const std::vector<mesh_instance>& mesh_instances);

  void set_development_mode(bool enabled);

protected:
  auto impl() -> session_impl&;

private:
  session_impl* m_impl{ nullptr };
};

} // namespace mvz
