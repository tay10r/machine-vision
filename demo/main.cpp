#include "mvz.h"

#include <GLFW/glfw3.h>

#include <iostream>

#include <cstdlib>

auto
main() -> int
{
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW." << std::endl;
    return EXIT_FAILURE;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  auto* window = glfwCreateWindow(640, 480, "MVZ Demo", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create window." << std::endl;
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);

  auto exit_code{ EXIT_SUCCESS };

  try {

    mvz::session s(glfwGetProcAddress);

    const int obj_id = s.load_obj(DEMO_PATH "/scene.obj");

    std::vector<mvz::mesh_instance> scene;

    scene.emplace_back(s.instance(obj_id, "Ground"));
    scene.emplace_back(s.instance(obj_id, "Suzanne"));

    mvz::camera cam;

    cam.position.y = 1;
    cam.position.z = 10;
    cam.rotation.y = 0;

    while (!glfwWindowShouldClose(window)) {

      int res[2]{};
      glfwGetFramebufferSize(window, &res[0], &res[1]);

      cam.resolution[0] = res[0];
      cam.resolution[1] = res[1];
      cam.aspect = static_cast<float>(res[0]) / static_cast<float>(res[1]);

      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        break;
      }

      s.render(cam, scene);

      cam.rotation.y += 0.002;

      glfwSwapBuffers(window);

      glfwPollEvents();
    }

  } catch (const mvz::glsl_error& err) {
    std::cerr << err.path() << ": " << err.what() << std::endl;
    std::cerr << "---" << std::endl;
    std::cerr << err.source() << std::endl;
    std::cerr << "---" << std::endl;
    exit_code = EXIT_FAILURE;
  } catch (const mvz::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    exit_code = EXIT_FAILURE;
  }

  glfwDestroyWindow(window);

  glfwTerminate();

  return exit_code;
}
