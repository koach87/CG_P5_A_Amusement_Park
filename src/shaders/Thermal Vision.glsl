#version 430 core
out vec4 f_color;

in V_OUT
{ 
   vec2 texture_coordinate;
} f_in;

uniform sampler2D u_texture;

void main() 
{ 
  vec2 uv = f_in.texture_coordinate.xy;
  
  vec3 tc = vec3(1.0, 0.0, 0.0);

    vec3 pixcol = texture2D(u_texture, uv).rgb;
    vec3 colors[3];
    colors[0] = vec3(0.,0.,1.);
    colors[1] = vec3(1.,1.,0.);
    colors[2] = vec3(1.,0.,0.);
    float lum = (pixcol.r+pixcol.g+pixcol.b)/3.;
    int ix = (lum < 0.5)? 0:1;
    tc = mix(colors[ix],colors[ix+1],(lum-float(ix)*0.5)/0.5);
  
	f_color = vec4(tc, 1.0);
}