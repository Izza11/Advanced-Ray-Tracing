#version 430

layout(location = 3) uniform mat4 PVM;
layout(location = 7) uniform bool inst_render;
layout(location = 4) uniform int pass; 

layout(location = 0) in vec3 pos_attrib;
in vec2 tex_coord_attrib;
in vec3 normal_attrib;
in int gl_InstanceID;

layout(location = 9) in mat4 trans_matrix;
layout(location = 8) in vec4 colorbuffer;

layout(location = 5) uniform mat4 Tmatrix;
layout(location = 6) uniform vec4 colorB;

out vec2 tex_coord;  
out vec4 color;
flat out int GLInstanceID;

void main(void)
{
	if(pass == 1) //render mesh to texture
	{
		if (inst_render) {
		gl_Position = trans_matrix * PVM * vec4(pos_attrib, 1.0);
		color = colorbuffer;
		
		} else {
			gl_Position = Tmatrix * PVM * vec4(pos_attrib, 1.0);
			color = colorB;
		}
		GLInstanceID = gl_InstanceID;
		tex_coord = tex_coord_attrib;
		
	} else if(pass == 2) // render textured quad to back buffer
	{
		//full screen quad is in clip coords, so no matrix mult needed when assigning to gl_Position
		gl_Position = vec4(pos_attrib, 1.0);

		//quad doesn't have tex coords, but we can generate them since quad goes from -1.0 to +1.0 in each direction
		tex_coord = 0.5*pos_attrib.xy + vec2(0.5);
	}
	   
   
}



