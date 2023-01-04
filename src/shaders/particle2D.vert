#version 430 core
// layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoords;

out vec2 TexCoords;
out vec4 ParticleColor;

uniform mat4 projection;
uniform mat4 view;
uniform vec2 offset;
uniform vec4 color;

void main()
{
    float scale = 0.1f;
    // TexCoords = vertex.zw;
    TexCoords = texCoords;
    ParticleColor = color;
    // gl_Position = projection * vec4((vertex.xy * scale) + offset, 0.0, 1.0);
    gl_Position = vec4((position * scale) + offset, 0.0, 1.0);
}