#version 140

in vec2 aPos;
in vec2 aTex;

uniform mat4 uTexMatrix;
uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;

out vec2 vTexCoords;

void main() {

   vTexCoords = ( uTexMatrix * vec4(aTex, 0.0, 1.0)).xy;
   gl_Position = uViewMatrix * (uModelMatrix * vec4(aPos, 0.0, 1.0));
}