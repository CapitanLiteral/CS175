#version 130

uniform float uVertexScale;
uniform float uXCoefficient;
uniform float uYCoefficient;
uniform float uXOffset;
uniform float uYOffset;

in vec2 aPosition;
in vec2 aTexCoord;

out vec2 vTexCoord;
out vec2 vTemp;

void main() {
  gl_Position =
    vec4((aPosition.x + uXOffset) * uVertexScale * uXCoefficient, (aPosition.y + uYOffset) * uYCoefficient, 0, 1);
  vTexCoord = aTexCoord;
  vTemp = vec2(1, 1);
}