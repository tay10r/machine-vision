#version 100

precision highp float;

varying vec2 frag_texcoords;

varying vec3 frag_normal;

uniform sampler2D skybox;

#define PI 3.14159265358979

#define TAU 6.2831853071795864769

vec4 sample_sky(vec3 dir)
{
  vec3 d = vec3(-dir.z, dir.x, dir.y);
  float theta = acos(d.z);
  float phi = atan(d.y, d.x);
  float u = theta / PI;
  float v = 1.0 - ((phi / PI) + 1.0) * 0.5;
  return texture2D(skybox, vec2(v, u));
}

void
main()
{
  vec3 albedo = vec3(0.8, 0.8, 0.8);

  if (frag_texcoords.x == 0.4242242) {
    albedo.x = 0.4;
  }

  gl_FragColor = vec4(albedo * sample_sky(frag_normal).rgb, 1.0);
}
