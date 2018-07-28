#version 330

uniform mat4 uNormalTransform;
uniform vec4 uColor;
uniform float uAlpha;
uniform float uHeight;
uniform sampler2D uDiffuse;
uniform sampler2D uNormals;

uniform bool uTransformNormals;
uniform bool uColorOnly;      // disregards diffuse texture and returns uColor * uAlpha
uniform bool uOutlineOnly;    // draw uColor * uAlpha on alpha-edge pixels only
uniform bool uOutputNormals;  // output from uNormals to outputNormal with vTexCoords, using uHeight as alpha
uniform bool uPointLight;     // render light at center of mesh, uses uColor * uAlpha * uLightIntensity
uniform bool uNormalLighting; // lighting checks uNormals for normals map
uniform bool uDiscardAlpha;   // discard alpha<1 fragments

uniform vec3 uLightAttrs;  // x: linearPortion, y: smoothing, z: intensity
uniform float uPointLightRadius;

in vec2 vTexCoords;

layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

float Falloff( in float _fDistance, in float _fRadius ) {
	//float fFalloff = max( 1.0 / sqrt( _fDistance ) - 1.0 / sqrt( _fRadius ), 0.0 );
   float fFalloff = clamp(1.0 - _fDistance*_fDistance/(_fRadius*_fRadius), 0.0, 1.0); 
   fFalloff *= fFalloff;
	return fFalloff;
}

float gaussianLight(float radius, float dist, float linearPortion, float smoothingFactor){
   float r = radius;
   float t = max(0, 1 - dist/r);   

   float a = linearPortion;
   float b = 1 - a;

   float n = smoothingFactor;
   float k = 10;
   float S = exp(-k * (pow(1-t, n)));

   return a*t + b*S;
}

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

      float radius = uPointLightRadius;
      float linPortion = uLightAttrs.x;
      float smoothing = uLightAttrs.y;
      float intensity = uLightAttrs.z;

      float dist = (distance(vTexCoords, center) / 0.5f) * radius;

      float attenuation = gaussianLight(radius, dist, linPortion, smoothing) * intensity;

      if(uNormalLighting){
         // light against the normal map

         vec2 normalCoord = gl_FragCoord.xy / textureSize(uNormals, 0);
         vec4 normalData = texture(uNormals, normalCoord);
         vec3 normal = (normalData.rgb * 2.0) - 1.0;

         vec3 lightDir = vec3(center - vTexCoords, uHeight - normalData.a);

         lightDir = normalize(lightDir);
         normal = normalize(normal);

         float light = max(dot(lightDir, normal), 0);

         vec4 diffuse = uColor * uAlpha * light;
         outputColor =  diffuse * attenuation;
      }
      else {
         // just a colored point light
         outputColor =  uColor * attenuation;
      }      
   }    
   else{
      // normal diffuse texture rendering
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;

      if(uOutputNormals){
         vec4 norm = texture(uNormals, vTexCoords);

         if(uTransformNormals){
            mat3 normalMatrix = mat3(uNormalTransform);
            //normalMatrix = inverse(transpose(normalMatrix));
            vec3 v = norm.rgb * 2 - 1;
            v = normalize(v * normalMatrix);
            norm.rgb = (v + 1) / 2;            
         }

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

