uniform float progress;
uniform int angle;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;

vec4 getFromColor (vec2 uv) {
  return GskTexture(u_texture1, uv);
}

vec4 getToColor (vec2 uv) {
  return GskTexture(u_texture2, uv);
}

// License: MIT
// Author: Qiu Wenbo, based on randomsquares from gre. See dissolve.glsl

const float smoothness = 0.0;
const ivec2 size = ivec2(20, 20);

float rand (vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,33.233))) * 43758.5453);
}

vec4 transition(vec2 uv) {
  float ra = radians(float(angle));
  vec2 direction = vec2(cos(ra), sin(ra));

  vec2 rounded_uv = floor(uv * vec2(size)) / vec2(size);

  vec2 p = rounded_uv - progress * direction;
  float should_speedup = 1.0 -  step(0.0, p.y) *
                                step(p.y, 1.0) *
                                step(0.0, p.x) *
                                step(p.x, 1.0);

  float r = max(0., rand(rounded_uv) - 0.4 * should_speedup);

  float m = smoothstep(0.0, -smoothness, r - (progress * (1.0 + smoothness)));

  return mix(getFromColor(uv), getToColor(uv), m);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}
