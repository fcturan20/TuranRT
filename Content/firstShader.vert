#version 450
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNorm;
layout(location = 2) in vec2 textCoords;


layout(std140, set = 0, binding = 0) readonly buffer transform{
    mat4 view;
    mat4 proj;
};

vec3 manuelVertColors[] = {
vec3(1, 0, 0),
vec3(0, 1, 0),
vec3(0, 0, 1)
};

layout(location = 0) out vec3 vertColor;
layout(location = 1) out vec2 textCoord;
layout(location = 2) out vec4 vecPos;

void main() {
    //gl_Position = vec4((positions[gl_VertexIndex] + translate) / screenRatio_divisor, 0.0, 1.0);
    vecPos = proj * view * vec4(vertPos, 1.0f);
    vecPos.y = -vecPos.y;
    gl_Position = vecPos;
    textCoord = textCoords.xy;
    vertColor = manuelVertColors[gl_VertexIndex % 3];
}