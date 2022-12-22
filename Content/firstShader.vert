#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNorm;
layout(location = 2) in vec2 textCoords;

layout(std140, set = 0, binding = 0) readonly buffer camTransform{
    mat4 view;
    mat4 proj;
} cam;

layout(std140, set = 1, binding = 0) readonly buffer worldTransforms{
    mat4 world[];
} transforms[];

vec3 manuelVertColors[] = {
  vec3(1, 0, 0),
  vec3(0, 1, 0),
  vec3(0, 0, 1)
};

layout(location = 0) out vec3 vertColor;
void main() {
    vec4 vecPos = cam.proj * cam.view * transforms[gl_DrawID].world[gl_InstanceIndex] * vec4(vertPos, 1.0f);
    vecPos.y = -vecPos.y;
    gl_Position = vecPos;
    //textCoord = textCoords.xy;
    vertColor = manuelVertColors[gl_VertexIndex % 3];
}