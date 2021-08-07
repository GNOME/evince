uniform float progress;
uniform int angle;
uniform float scale;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;

vec4 getFromColor (vec2 uv) {
  return GskTexture(u_texture1, uv);
}

vec4 getToColor (vec2 uv) {
  return GskTexture(u_texture2, uv);
}

// License: MIT
// Author: Qiu Wenbo
const vec2 center = vec2(0.5, 0.5);

float r = radians(float(angle));
vec2 direction = vec2(cos(r), sin(r));
float size = (scale - 1.) * progress + 1.;

vec2 box_coordinate(vec2 origin)
{
  return (origin - center) * size + center + direction * (1. + scale) / 2. * progress;
}

vec4 transition (vec2 uv) {
  vec2 f = (uv - (1. + scale) / 2. * progress * direction - center) / size + center;

  vec2 top_left = box_coordinate(vec2(0, 1));
  vec2 bottom_right = box_coordinate(vec2(1, 0));

  float region = step(top_left.x, uv.x) * step(uv.x, bottom_right.x) *
                 step(uv.y, top_left.y) * step(bottom_right.y, uv.y);

  return mix(getToColor(uv), getFromColor(f), region);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}
