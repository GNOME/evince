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
// Author: Qiu Wenbo

vec4 transition (vec2 uv) {
  float r = radians(float(angle));
  vec2 direction = vec2(cos(r), sin(r));
  vec2 p = uv - progress * sign(direction);
  vec2 f = fract(p);

  return mix(
    getToColor(f),
    getFromColor(uv),
    step(0.0, p.y) * step(p.y, 1.0) * step(0.0, p.x) * step(p.x, 1.0)
  );
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}
