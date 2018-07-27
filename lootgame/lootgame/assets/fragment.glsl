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

uniform vec3 uLightFalloff;  // quadratic attenuation (x:const, y:lin, z:quad)
uniform float uPointLightSize;

in vec2 vTexCoords;

layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

float Falloff( in float _fDistance, in float _fRadius ) {
	//float fFalloff = max( 1.0 / sqrt( _fDistance ) - 1.0 / sqrt( _fRadius ), 0.0 );
   float fFalloff = clamp(1.0 - _fDistance*_fDistance/(_fRadius*_fRadius), 0.0, 1.0); 
   fFalloff *= fFalloff;
	return fFalloff;
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

      float sz = uPointLightSize;
      float radius = sz / 2.0f;
      float dist = (distance(vTexCoords, center) / 0.5f) * radius;

      float attenuation = 1.0 / (uLightFalloff.x + (uLightFalloff.y * dist) + (uLightFalloff.z * dist * dist));
      float r = Falloff(dist, radius);



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
         outputColor =  diffuse * attenuation * r;
      }
      else {
         // just a colored point light
         outputColor =  uColor * uAlpha * attenuation * r;
      }      
   }    
   else{
      // normal diffuse texture rendering
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;

      if(uOutputNormals){
         vec4 norm = texture(uNormals, vTexCoords);

         if(uTransformNormals){
            mat3 normalMatrix = mat3(uNormalTransform);
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

