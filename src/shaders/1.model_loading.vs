#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 model;

layout(std140, binding = 0) uniform commom_matrices
{
    mat4 u_projection;
    mat4 u_view;
};


void main()
{
    TexCoords = aTexCoords;    
    gl_Position = u_projection * u_view * model * vec4(aPos, 1.0);
}