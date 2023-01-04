#version 430 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texture_coordinate;

out V_OUT
{
   vec2 texture_coordinate;
} v_out;

void main()
{
    gl_Position = vec4(position,0.0f , 1.0f);
//    gl_Position = vec4(-position.x, -position.y,0.0f , 1.0f);
    v_out.texture_coordinate = texture_coordinate;
}