#version 430

layout(location = 2) uniform sampler2D diffuse_color;
layout(location = 4) uniform int pass;
layout(location = 10) uniform int pickID;
layout(location = 1) uniform bool edge;


out vec4 fragcolor;  
out vec4 pick_color;           
in vec2 tex_coord;
in vec4 color;
flat in int GLInstanceID;

void main(void)
{   
	if(pass == 1)
	{
		fragcolor = texture(diffuse_color, tex_coord);
		pick_color = vec4(vec3(GLInstanceID/255.0),1.0);

		if (pickID == GLInstanceID) {
			fragcolor = texture(diffuse_color, tex_coord)*color;
		}
			
	} else {  // pass 2

		if (edge) {
			vec4 above = texelFetch(diffuse_color, ivec2(gl_FragCoord.xy)+ivec2(0,1), 0);
			vec4 below = texelFetch(diffuse_color, ivec2(gl_FragCoord.xy)+ivec2(0,-1), 0);
			vec4 left = texelFetch(diffuse_color, ivec2(gl_FragCoord.xy)+ivec2(-1,0), 0);
			vec4 right = texelFetch(diffuse_color, ivec2(gl_FragCoord.xy)+ivec2(1,0), 0); 
			
			vec4 edge_color = ((left-right)*(left-right)) + ((above-below)*(above-below));
			edge_color = sqrt(edge_color);
			fragcolor = vec4(edge_color.rgb,1.0);
		} else {
			fragcolor = texture(diffuse_color, tex_coord);

		}
	
	}
	

}




















