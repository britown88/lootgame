#version 330

uniform vec4 uColor;
uniform float uAlpha;
uniform sampler2D uDiffuse;

uniform bool uColorOnly;      // disregards diffuse texture and returns uColor * uAlpha

in vec2 vTexCoords;

layout(location = 0) out vec4 outputColor;

void main() {
   if(uColorOnly){
      outputColor = uColor * uAlpha;
   }   
   else{
      // normal diffuse texture rendering
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;
   }
}

