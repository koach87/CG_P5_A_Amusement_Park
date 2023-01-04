#version 430 core
out vec4 f_color;

in V_OUT
{ 
   vec2 texture_coordinate;
} f_in;

uniform sampler2D u_texture;

void main()
{   
    vec3 color = vec3(texture(u_texture, f_in.texture_coordinate));
    float gray = color.r * 0.3 + color.g * 0.6 + color.b*0.1;
    f_color = vec4(vec3(gray), 1.0f);
}