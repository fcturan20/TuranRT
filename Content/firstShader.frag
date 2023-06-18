#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vertexColor;
layout(location = 1) in vec2 textCoord;

layout(binding = 0) uniform utexture2D DIFFUSETEXTURES[10]; 
layout(binding = 1) uniform sampler DIFFUSESAMPLERS[10]; 

vec2 load_TEXTCOORD0(){return textCoord;}

vec3 checkerboardSample(vec2 uv)
{
  float checkSize = 2;
  float fmodResult = mod(floor(checkSize * uv.x) + floor(checkSize * uv.y), 2.0);
  float fin = max(sign(fmodResult), 0.0);
  return vec3(fin, fin, fin);
}

vec4 loadTexture(utexture2D tex, sampler s, vec2 textCoord){
  return texture(usampler2D(tex, s), textCoord);
}

void main() {
    //outColor = vec4(texture(usampler2D(firstImage, firstSampler), textCoord)) / vec4(255.0f);
    //outColor = vec4(textCoord, 255.0f, 255.0f) / vec4(255.0f);
    //outColor = vec4(checkerboardSample(textCoord), 1.0f);
}