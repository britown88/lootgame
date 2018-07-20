#version 140

// uniform float uAlpha;
// uniform vec4 uColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform sampler2D uDiffuse;

uniform bool uColorOnly;

in vec2 vTexCoords;

out vec4 outputColor;

void main() {
	if(uColorOnly){
		outputColor = vec4(uColor.rgb, uAlpha);
	}
	else{
		outputColor = texture(uDiffuse, vTexCoords) * vec4(uColor.rgb, uAlpha);
	}
   
}