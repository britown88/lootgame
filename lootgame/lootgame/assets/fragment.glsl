#version 140

// uniform float uAlpha;
// uniform vec4 uColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform sampler2D uDiffuse;
uniform bool uColorOnly;
uniform bool uPointLight;
uniform float uLightIntensity;

in vec2 vTexCoords;

out vec4 outputColor;

void main() {
   if(uColorOnly){
      outputColor = vec4(uColor.rgb, uAlpha);
   }
   else if(uPointLight){
      float dist = distance(vTexCoords, vec2(0.5f, 0.5f)) ;
      float r = max(1.0f - (dist/0.5f), 0);
      outputColor = (uColor * uAlpha) * r * uLightIntensity;
   }
   else{
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;
//      vec2 tSize = textureSize(uDiffuse, 0);
//      vec2 pos = tSize * vTexCoords;
//      pos.x -= 1;
//      //pos.y -= 1;
//
//      vec4 avg = vec4(0,0,0,0);
//      
//      //for(int y = 0; y < 3; ++y){
//         for(int x = 0; x < 3; ++x){
//            avg += texture(uDiffuse, pos/tSize);
//            pos.x += 1;
//         }
//         //pos.y += 1;
//      //}
//      outputColor = avg / 3;
   }   
}