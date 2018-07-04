#version 140

// uniform float uAlpha;
// uniform vec4 uColor;

uniform sampler2D uDiffuse;
in vec2 vTexCoords;

out vec4 outputColor;

void main() {
   outputColor = texture(uDiffuse, vTexCoords);
}