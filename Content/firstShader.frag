#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vertexColor;

void main() {
    //outColor = vec4(texture(usampler2D(firstImage, firstSampler), textCoord)) / vec4(255.0f);
    //outColor = vec4(textCoord, 255.0f, 255.0f) / vec4(255.0f);
    outColor = vec4(vertexColor, 1.0f);
}