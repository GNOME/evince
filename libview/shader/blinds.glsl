uniform float progress;
uniform int alignment;  // 0 for horizontal, 1 for vertical
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

const int count = 10;

vec4 transition (vec2 uv) {
  float s = sign(float(alignment));
  float axis = mix(uv.y, uv.x, s);

  return mix(getToColor(uv), getFromColor(uv),
                step(progress, fract(((2. * s - 1.)) * count * axis)));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}
