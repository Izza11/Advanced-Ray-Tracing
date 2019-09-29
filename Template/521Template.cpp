#include <windows.h>

#include <GL/glew.h>

#include <GL/freeglut.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

#include "InitShader.h"
#include "LoadMesh.h"
#include "LoadTexture.h"
#include "imgui_impl_glut.h"
#include "VideoMux.h"
#include <vector>
#include "Quad.h"

//names of the shader files to load
static const std::string vertex_shader("template_vs.glsl");
static const std::string fragment_shader("template_fs.glsl");

GLuint shader_program = -1;
GLuint texture_id = -1; //Texture map for fish
GLuint InstColor = -1;
GLuint InstMatrix = -1;
std::vector<glm::vec3> matrix_buffer;
std::vector<glm::vec3> color_buffer;
glm::mat4 Umatrix;
glm::vec4 Ucolorbuffer;
int pickID = -1;

int INSTANCE_COUNT = 9;


GLuint timer_query;
GLuint nanoseconds;

GLuint fbo = -1;
GLuint fbo_texture = -1;
GLuint pick_texture = -1;
GLuint fbo_texture_width = 1280;
GLuint fbo_texture_height = 720;
const GLenum CBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

//Full screen quad in a VAO
GLuint quad_vao = -1;

//names of the mesh and texture files to load
static const std::string mesh_name = "Amago0.obj";
static const std::string texture_name = "AmagoT.bmp";

MeshData mesh_data;
float time_sec = 0.0f;
float angle = 0.0f;
bool recording = false;
bool instancedR = true;
bool edgeD = false;

void reload_shader();

void DrawMesh();

//Draw the user interface using ImGui
void draw_gui()
{
   ImGui_ImplGlut_NewFrame();
   
   if (ImGui::Button("Reload Shader"))
   {
	   reload_shader();
   }

   // Checkbox to enable/disable instanced rendering
   ImGui::Checkbox("Instanced Rendering", &instancedR);

   // Checkbox to enable/disable edge detection
   unsigned int edge_loc = 1;
   if (ImGui::Checkbox("Edge Detection", &edgeD)) {
	   glUniform1i(edge_loc, edgeD);  
   }

   const int filename_len = 256;
   static char video_filename[filename_len] = "capture.mp4";

   ImGui::InputText("Video filename", video_filename, filename_len);
   ImGui::SameLine();
   if (recording == false)
   {
      if (ImGui::Button("Start Recording"))
      {
         const int w = glutGet(GLUT_WINDOW_WIDTH);
         const int h = glutGet(GLUT_WINDOW_HEIGHT);
         recording = true;
         start_encoding(video_filename, w, h); //Uses ffmpeg
      }
      
   }
   else
   {
      if (ImGui::Button("Stop Recording"))
      {
         recording = false;
         finish_encoding(); //Uses ffmpeg
      }
   }

   //create a slider to change the angle variables
   ImGui::SliderFloat("View angle", &angle, -3.141592f, +3.141592f);

   ImGui::Image((void*)texture_id, ImVec2(128,128));
   ImGui::SameLine();
   ImGui::Image((void*)fbo_texture, ImVec2(128.0f, 128.0f), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
   ImGui::SameLine();
   ImGui::Image((void*)pick_texture, ImVec2(128.0f, 128.0f), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));


   static bool show = false;

   

  //  ImGui::ShowTestWindow();
   ImGui::Render();
 }

// glut display callback function.
// This function gets called every time the scene gets redisplayed 
void display()
{

	glUseProgram(shader_program);

	const int w = glutGet(GLUT_WINDOW_WIDTH);
	const int h = glutGet(GLUT_WINDOW_HEIGHT);
	const float aspect_ratio = float(w) / float(h);

	//Set uop some uniform variables
	glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f))*glm::scale(glm::vec3(mesh_data.mScaleFactor));
	glm::mat4 V = glm::lookAt(glm::vec3(0.0f, 1.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 P = glm::perspective(3.141592f / 4.0f, aspect_ratio, 0.1f, 100.0f);

	//we are using layout qualifiers in the shader
	const int PVM_loc = 3;
	const int pass_loc = 4;
	const int tex_loc = 2;

	glm::mat4 PVM = P*V*M;
	glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));

	///////////////////////////////////////////////////
	// Begin pass 1: render scene to texture.
	///////////////////////////////////////////////////
	//Set pass uniform variable.
	glUniform1i(pass_loc, 1);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Render to FBO.
	glDrawBuffers(2, CBuffers);

	//Bind texture and set sampler uniform so we can read the texture in the shader
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glUniform1i(tex_loc, 0); // we bound our texture to texture unit 0

	//Make the viewport match the FBO texture size.
	glViewport(0, 0, fbo_texture_width, fbo_texture_height);
	//Clear the FBO attached texture.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Draw the fishes
	DrawMesh();

	///////////////////////////////////////////////////
	// Begin pass 2: render textured quad to screen
	///////////////////////////////////////////////////
	glUniform1i(pass_loc, 2);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Do not render the next pass to FBO.
	glDrawBuffer(GL_BACK); // Render to back buffer.

	//Bind texture and set sampler uniform so we can read the texture in the shader
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glUniform1i(tex_loc, 0); // we bound our texture to texture unit 0

	//Change viewport back to full screen
	//glViewport(0, 0, fbo_texture_width, fbo_texture_height); //Render to the full window
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Clear the back buffer

	//draw the quad to the screen.
	draw_quad_vao(quad_vao);

	draw_gui();

	if (recording == true)
	{
		glFinish();

		glReadBuffer(GL_BACK);
		read_frame_to_encode(&rgb, &pixels, w, h);
		encode_frame(rgb);
	}

	glutSwapBuffers();
	///////////////////////////////////////////////////////////////////

}

// glut idle callback.
//This function gets called between frames
void idle()
{
	glutPostRedisplay();

   const int time_ms = glutGet(GLUT_ELAPSED_TIME);
   time_sec = 0.001f*time_ms;
   const GLint time_loc = 2;
   //int time_loc = glGetUniformLocation(shader_program, "time");
   if (time_loc != -1)
   {
	   glUniform1f(time_loc, time_sec); // we bound our texture to texture unit 0
   }
   
}

void reload_shader()
{
   GLuint new_shader = InitShader(vertex_shader.c_str(), fragment_shader.c_str());

   if(new_shader == -1) // loading failed
   {
      glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
   }
   else
   {
      glClearColor(0.35f, 0.35f, 0.35f, 0.0f);

      if(shader_program != -1)
      {
         glDeleteProgram(shader_program);
      }
      shader_program = new_shader;

      if(mesh_data.mVao != -1)
      {
         BufferIndexedVerts(mesh_data);
      }
   }
}

// Display info about the OpenGL implementation provided by the graphics driver.
// Your version should be > 4.0 for CGT 521 
void printGlInfo()
{
   std::cout << "Vendor: "       << glGetString(GL_VENDOR)                    << std::endl;
   std::cout << "Renderer: "     << glGetString(GL_RENDERER)                  << std::endl;
   std::cout << "Version: "      << glGetString(GL_VERSION)                   << std::endl;
   std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)  << std::endl;
}

void initOpenGl()
{
   //Initialize glew so that new OpenGL function names can be used
   glewInit();

   glGenQueries(1, &timer_query);

   glEnable(GL_DEPTH_TEST);
   //glDepthFunc(GL_EQUAL);

   reload_shader();

   //Load a mesh and a texture
   mesh_data = LoadMesh(mesh_name); //Helper function: Uses Open Asset Import library.
   texture_id = LoadTexture(texture_name.c_str()); //Helper function: Uses FreeImage library
   
   ///////////////////////////////// INIT FISH MESH //////////////////////////////
   glBindVertexArray(mesh_data.mVao);

   unsigned int InstMatrix_loc = 9;
   unsigned int InstColor_loc = 8;

   std::vector<glm::mat4> matrices(INSTANCE_COUNT);
   std::vector<glm::vec4> colorbuffer(INSTANCE_COUNT);
   float y_pos = -0.5f;
   for (int i = 0; i < INSTANCE_COUNT; i++)
   {
	   //GLint m_viewport[4];
	   //glGetIntegerv(GL_VIEWPORT, m_viewport);

	   float a = (float)(INSTANCE_COUNT - i) * 0.1f;
	   float b = (float)(i) / (float)(INSTANCE_COUNT);
	   float c = (float)(INSTANCE_COUNT - i) * 0.01f;

	   int x_pos = (0 + i) % 3;
	   float x_p = ((float)(x_pos)-1.0) / 1.5;
	   if (i != 0 && (i % 3) == 0) {
		   y_pos = y_pos + 0.5;
	   }

	   matrices[i] = glm::translate(glm::vec3(x_p, y_pos, 0.0f));
	   colorbuffer[i] = glm::vec4(a, b, c, 1.0f);
   }

   // Translation Matrix
   glGenBuffers(1, &InstMatrix);
   glBindBuffer(GL_ARRAY_BUFFER, InstMatrix);
   glBufferData(GL_ARRAY_BUFFER, matrices.size() * sizeof(glm::mat4), &matrices[0], GL_STATIC_DRAW);

   for (int i = 0; i < 4; i++) {     // assign locations to matrix

	   glVertexAttribPointer(InstMatrix_loc + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)(sizeof(glm::vec4) * i));
	   glEnableVertexAttribArray(InstMatrix_loc + i);
	   glVertexAttribDivisor(InstMatrix_loc + i, 1);
   }

   // Colorbuffer 
   glGenBuffers(1, &InstColor);
   glBindBuffer(GL_ARRAY_BUFFER, InstColor);
   glBufferData(GL_ARRAY_BUFFER, colorbuffer.size() * sizeof(glm::vec4), &colorbuffer[0], GL_STATIC_DRAW);

   glVertexAttribPointer(InstColor_loc, 4, GL_FLOAT, GL_FALSE, 0, NULL);
   glEnableVertexAttribArray(InstColor_loc);
   glVertexAttribDivisor(InstColor_loc, 1);

   glBindVertexArray(0);

   ///////////////////////////////// INIT FISH MESH ENDED /////////////////////////////

	//Load the quadrilateral into a vao/vbo
   quad_vao = create_quad_vao();

   //Create a texture object and set initial wrapping and filtering state
   glGenTextures(1, &fbo_texture);
   glBindTexture(GL_TEXTURE_2D, fbo_texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbo_texture_width, fbo_texture_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D, 0);

   glGenTextures(1, &pick_texture);
   glBindTexture(GL_TEXTURE_2D, pick_texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_texture_width, fbo_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D, 1);
   
   GLuint renderbuffer;
   glGenRenderbuffers(1, &renderbuffer);
   glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fbo_texture_width, fbo_texture_height);

   //Create the framebuffer object
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
   //attach the texture we just created to color attachment 1
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pick_texture, 0);

   //unbind the fbo
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

  
}

// glut callbacks need to send keyboard and mouse events to imgui
void keyboard(unsigned char key, int x, int y)
{
   ImGui_ImplGlut_KeyCallback(key);
   std::cout << "key : " << key << ", x: " << x << ", y: " << y << std::endl;

   switch(key)
   {
      case 'r':
      case 'R':
         reload_shader();     
      break;
   }
}

void keyboard_up(unsigned char key, int x, int y)
{
   ImGui_ImplGlut_KeyUpCallback(key);
}

void special_up(int key, int x, int y)
{
   ImGui_ImplGlut_SpecialUpCallback(key);
}

void passive(int x, int y)
{
   ImGui_ImplGlut_PassiveMouseMotionCallback(x,y);
}

void special(int key, int x, int y)
{
   ImGui_ImplGlut_SpecialCallback(key);
}

void motion(int x, int y)
{
   ImGui_ImplGlut_MouseMotionCallback(x, y);
}

void mouse(int button, int state, int x, int y)
{
   ImGui_ImplGlut_MouseButtonCallback(button, state);
   GLubyte buffer[4];
   unsigned int pickID_loc = 10;

   if ((button == GLUT_LEFT_BUTTON) && (state == GLUT_DOWN)) {

	   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	   glReadBuffer(GL_COLOR_ATTACHMENT1);
	   glPixelStorei(GL_PACK_ALIGNMENT, 1);
	   glReadPixels(x, fbo_texture_height - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	   glBindFramebuffer(GL_FRAMEBUFFER, 0);
	   pickID = static_cast<int>(buffer[0]);

	   glUniform1i(pickID_loc, pickID);
   }
   std::cout << pickID << std::endl;
   
}


int main (int argc, char **argv)
{
   //Configure initial window state using freeglut
   glutInit(&argc, argv); 
   glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
   glutInitWindowPosition (5, 5);
   glutInitWindowSize (fbo_texture_width, fbo_texture_height);
   int win = glutCreateWindow ("CGT 521 Template");

   printGlInfo();

   //Register callback functions with glut. 
   glutDisplayFunc(display); 
   glutKeyboardFunc(keyboard);
   glutSpecialFunc(special);
   glutKeyboardUpFunc(keyboard_up);
   glutSpecialUpFunc(special_up);
   glutMouseFunc(mouse);
   glutMotionFunc(motion);
   glutPassiveMotionFunc(motion);

   glutIdleFunc(idle);

   initOpenGl();
   ImGui_ImplGlut_Init(); // initialize the imgui system

   //Enter the glut event loop.
   glutMainLoop();
   glutDestroyWindow(win);
   return 0;		
}

void DrawMesh() {

	unsigned int Umatrix_loc = 5;
	unsigned int Ucolorbuffer_loc = 6;
	unsigned int instR_loc = 7;

	if (instR_loc != -1)
	{
		glUniform1i(instR_loc, instancedR);
	}

	glBindVertexArray(mesh_data.mVao);

	if (instancedR) {
		glDrawElementsInstanced(GL_TRIANGLES, mesh_data.mNumIndices, GL_UNSIGNED_INT, 0, INSTANCE_COUNT);
		
	}
	else {
		//glBeginQuery(GL_TIME_ELAPSED, timer_query);
		float y_pos = -0.5f;

		for (int i = 0; i < INSTANCE_COUNT; i++)
		{
			float a = (float)(INSTANCE_COUNT - i) * 0.1f;
			float b = (float)(i) / (float)(INSTANCE_COUNT);
			float c = (float)(INSTANCE_COUNT - i) * 0.01f;

			int x_pos = (0 + i) % 3;
			float x_p = ((float)(x_pos)-1.0) / 1.5;
			if (i != 0 && (i % 3) == 0) {
				y_pos = y_pos + 0.5;
			}

			Umatrix = glm::translate(glm::vec3(x_p, y_pos, 0.0f));
			Ucolorbuffer = glm::vec4(a, b, c, 1.0f);
			if (Umatrix_loc != -1)
			{
				glUniformMatrix4fv(Umatrix_loc, 1, false, glm::value_ptr(Umatrix)); // we bound our texture to texture unit 0
			}
			if (Ucolorbuffer_loc != -1)
			{
				glUniform4f(Ucolorbuffer_loc, Ucolorbuffer.x, Ucolorbuffer.y, Ucolorbuffer.z, Ucolorbuffer.w); // we bound our texture to texture unit 0
			}

			glDrawElements(GL_TRIANGLES, mesh_data.mNumIndices, GL_UNSIGNED_INT, 0);
			//glEndQuery(GL_TIME_ELAPSED);
			//glGetQueryObjectuiv(timer_query, GL_QUERY_RESULT, &nanoseconds);

		}

		//std::cout << "Time Elapsed is " << nanoseconds << std::endl;

	}


}