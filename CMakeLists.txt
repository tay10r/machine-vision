cmake_minimum_required(VERSION 3.14.7)

project(machine_vision)

include(CMakeRC.cmake)

option(MVZ_DEMO "Whether or not to build the demo." ON)

find_package(glm CONFIG REQUIRED)

cmrc_add_resource_library(mvz_assets
  assets/skyboxes/DaySkyHDRI017B/nx.png
  assets/skyboxes/DaySkyHDRI017B/ny.png
  assets/skyboxes/DaySkyHDRI017B/nz.png
  assets/skyboxes/DaySkyHDRI017B/px.png
  assets/skyboxes/DaySkyHDRI017B/py.png
  assets/skyboxes/DaySkyHDRI017B/pz.png
  assets/shaders/skybox.vert
  assets/shaders/skybox_color.frag
  assets/shaders/mesh.vert
  assets/shaders/mesh_color.frag)

add_library(mvz
  mvz.h
  mvz.cpp
  mvz_stb.h
  mvz_stb.cpp
  mvz_obj.h
  mvz_obj.cpp
  deps/tiny_obj_loader.h
  deps/stb_image.h
  deps/stb_image_write.h
  deps/glad/include/KHR/khrplatform.h
  deps/glad/include/glad/glad.h
  deps/glad/src/glad.c)
target_link_libraries(mvz
  PUBLIC
    mvz_assets
    glm::glm)
target_compile_definitions(mvz
  PRIVATE
    MVZ_BUILD=1)
target_include_directories(mvz
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/deps>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include>)

if(MVZ_DEMO)
  find_package(glfw3 REQUIRED)
  add_executable(demo demo/main.cpp)
  target_link_libraries(demo PUBLIC mvz glfw)
  target_compile_definitions(demo
    PUBLIC
      "DEMO_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/demo\"")
endif()
