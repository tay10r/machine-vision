#include "mvz.h"

#include "mvz_obj.h"
#include "mvz_stb.h"

#include <glad/glad.h>

#include <cmrc/cmrc.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <map>
#include <sstream>
#include <string>

#include <cstddef>

CMRC_DECLARE(mvz_assets);

namespace mvz {

namespace {

auto
format_gl_error(const char* err, const char* code, const int line) -> std::string
{
  std::ostringstream stream;
  stream << __FILE__ << ':' << line << ": '" << code << "' -> " << err;
  return stream.str();
}

void
throw_gl_error(const GLenum err, const int line, const char* code)
{
  switch (err) {
    case GL_NO_ERROR:
      break;
    case GL_INVALID_VALUE:
      throw open_gl_error(format_gl_error("GL_INVALID_VALUE", code, line));
    case GL_INVALID_ENUM:
      throw open_gl_error(format_gl_error("GL_INVALID_ENUM", code, line));
    case GL_INVALID_OPERATION:
      throw open_gl_error(format_gl_error("GL_INVALID_OPERATION", code, line));
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      throw open_gl_error(format_gl_error("GL_INVALID_FRAMEBUFFER_OPERATION", code, line));
    default:
      throw open_gl_error(format_gl_error("(UNKNOWN_OPENGL_ERROR)", code, line));
  }
}

} // namespace

#define CHECK_GL(stmt)                                                                                                 \
  do {                                                                                                                 \
    stmt;                                                                                                              \
    const GLenum err = glGetError();                                                                                   \
    if (err != GL_NO_ERROR) {                                                                                          \
      throw_gl_error(err, __LINE__, #stmt);                                                                            \
    }                                                                                                                  \
  } while (0)

#define CHECK_GL_EXPR(assignment_var, func, ...)                                                                       \
  do {                                                                                                                 \
    assignment_var = func(__VA_ARGS__);                                                                                \
    const GLenum err = glGetError();                                                                                   \
    if (err != GL_NO_ERROR) {                                                                                          \
      throw_gl_error(err, __LINE__, #func);                                                                            \
    }                                                                                                                  \
  } while (0)

namespace {

constexpr GLint color_texture_index{ 0 };

constexpr GLint segmentation_texture_index{ 1 };

constexpr GLint skybox_texture_index{ 2 };

constexpr GLint diffuse_irradiance_texture_index{ 3 };

constexpr GLint specular_irradiance_texture_index{ 4 };

auto
create_texture(GLenum active_texture) -> GLuint
{
  GLuint texture{};

  CHECK_GL(glGenTextures(1, &texture));

  CHECK_GL(glActiveTexture(active_texture));

  CHECK_GL(glBindTexture(GL_TEXTURE_2D, texture));

  CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

  CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  return texture;
}

auto
create_cubemap(GLenum active_texture) -> GLuint
{
  GLuint texture{};

  CHECK_GL(glGenTextures(1, &texture));

  CHECK_GL(glActiveTexture(GL_TEXTURE0 + skybox_texture_index));

  CHECK_GL(glBindTexture(GL_TEXTURE_CUBE_MAP, texture));

  CHECK_GL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  CHECK_GL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

  CHECK_GL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  CHECK_GL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  return texture;
}

class gl_object
{
public:
  gl_object() = default;

  gl_object(const gl_object&) = delete;

  gl_object(gl_object&&) = delete;

  auto operator=(const gl_object&) -> gl_object& = delete;

  auto operator=(gl_object&&) -> gl_object& = delete;

  virtual ~gl_object()
  {
    if (m_initialized) {
      cleanup();
    }
  }

  void init()
  {
    init_impl();
    m_initialized = true;
  }

  void cleanup()
  {
    if (m_initialized) {
      cleanup_impl();
      m_initialized = false;
    }
  }

protected:
  virtual void init_impl() = 0;

  virtual void cleanup_impl() = 0;

private:
  bool m_initialized{ false };
};

template<GLenum texture_target, bool HasDepth>
class framebuffer final : public gl_object
{
public:
  framebuffer(const GLint w, const GLint h)
    : m_width(w)
    , m_height(h)
  {
  }

protected:
  void init_impl()
  {
    CHECK_GL(glGenTextures(1, &m_texture));

    if (HasDepth) {
      CHECK_GL(glGenRenderbuffers(1, &m_renderbuffer));
    }

    CHECK_GL(glGenFramebuffers(1, &m_framebuffer));
  }

  void cleanup_impl()
  {
    CHECK_GL(glDeleteFramebuffers(1, &m_framebuffer));
    CHECK_GL(glDeleteTextures(1, &m_texture));
    if (HasDepth) {
      CHECK_GL(glDeleteRenderbuffers(1, &m_renderbuffer));
    }
  }

private:
  GLuint m_framebuffer{};

  GLuint m_renderbuffer{};

  GLuint m_texture{};

  GLint m_width{};

  GLint m_height{};
};

class shader final
{
public:
  shader() = default;

  shader(const shader&) = delete;

  void init(GLenum type, const char* path)
  {
    const auto fs = cmrc::mvz_assets::get_filesystem();

    const auto file = fs.open(path);

    const char* source_ptr = file.begin();

    const GLint source_len = static_cast<GLint>(file.size());

    CHECK_GL_EXPR(m_id, glCreateShader, type);

    try {
      CHECK_GL(glShaderSource(m_id, 1, &source_ptr, &source_len));
      CHECK_GL(glCompileShader(m_id));
      GLint compile_status{ GL_FALSE };
      CHECK_GL(glGetShaderiv(m_id, GL_COMPILE_STATUS, &compile_status));
      if (compile_status == GL_TRUE) {
        return;
      }
    } catch (...) {
      glDeleteShader(m_id);
      throw;
    }

    GLint log_length{};

    try {
      CHECK_GL(glGetShaderiv(m_id, GL_INFO_LOG_LENGTH, &log_length));
    } catch (...) {
      glDeleteShader(m_id);
      throw;
    }

    if (log_length < 0) {
      glDeleteShader(m_id);
      std::ostringstream stream;
      stream << "Shader log length of '" << log_length << "' is invalid.";
      throw runtime_error(stream.str());
    }

    std::string log;

    log.resize(static_cast<size_t>(log_length));

    GLsizei read_size{};

    try {
      CHECK_GL(glGetShaderInfoLog(m_id, log_length, &read_size, &log[0]));
    } catch (...) {
      glDeleteShader(m_id);
      throw;
    }

    if (read_size < 0) {
      glDeleteShader(m_id);
      std::ostringstream stream;
      stream << "Shader log read size of '" << read_size << "' is invalid.";
      throw runtime_error(stream.str());
    }

    log.resize(static_cast<size_t>(read_size));

    CHECK_GL(glDeleteShader(m_id));

    throw glsl_error(std::move(log), path, std::string(file.begin(), file.size()));
  }

  void cleanup() { glDeleteShader(m_id); }

  auto id() -> GLuint { return m_id; }

private:
  GLuint m_id{};
};

class program final
{
public:
  void init(GLuint vert_shader, GLuint frag_shader)
  {
    CHECK_GL_EXPR(m_id, glCreateProgram);

    try {
      CHECK_GL(glAttachShader(m_id, vert_shader));
      CHECK_GL(glAttachShader(m_id, frag_shader));
      CHECK_GL(glLinkProgram(m_id));
      CHECK_GL(glDetachShader(m_id, vert_shader));
      CHECK_GL(glDetachShader(m_id, frag_shader));
    } catch (...) {
      glDeleteProgram(m_id);
      throw;
    }

    GLint link_status{};

    try {
      CHECK_GL(glGetProgramiv(m_id, GL_LINK_STATUS, &link_status));
    } catch (...) {
      glDeleteProgram(m_id);
      throw;
    }

    if (link_status == GL_TRUE) {
      return;
    }

    GLint log_length{};

    try {
      CHECK_GL(glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &log_length));
    } catch (...) {
      glDeleteProgram(m_id);
      throw;
    }

    if (log_length < 0) {
      glDeleteProgram(m_id);
      std::ostringstream stream;
      stream << "Invalid log length '" << log_length << "' for shader program.";
      throw runtime_error(stream.str());
    }

    std::string info_log;

    info_log.resize(static_cast<std::size_t>(log_length));

    GLsizei read_size{};

    try {
      CHECK_GL(glGetProgramInfoLog(m_id, log_length, &read_size, &info_log[0]));
    } catch (...) {
      glDeleteProgram(m_id);
      throw;
    }

    if (read_size < 0) {
      glDeleteProgram(m_id);
      std::ostringstream stream;
      stream << "Invalid log read size '" << read_size << "' for shader program.";
      throw runtime_error(stream.str());
    }

    CHECK_GL(glDeleteProgram(m_id));

    info_log.resize(static_cast<std::size_t>(read_size));

    throw open_gl_error(info_log);
  }

  void cleanup() { glDeleteProgram(m_id); }

  void use() { CHECK_GL(glUseProgram(m_id)); }

  auto get_uniform_location(const char* name) -> GLint { return glGetUniformLocation(m_id, name); }

  auto get_attribute_location(const char* name) -> GLint { return glGetAttribLocation(m_id, name); }

private:
  GLuint m_id{};
};

struct gl_obj_shape final
{
  std::vector<GLuint> meshes;

  std::vector<int> num_vertices;
};

struct gl_obj_file final
{
  std::vector<gl_obj_shape> shapes;
};

} // namespace

//====================//
// Irradiance Mapping //
//====================//

namespace {

class irradiance_integrator final
{
public:
private:
  framebuffer<GL_TEXTURE0 + diffuse_irradiance_texture_index, false> m_diffuse_framebuffer;

  framebuffer<GL_TEXTURE0 + specular_irradiance_texture_index, false> m_specular_framebuffer;
};

struct render_target final
{
  framebuffer<GL_TEXTURE0 + color_texture_index, true> color;

  framebuffer<GL_TEXTURE0 + segmentation_texture_index, true> segmentation;

  render_target(GLint w, GLint h)
    : color(w, h)
    , segmentation(w, h)
  {
  }

  void init()
  {
    color.init();
    segmentation.init();
  }
};

} // namespace

//============//
// Public API //
//============//

class session_impl final
{
public:
  session_impl()
  {
    m_color_framebuffer.init();

    try {
      m_segmentation_framebuffer.init();
    } catch (...) {
      m_color_framebuffer.cleanup();
      throw;
    }

    try {
      create_skybox_texture();
    } catch (...) {
      m_segmentation_framebuffer.cleanup();
      m_color_framebuffer.cleanup();
      throw;
    }

    try {
      create_screen_quad();
    } catch (...) {
      m_segmentation_framebuffer.cleanup();
      m_color_framebuffer.cleanup();
      glDeleteTextures(1, &m_skybox_texture);
      throw;
    }

    try {
      create_shaders();
    } catch (...) {
      glDeleteTextures(1, &m_skybox_texture);
      glDeleteBuffers(1, &m_screen_quad);
      m_segmentation_framebuffer.cleanup();
      m_color_framebuffer.cleanup();
      throw;
    }
  }

  ~session_impl()
  {
    m_color_framebuffer.cleanup();
    m_segmentation_framebuffer.cleanup();
    glDeleteTextures(1, &m_skybox_texture);
    glDeleteBuffers(1, &m_screen_quad);
    m_skybox_color_program.cleanup();
    m_mesh_color_program.cleanup();
  }

  void render_current_fbo(const camera& cam, const std::vector<mesh_instance>& instances)
  {
    CHECK_GL(glEnable(GL_DEPTH_TEST));

    CHECK_GL(glViewport(0, 0, cam.resolution[0], cam.resolution[1]));

    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    render_skybox(cam);

    CHECK_GL(glClear(GL_DEPTH_BUFFER_BIT));

    m_mesh_color_program.use();

    const auto pos_loc = m_mesh_color_program.get_attribute_location("position");
    const auto texcoords_loc = m_mesh_color_program.get_attribute_location("texcoord");
    const auto normal_loc = m_mesh_color_program.get_attribute_location("normal");

    CHECK_GL(glEnableVertexAttribArray(pos_loc));
    CHECK_GL(glEnableVertexAttribArray(texcoords_loc));
    CHECK_GL(glEnableVertexAttribArray(normal_loc));

    CHECK_GL(glActiveTexture(GL_TEXTURE0 + skybox_texture_index));
    CHECK_GL(glBindTexture(GL_TEXTURE_CUBE_MAP, m_skybox_texture));
    CHECK_GL(glUniform1i(m_mesh_color_program.get_uniform_location("skybox"), skybox_texture_index));

    constexpr auto stride{ sizeof(float) * 8 };

    auto ptr_offset = [](std::size_t i) -> void* { return reinterpret_cast<void*>(i); };

    const auto proj = glm::perspective(cam.fovy, cam.aspect, cam.near, cam.far);

    const auto cam_pos = glm::vec3(cam.position.x, cam.position.y, cam.position.z);
    const auto cam_rot = glm::mat3(get_rotation_matrix(cam));

    const auto view = glm::lookAt(cam_pos, cam_pos + cam_rot * glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

    const auto mvp_loc = m_mesh_color_program.get_uniform_location("mvp");

    for (const auto& inst : instances) {

      continue;

      const glm::mat4 model_transform(1.0) /* TODO */;

      const glm::mat4 mvp = proj * view * model_transform;

      glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

      const auto& file = m_gl_obj_files.at(inst.obj_id);

      const auto& shp = file.shapes.at(inst.shape_index);

      for (std::size_t i = 0; i < shp.meshes.size(); i++) {

        const auto& m = shp.meshes.at(i);

        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, m));

        CHECK_GL(glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, stride, ptr_offset(0)));
        CHECK_GL(glVertexAttribPointer(texcoords_loc, 2, GL_FLOAT, GL_FALSE, stride, ptr_offset(sizeof(float) * 3)));
        CHECK_GL(glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, stride, ptr_offset(sizeof(float) * 5)));

        CHECK_GL(glDrawArrays(GL_TRIANGLES, 0, shp.num_vertices.at(i)));
      }
    }

    CHECK_GL(glDisableVertexAttribArray(pos_loc));
    CHECK_GL(glDisableVertexAttribArray(texcoords_loc));
    CHECK_GL(glDisableVertexAttribArray(normal_loc));
  }

  auto load_obj(const char* path) -> int
  {
    obj_file file;

    if (!file.load(path)) {
      std::ostringstream stream;
      stream << "Failed to load OBJ file '" << path << "'.";
      throw runtime_error(stream.str());
    }

    auto gl_file = create_gl_obj_file(file);

    const auto id = m_next_obj_id++;

    m_obj_files.emplace(id, std::move(file));

    m_obj_paths.emplace(id, path);

    m_gl_obj_files.emplace(id, std::move(gl_file));

    return id;
  }

  auto instance(const int obj_id, const char* shape) -> mesh_instance
  {
    const auto& file = m_obj_files.at(obj_id);

    const auto shape_idx = file.find_shape(shape);
    if (shape_idx < 0) {
      std::ostringstream stream;
      stream << "Failed to find shape '" << shape << "' in OBJ '" << m_obj_paths.at(obj_id) << "'.";
      throw runtime_error(stream.str());
    }

    mesh_instance instance;
    instance.obj_id = obj_id;
    instance.shape_index = static_cast<std::size_t>(shape_idx);
    return instance;
  }

  void set_development_mode(const bool state) { m_development_mode = state; }

protected:
  static auto get_rotation_matrix(const camera& cam) -> glm::mat4
  {
    const auto x_rot = glm::rotate(glm::mat4(1.0), cam.rotation.x, glm::vec3(1, 0, 0));
    const auto y_rot = glm::rotate(glm::mat4(1.0), cam.rotation.y, glm::vec3(0, 1, 0));
    const auto z_rot = glm::rotate(glm::mat4(1.0), cam.rotation.z, glm::vec3(0, 0, 1));
    return z_rot * y_rot * x_rot;
  }

  void render_skybox(const camera& cam)
  {
    CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, m_screen_quad));

    m_skybox_color_program.use();

    const auto pos_loc = m_skybox_color_program.get_attribute_location("position");
    const auto rot_loc = m_skybox_color_program.get_uniform_location("camera_rotation");
    const auto sky_loc = m_skybox_color_program.get_uniform_location("skybox");

    CHECK_GL(glActiveTexture(GL_TEXTURE0 + skybox_texture_index));

    CHECK_GL(glBindTexture(GL_TEXTURE_CUBE_MAP, m_skybox_texture));

    CHECK_GL(glUniform1i(sky_loc, skybox_texture_index));

    const glm::mat3 rotation = get_rotation_matrix(cam);

    CHECK_GL(glUniformMatrix3fv(rot_loc, 1, GL_FALSE, glm::value_ptr(rotation)));

    CHECK_GL(glEnableVertexAttribArray(pos_loc));

    CHECK_GL(glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0)));

    CHECK_GL(glDrawArrays(GL_TRIANGLES, 0, 6));

    glDisableVertexAttribArray(pos_loc);
  }

  void open_internal_skybox(const char* prefix)
  {
    const std::array<std::pair<GLenum, const char*>, 6> entries{ { { GL_TEXTURE_CUBE_MAP_POSITIVE_X, "/px.png" },
                                                                   { GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "/nx.png" },
                                                                   { GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "/py.png" },
                                                                   { GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "/ny.png" },
                                                                   { GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "/pz.png" },
                                                                   { GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "/nz.png" } } };

    glActiveTexture(GL_TEXTURE0 + skybox_texture_index);

    CHECK_GL(glBindTexture(GL_TEXTURE_CUBE_MAP, m_skybox_texture));

    for (const auto& entry : entries) {

      const std::string path = std::string(prefix) + entry.second;

      const GLenum target = entry.first;

      int w{};
      int h{};

      const auto data = open_rc_image(path.c_str(), &w, &h);
      if (!data) {
        std::ostringstream stream;
        stream << "Failed to open internal skybox '" << path << "'.";
        throw runtime_error(stream.str());
      }

      CHECK_GL(glTexImage2D(target, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.get()));
    }
  }

  auto create_gl_obj_file(const obj_file& f) -> gl_obj_file
  {
    gl_obj_file file;

    const auto num_shapes = f.shapes.size();

    file.shapes.resize(num_shapes);

    for (std::size_t i = 0; i < num_shapes; i++) {

      auto& shp = file.shapes.at(i);

      const auto num_meshes = f.shapes[i].meshes.size();

      shp.meshes.resize(num_meshes);

      shp.num_vertices.resize(num_meshes);

      for (std::size_t j = 0; j < num_meshes; j++) {

        try {
          CHECK_GL(glGenBuffers(1, &shp.meshes.at(j)));
        } catch (...) {
          if (j > 0) {
            glDeleteBuffers(j - 1, shp.meshes.data());
          }
        }

        auto id = shp.meshes.at(j);

        shp.num_vertices[j] = f.shapes.at(i).meshes.at(j).num_vertices;

        const auto& data = f.shapes.at(i).meshes.at(j).vertices;

        try {
          CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, id));
          CHECK_GL(glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(data[0]), data.data(), GL_STATIC_DRAW));
        } catch (...) {
          glDeleteBuffers(j, shp.meshes.data());
        }
      }
    }

    return file;
  }

  // initialization routines

  void create_mesh_shaders()
  {
    shader mesh_vert_shader;
    mesh_vert_shader.init(GL_VERTEX_SHADER, "assets/shaders/mesh.vert");

    shader mesh_color_frag_shader;

    try {
      mesh_color_frag_shader.init(GL_FRAGMENT_SHADER, "assets/shaders/mesh_color.frag");
    } catch (...) {
      mesh_vert_shader.cleanup();
      throw;
    }

    try {
      m_mesh_color_program.init(mesh_vert_shader.id(), mesh_color_frag_shader.id());
    } catch (...) {
      mesh_vert_shader.cleanup();
      mesh_color_frag_shader.cleanup();
    }
  }

  void create_skybox_texture()
  {
    m_skybox_texture = create_cubemap(GL_TEXTURE0 + skybox_texture_index);

    try {
      open_internal_skybox("assets/skyboxes/DaySkyHDRI017B");
    } catch (const std::exception&) {
      glDeleteTextures(1, &m_skybox_texture);
      throw;
    }
  }

  void create_skybox_shaders()
  {
    shader skybox_vert_shader;
    skybox_vert_shader.init(GL_VERTEX_SHADER, "assets/shaders/skybox.vert");

    shader skybox_color_frag_shader;
    try {
      skybox_color_frag_shader.init(GL_FRAGMENT_SHADER, "assets/shaders/skybox_color.frag");
    } catch (...) {
      skybox_vert_shader.cleanup();
      throw;
    }

    try {
      m_skybox_color_program.init(skybox_vert_shader.id(), skybox_color_frag_shader.id());
    } catch (...) {
      skybox_color_frag_shader.cleanup();
      skybox_vert_shader.cleanup();
    }

    skybox_vert_shader.cleanup();
    skybox_color_frag_shader.cleanup();
  }

  void create_shaders()
  {
    create_skybox_shaders();

    try {
      create_mesh_shaders();
    } catch (...) {
      m_skybox_color_program.cleanup();
      throw;
      // TODO : add the segmentation fragment shader as well
    }
  }

  void create_screen_quad()
  {
    CHECK_GL(glGenBuffers(1, &m_screen_quad));

    glBindBuffer(GL_ARRAY_BUFFER, m_screen_quad);

    const std::array<float, 12> data{
      // clang-format off
      0, 0,
      0, 1,
      1, 1,
      1, 1,
      1, 0,
      0, 0
      // clang-format on
    };

    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(data[0]), data.data(), GL_STATIC_DRAW);
  }

private:
  framebuffer<GL_TEXTURE0 + color_texture_index, true> m_color_framebuffer;

  framebuffer<GL_TEXTURE0 + segmentation_texture_index, true> m_segmentation_framebuffer;

  program m_skybox_color_program;

  program m_mesh_color_program;

  GLuint m_skybox_texture{};

  GLuint m_screen_quad{};

  std::map<int, obj_file> m_obj_files;

  std::map<int, std::string> m_obj_paths;

  std::map<int, gl_obj_file> m_gl_obj_files;

  int m_next_obj_id{};

  bool m_development_mode{ false };
};

session::session(gl_get_func func)
{
  gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(func));

  m_impl = new session_impl();
}

session::~session()
{
  delete m_impl;
}

void
session::set_development_mode(const bool enabled)
{
  m_impl->set_development_mode(enabled);
}

auto
session::load_obj(const char* path) -> int
{
  return m_impl->load_obj(path);
}

auto
session::instance(int obj_id, const char* shape) -> mesh_instance
{
  return m_impl->instance(obj_id, shape);
}

void
session::render(const camera& cam, const std::vector<mesh_instance>& instances)
{
  m_impl->render_current_fbo(cam, instances);
}

glsl_error::glsl_error(std::string what, std::string path, std::string source)
  : runtime_error(std::move(what))
  , m_path(std::move(path))
  , m_source(std::move(source))
{
}

auto
glsl_error::path() const -> std::string
{
  return m_path;
}

auto
glsl_error::source() const -> std::string
{
  return m_source;
}

} // namespace mvz
