#version 140

// uniform float uAlpha;
// uniform vec4 uColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform sampler2D uDiffuse;
uniform bool uColorOnly;
uniform bool uPointLight;
uniform float uPointLightIntensity;

in vec2 vTexCoords;

out vec4 outputColor;

void main() {
   if(uColorOnly){
      outputColor = vec4(uColor.rgb, uAlpha);
   }
   else if(uPointLight){
      float dist = distance(vTexCoords, vec2(0.5f, 0.5f)) ;
      float r = max(1.0f - (dist/0.5f), 0);
      outputColor = (uColor * uAlpha) * r * uPointLightIntensity;
   }
   else{
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;
   }   
}