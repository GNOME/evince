uniform float progress;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;

vec4 getFromColor (vec2 uv) {
  return GskTexture(u_texture1, uv);
}

vec4 getToColor (vec2 uv) {
  return GskTexture(u_texture2, uv);
}

// Source: https://gl-transitions.com/editor/randomsquares
// License: MIT
// Author: gre

const ivec2 size = ivec2(20, 20);
const float smoothness = 0.2;

float rand (vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec4 transition(vec2 uv) {
  float r = rand(floor(vec2(size) * uv));
  float m = smoothstep(0.0, -smoothness, r - (progress * (1.0 + smoothness)));
  return mix(getFromColor(uv), getToColor(uv), m);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}
