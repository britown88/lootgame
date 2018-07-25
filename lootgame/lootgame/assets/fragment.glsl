#version 140

// uniform float uAlpha;
// uniform vec4 uColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform sampler2D uDiffuse;
uniform sampler2D uNormals;
uniform bool uColorOnly;
uniform bool uOutlineOnly;
uniform bool uPointLight;
uniform float uLightIntensity;

in vec2 vTexCoords;

out vec4 outputColor;

vec4 outline(sampler2D tex, vec2 uv )
{
    // simply return the color at the current coordinates:
    vec4 c = texture2D( tex, uv );
    vec2 pxSz = 1.0 / textureSize(tex, 0);
    float a = c.a * 4;
    int w = 1;
    a -= texture2D( tex, uv + vec2(pxSz.x*w, 0) ).a;
    a -= texture2D( tex, uv + vec2(-pxSz.x*w, 0) ).a;
    a -= texture2D( tex, uv + vec2(0, pxSz.y*w) ).a;
    a -= texture2D( tex, uv + vec2(0, -pxSz.y*w) ).a;
    return vec4(1,1,1,1) * clamp(a, 0, 1);
}

void main() {
   if(uColorOnly){
      outputColor = vec4(uColor.rgb, uAlpha);
  }
   else if(uPointLight){
      vec2 center = vec2(0.5f, 0.5f);
      float dist = distance(vTexCoords, center);
      float r = max(1.0f - (dist/0.5f), 0);

      vec3 lightDir = vec3(center - vTexCoords, 0.2);

      vec2 normalCoord = gl_FragCoord.xy / textureSize(uNormals, 0);
      vec3 normal = (texture(uNormals, normalCoord).rgb * 2.0) - 1.0;

      lightDir = normalize(lightDir);
      normal = normalize(normal);

      float d = max(dot(lightDir, normal), 0);

      vec4 diffuse = uColor * uAlpha;

      outputColor =  (0.5 * r) + (diffuse * d * r);
   }
   else if(uOutlineOnly){      

      outputColor = outline(uDiffuse, vTexCoords) * uColor * uAlpha;
   }   
   else{
      outputColor = texture(uDiffuse, vTexCoords) * uColor * uAlpha;
   }
}

