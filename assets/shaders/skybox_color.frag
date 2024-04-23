#version 100

precision highp float;

uniform samplerCube skybox;

varying vec3 ray_direction;

void
main()
{
  gl_FragColor = textureCube(skybox, ray_direction);
}
