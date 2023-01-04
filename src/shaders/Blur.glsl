#version 430 core
out vec4 f_color;

in V_OUT
{ 
   vec2 texture_coordinate;
} f_in;

uniform sampler2D u_texture;

float offset[3] = float[]( 0.0, 1.3846153846, 3.2307692308 );
float weight[3] = float[]( 0.2270270270, 0.3162162162, 0.0702702703 );

void main()
{   
    vec3 color = vec3(texture(u_texture, f_in.texture_coordinate));
    float gray = color.r * 0.3 + color.g * 0.6 + color.b*0.1;
    f_color = vec4(vec3(gray), 1.0f);

    vec3 tc = vec3(1.0, 0.0, 0.0);
    
    vec2 uv = f_in.texture_coordinate;
    tc = texture2D(u_texture, uv).rgb * weight[0];
    for (int i=1; i<3; i++) 
    {
    tc += texture2D(u_texture, uv + vec2(0.0, offset[i])/300.0).rgb \
            * weight[i];
    tc += texture2D(u_texture, uv - vec2(0.0, offset[i])/300.0).rgb \
            * weight[i];
    }
    
    f_color = vec4(tc, 1.0);
}