#version 100

attribute in vec3 position;

attribute in vec2 texcoord;

attribute in vec3 normal;

uniform mat4 mvp;

varying vec2 frag_texcoords;

varying vec3 frag_normal;

void
main()
{
  frag_texcoords = texcoord;
  /* TODO transform normal */
  frag_normal = normal;
  gl_Position = mvp * vec4(position, 1.0);
}
