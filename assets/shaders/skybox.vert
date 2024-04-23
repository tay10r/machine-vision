#version 100

attribute vec2 position;

uniform mat3 camera_rotation;

varying vec3 ray_direction;

void
main()
{
  vec2 pos = (position * 2.0) - 1.0;
  ray_direction = camera_rotation * normalize(vec3(pos, -1.0));
  gl_Position = vec4(pos, 0.0, 1.0);
}
