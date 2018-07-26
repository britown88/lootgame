#version 330

// uniform float uAlpha;
// uniform vec4 uColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform float uHeight;
uniform sampler2D uDiffuse;
uniform sampler2D uNormals;

uniform bool uColorOnly;      // disregards diffuse texture and returns uColor * uAlpha
uniform bool uOutlineOnly;    // draw uColor * uAlpha on alpha-edge pixels only
uniform bool uOutputNormals;  // output from uNormals to outputNormal with vTexCoords, using uHeight as alpha
uniform bool uPointLight;     // render light at center of mesh, uses uColor * uAlpha * uLightIntensity
uniform bool uNormalLighting; // lighting checks uNormals for normals map
uniform bool uDiscardAlpha;   // discard alpha<1 fragments

uniform float uLightIntensity;

in vec2 vTexCoords;

layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;


void main() {
   if(uColorOnly){
      outputColor = uColor * uAlpha;
   }
   else if(uOutlineOnly){ 
      vec4 c = texture2D( uDiffuse, vTexCoords );
      vec2 pxSz = 1.0 / textureSize(uDiffuse, 0);
      float a = c.a * 4;
      int w = 1;
      a -= texture2D( uDiffuse, vTexCoords + vec2(pxSz.x*w, 0) ).a;
      a -= texture2D( uDiffuse, vTexCoords + vec2(-pxSz.x*w, 0) ).a;
      a -= texture2D( uDiffuse, vTexCoords + vec2(0, pxSz.y*w) ).a;
      a -= texture2D( uDiffuse, vTexCoords + vec2(0, -pxSz.y*w) ).a;
      vec4 edgeColor =  vec4(1,1,1,1) * clamp(a, 0, 1);

      outputColor = edgeColor * uColor * uAlpha;
   }  
   else if(uPointLight){

      vec2 center = vec2(0.5f, 0.5f);
      float dist = distance(vTexCoords, center);
      float r = max(1.0f - (dist/0.5f), 0);

      if(uNormalLighting){
         // light against the normal map
         

         vec2 normalCoord = gl_FragCoord.xy / textureSize(uNormals, 0);
         vec4 normalData = texture(uNormals, normalCoord);
         vec3 normal = (normalData.rgb * 2.0) - 1.0;
         vec3 lightDir = vec3(center - vTexCoords, uHeight - normalData.a);

         lightDir = normalize(lightDir);
         normal = normalize(normal);

         float d = max(dot(lightDir, normal), 0);

         vec4 diffuse = uColor * uAlpha;
         outputColor =  diffuse * d * r * uLightIntensity;
      }
      else {
         // just a colored point light
         outputColor =  uColor * uAlpha * r * uLightIntensity;
      }      
   }    
   else{
      // normal diffuse texture rendering
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;

      if(uOutputNormals){
         vec4 norm = texture(uNormals, vTexCoords);
         norm.a = uHeight;
         outputNormal = norm;        
      }
   }

   if(uDiscardAlpha){
      if(outputColor.a < 1.0){
         discard;
      }      
   }
}

