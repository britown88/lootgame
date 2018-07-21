#version 140

// uniform float uAlpha;
// uniform vec4 uColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform sampler2D uDiffuse;
uniform vec2 uSceneSize;

uniform bool uColorOnly;
uniform bool uPointLight;

uniform vec2 uPointLightPos;
uniform float uPointLightSize;
uniform float uPointLightIntensity;

in vec2 vTexCoords;

out vec4 outputColor;

void main() {
	if(uColorOnly){
		outputColor = vec4(uColor.rgb, uAlpha);
	}
	else if(uPointLight){
		float dist = distance(vTexCoords * uSceneSize, uPointLightPos) ;
		if(dist < uPointLightSize){
			outputColor = texture(uDiffuse, vTexCoords) * vec4(uColor.rgb, uAlpha) * (1.0f - (dist / uPointLightSize)) * uPointLightIntensity;
		}
	}
	else{
		outputColor = texture(uDiffuse, vTexCoords) * vec4(uColor.rgb, uAlpha);
	}   
}