#include "stdafx.h"
#include "GL/glew.h"
#include "GL/freeglut.h"
#include "common.h"
#include <cstdio>
#include "helpers.h"

std::string vssource, fssource;

std::string readFile(std::string filename) {
	std::ifstream fin(filename);
	char buffer[1024];
	std::vector<char> result;
	while (fin) {
		fin.read(buffer, sizeof(buffer));
		if (fin.gcount() == 0) break;

		int prevSize = result.size();
		result.resize(result.size() + fin.gcount());
		for (int i = 0; i < fin.gcount(); i++)
			result[i + prevSize] = buffer[i];
	}
	result.resize(result.size() + 1);
	result[result.size() - 1] = '\0';
	return &result[0];
}

uint prog;

class DepthBuffer {
public:
	GLuint m_name;
	GLuint m_tex;
	glm::ivec2 m_texSize = glm::ivec2(1024, 1024);

	void init() {
		// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
		m_name = 0;
		glGenFramebuffers(1, &m_name);
		glBindFramebuffer(GL_FRAMEBUFFER, m_name);

		// Depth texture. Slower than a depth buffer, but you can sample it later in your shader
		GLuint depthTexture;

		glGenTextures(1, &depthTexture);
		m_tex = depthTexture;
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void bind() {
		glBindFramebuffer(GL_FRAMEBUFFER, m_name);
		glDrawBuffer(GL_NONE); // No color buffer is drawn to.

							   // Always check that our framebuffer is ok
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			printf("Failed setting up depth FBO!\n");
	}

	void unbind() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDrawBuffer(GL_FRONT);
	}

} depthBuffer;

class DirectionalLight {
public:
	Vec3 pos;
	Vec3 lookAt;
	Vec3 up;
	Vec3 shadowMapWorldSize;
};

class ShaderProgram {
public:
	std::string vsSource;
	std::string fsSource;
	GLuint vs = -1, fs = -1, prog = -1;
	bool success = false;

	void compile() {
		if (vs != -1) {
			glDeleteShader(vs);
			vs = -1;
			glDeleteShader(fs);
			fs = -1;
			glDeleteProgram(prog);
			prog = -1;
		}

		vs = glCreateShader(GL_VERTEX_SHADER);
		fs = glCreateShader(GL_FRAGMENT_SHADER);

		success = true;
		if (!loadAndCompileShader(vs, vsSource)) success = false;
		if (!loadAndCompileShader(fs, fsSource)) success = false;

		prog = glCreateProgram();
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);
	}

	void bind() {
		if (success) glUseProgram(prog);
	}

	void unbind() {
		if (success) glUseProgram(0);
	}

	template<typename T>
	void setUniform(std::string const& name, T val) {
		if (!success) return;
		GLint loc = glGetUniformLocation(prog, name.c_str());
		if (loc == -1) return;
		glUniform(loc, val);
	}

private:
	bool loadAndCompileShader(GLuint name, std::string const& source) {
		if (source.length() > 0) {
			const char *src = &source[0];
			int len = source.length();
			glShaderSource(name, 1, &src, &len);
			glCompileShader(name);

			int params = GL_FALSE;
			glGetShaderiv(name, GL_COMPILE_STATUS, &params);
			if (params != GL_TRUE) {
				printf("Unable to compile shader!\n");
				char buffer[1024];
				int len;
				glGetShaderInfoLog(name, 1024, &len, buffer);
				printf("Error: %s\n", buffer);
				return false;
			}
			return true;
		}
	}

	void glUniform(GLuint name, glm::vec3 val) { glUniform3f(name, val.x, val.y, val.z); }
	void glUniform(GLuint name, glm::vec4 val) { glUniform4f(name, val.x, val.y, val.z, val.w); }
	void glUniform(GLuint name, glm::mat4 val) { glUniformMatrix4fv(name, 1, GL_FALSE, &val[0][0]); }
};

class Scene01 {
public:
	DirectionalLight light;
	ShaderProgram prog, sprog, tprog;
	DepthBuffer depbuf;

	void init() {
		light.pos = Vec3(-0.5, -0.5, 1);
		light.lookAt = Vec3(0, 0, 0);
		light.up = Vec3(0, 0, 1);
		light.shadowMapWorldSize = Vec3(1, 1, 2);

		// Material shader
		prog.vsSource = R"(
				uniform mat4 worldToLight;
				uniform vec3 lightDir;
				uniform vec3 eyePos;
				uniform sampler2D tex;

				varying vec4 position;
				varying vec4 normal;
				varying vec3 posInLightSpace;

				void main() {
					gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
					normal = gl_ModelViewMatrix * vec4(gl_Normal, 0);
					position = gl_ModelViewMatrix * gl_Vertex;
					posInLightSpace = (worldToLight * position).xyz;
				}
			)";
		//prog.fsSource = R"(
		//	uniform vec3 lightDir;
		//	uniform vec3 eyePos;
		//	uniform sampler2D tex;
		//	varying vec4 position;
		//	varying vec4 normal;
		//	varying vec3 posInLightSpace;
		//	void main() {
		//		vec3 mtlDiff = vec3(0, 1, 0);
		//		gl_FragColor = vec4(mtlDiff, 1);
		//		//gl_FragColor = vec4(lightDir, 1);
		//		//if (length(lightDir) == 0) gl_FragColor = vec4(1, 0, 0, 1);
		//		//else gl_FragColor = vec4(0, 1, 0, 1);
		//	}
		//)";
		prog.fsSource = R"(
				uniform mat4 worldToLight;
				uniform vec3 lightDir;
				uniform vec3 eyePos;
				uniform sampler2D tex;

				varying vec4 position;
				varying vec4 normal;
				varying vec3 posInLightSpace;

				void main() {
					float c;
					vec3 lightDir2 = normalize(vec3(-1, -1, 1));
					lightDir2 = lightDir;
				
					vec3 n = normalize(normal.xyz);
					vec3 ref = reflect(lightDir2, n);
					vec3 eye = -normalize(eyePos - position.xyz);
					float spec = pow(clamp(dot(ref, eye), 0, 1), 16);
					vec3 col = vec3(0.3, 1, 0.3);
					vec3 speccol = vec3(1, 1, 1);
					c = dot(n, lightDir2);
					c = clamp(c, 0, 1);
					col = (1 - spec) * c * col + spec * speccol;
					vec4 diffcol = clamp(vec4(col.x, col.y, col.z, 1), 0, 1);
					float tdepth = texture(tex, (posInLightSpace.xy + vec2(1, 1)) * 0.5).x;
					c = (clamp(tdepth, -1, 1) + 1) * 0.5;
					gl_FragColor = vec4(tdepth, tdepth, tdepth, 1);
					float actualDepth = (posInLightSpace.z + 1) * 0.5;
					gl_FragColor = (tdepth >= actualDepth - 0.005) ? diffcol : vec4(0);
					//if (length(lightDir) == 0) gl_FragColor = vec4(1, 0, 0, 1);
				}
			)";
		prog.compile();

		// Render depth
		sprog.vsSource = R"(
			varying vec4 position;
			void main() {
				gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
			}
		)";
		sprog.fsSource = R"(
			void main(){
				gl_FragDepth = gl_FragCoord.z;
			}
		)";
		sprog.compile();

		// Render depth texture to display
		tprog.vsSource = R"(
			void main() {
				gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
				gl_TexCoord[0] = gl_MultiTexCoord0; 
			}
		)";
		tprog.fsSource = R"(
			uniform sampler2D tex;
			void main() {
				//gl_FragColor = vec4(gl_TexCoord[0].st, 0, 1);
				float c = texture(tex, gl_TexCoord[0].st).x;
				c = mod(c, 1);
				gl_FragColor = vec4(c, c, c, 1);
				//gl_FragColor = vec4(1, 0, 0, 1);
			}
		)";
		tprog.compile();

		depbuf.init();
	}

	double t = 0;
	void render() {
		t += 0.01;
		light.pos.x = 1 * cos(t);
		light.pos.y = 1 * sin(t);

		bool shadow = true;
		if (shadow) {
			pushCamOrtho();
			glDrawBuffer(GL_NONE);
			depbuf.bind();
			glPushAttrib(GL_VIEWPORT_BIT);
			glViewport(0, 0, depbuf.m_texSize.x, depbuf.m_texSize.y);
			glClear(GL_DEPTH_BUFFER_BIT);
			sprog.bind();
			sendTris();
			sprog.unbind();
			depbuf.unbind();
			popCamOrtho();
			glDrawBuffer(GL_FRONT);
			glPopAttrib();
		}

		drawLight();
		drawCamera();

		glEnable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depbuf.m_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
		tprog.bind();
		glBegin(GL_POLYGON);
		glTexCoord2d(0, 0); glVertex3d(-0.25, .5, 0);
		glTexCoord2d(1, 0); glVertex3d(0.25, .5, 0);
		glTexCoord2d(1, 1); glVertex3d(0.25, .5, 0.5);
		glTexCoord2d(0, 1); glVertex3d(-0.25, .5, 0.5);
		glEnd();
		tprog.unbind();
		glDisable(GL_TEXTURE_2D);

		prog.bind();

		glm::mat4 m;
		glGetFloatv(GL_MODELVIEW_MATRIX, &m[0][0]);
		glm::vec3 r = glm::vec3(light.pos.x, light.pos.y, light.pos.z)
			- glm::vec3(light.lookAt.x, light.lookAt.y, light.lookAt.z);
		r = glm::normalize(r);
		glm::vec3 lightDir = (glm::vec3)(m * glm::vec4(r, 0));
		//printf("lightDir = (%f, %f, %f)\n", lightDir.x, lightDir.y, lightDir.z);
		prog.setUniform("lightDir", lightDir);
		prog.setUniform("eyePos", glm::vec3(m * glm::vec4(camera.pos.x, camera.pos.y, camera.pos.z, 1)));

		glm::mat4 m2, m3;
		glm::vec3 lightPos = m * glm::vec4(light.pos.x, light.pos.y, light.pos.z, 1.0);
		glm::vec3 lightLook = m * glm::vec4(light.lookAt.x, light.lookAt.y, light.lookAt.z, 1.0);
		m2 = glm::lookAt(lightPos, lightLook, (glm::vec3)(m * glm::vec4(0, 0, 1, 0)));
		Vec3 bounds = light.shadowMapWorldSize * 0.5;
		m3 = glm::ortho(-bounds.x, bounds.x, -bounds.y, bounds.y, 0.0, light.shadowMapWorldSize.z);
		prog.setUniform("worldToLight", m3 * m2);

		sendTris();
		prog.unbind();
	}

	void pushCamOrtho() {
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();

		Vec3 bounds = light.shadowMapWorldSize * 0.5;
		glOrtho(-bounds.x, bounds.x, -bounds.y, bounds.y, 0, light.shadowMapWorldSize.z);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		gluLookAt(light.pos.x, light.pos.y, light.pos.z, light.lookAt.x, light.lookAt.y, light.lookAt.z, 0, 0, 1);

	}

	void popCamOrtho() {
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}

	void setCamera() {
		return;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		Vec3 bounds = light.shadowMapWorldSize * 0.5;
		glOrtho(-bounds.x, bounds.x, -bounds.y, bounds.y, 0, light.shadowMapWorldSize.z);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(light.pos.x, light.pos.y, light.pos.z, light.lookAt.x, light.lookAt.y, light.lookAt.z, 0, 0, 1);
	}

	void drawCamera() {
		glPushMatrix();
		glTranslated(camera.pos.x, camera.pos.y, camera.pos.z);
		glColor3d(0.5, 0.5, 1);
		glutWireCube(0.1);
		glPopMatrix();
	}

	void drawLight() {
		glColor3d(1, 1, 0);
		glPushMatrix();
		glTranslated(light.pos.x, light.pos.y, light.pos.z);
		glutWireSphere(0.05, 6, 6);
		glPopMatrix();
		glPushMatrix();
		glTranslated(light.lookAt.x, light.lookAt.y, light.lookAt.z);
		glutWireCube(0.025);
		glPopMatrix();
		glBegin(GL_LINES);
		glVertex3d(light.pos.x, light.pos.y, light.pos.z);
		glVertex3d(light.lookAt.x, light.lookAt.y, light.lookAt.z);
		glEnd();
	}

	void sendTris() {
		glPushMatrix();
		glTranslated(0, 0, 0.1);
		glRotated(90, 1, 0, 0);
		glColor3d(1, 1, 1);
		glFrontFace(GL_CW);
		glutSolidTeapot(0.1);
		glFrontFace(GL_CCW);
		glPopMatrix();
		glBegin(GL_POLYGON);
		float s = 0.3;
		glVertex3f(-s, -s, 0);
		glVertex3f( s, -s, 0);
		glVertex3f( s,  s, 0);
		glVertex3f(-s,  s, 0);
		glEnd();
	}
} scene01;

class GLUTCallbacks {
public:
	static void __cdecl idleFunc() {
		framerate.sleep();
		glutPostRedisplay();
	}

	static void __cdecl initFunc() {
		vssource = readFile("vs.glsl");
		fssource = readFile("fs.glsl");
		printf("OpenGL version is (%s)\n", glGetString(GL_VERSION));
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		/*glEnable(GL_LINE_SMOOTH);
		glEnable(GL_POLYGON_SMOOTH);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);*/
		if (1) {
			prog = glCreateProgramObjectARB();
			uint vs = glCreateShaderObjectARB(GL_VERTEX_SHADER);
			uint fs = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);

			const GLcharARB *src;
			src = vssource.c_str();
			glShaderSourceARB(vs, 1, (const char **)&src, NULL);
			src = fssource.c_str();
			glShaderSourceARB(fs, 1, (const char **)&src, NULL);
			glCompileShaderARB(vs);
			glCompileShaderARB(fs);

			GLint success;
			success = 0;
			glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
			if (success == GL_FALSE) {
				printf("Vertex shader compilation failed!\n");
				char buffer[200];
				glGetShaderInfoLog(vs, 200, 0, buffer);
				printf("Message: %s\n", buffer);
			}
			success = 0;
			glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
			if (success == GL_FALSE) {
				printf("Fragment shader compilation failed!\n");
				char buffer[200];
				glGetShaderInfoLog(fs, 200, 0, buffer);
				printf("Message: %s\n", buffer);
			}

			glAttachObjectARB(prog, vs);
			glAttachObjectARB(prog, fs);
			glLinkProgramARB(prog);
			glUseProgramObjectARB(prog);
		}
		if (1) {
			depthBuffer.init();
		}
		scene01.init();
	}

	static void __cdecl displayFunc() {
		framerate.tick();
		glClearColor(0.1, 0.1, 0.1, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// set camera
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(60.0, (double)window.width / window.height, 0.1, 10.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(camera.pos.x, camera.pos.y, camera.pos.z,
			camera.lookAt.x, camera.lookAt.y, camera.lookAt.z,
			camera.up.x, camera.up.y, camera.up.z);
		scene01.setCamera();

		// draw axes
		glBegin(GL_LINES);
		double axisSize = 0.5;
		glColor3d(1, 0, 0); glVertex3d(0, 0, 0); glVertex3d(axisSize, 0, 0);
		glColor3d(0, 1, 0); glVertex3d(0, 0, 0); glVertex3d(0, axisSize, 0);
		glColor3d(0, 0, 1); glVertex3d(0, 0, 0); glVertex3d(0, 0, axisSize);
		glEnd();


		// Teapot
		/*depthBuffer.bind();
		glUseProgram(prog);
		glColor3d(1, 1, 1);
		glPushMatrix();
		glTranslated(0, 0, 0.1);
		glRotated(90, 1, 0, 0);
		glFrontFace(GL_CW);
		glutSolidTeapot(0.1);
		glFrontFace(GL_CCW);
		glPopMatrix();
		glUseProgram(0);
		depthBuffer.unbind();

		// Floor
		glPushMatrix();
		glTranslatef(0, 0, -0.01);
		glColor3d(0.6, 0.6, 0.6);
		glBegin(GL_POLYGON);
		glVertex3d(-1, -1, 0);
		glVertex3d(1, -1, 0);
		glVertex3d(1, 1, 0);
		glVertex3d(-1, 1, 0);
		glEnd();
		glPopMatrix();*/

		// draw grid
		glBegin(GL_LINES);
		glColor3d(0.2, 0.2, 0.2);
		for (int i = -10; i <= 10; i++) {
			double cell = 0.1;
			double max = cell * 10;
			double pos = cell * i;
			glVertex3d(-max, pos, 0); glVertex3d(max, pos, 0);
			glVertex3d(pos, -max, 0); glVertex3d(pos, max, 0);
		}
		glEnd();

		// Text
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
		glWindowPos2d(10, window.height - 20);
		glDisable(GL_DEPTH_TEST);
		char buffer[100];
		sprintf_s(buffer, "FPS: %d", (int)framerate.get());
		glutBitmapString(GLUT_BITMAP_8_BY_13, (unsigned char*)buffer);
		glEnable(GL_DEPTH_TEST);

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		scene01.render();
		glutSwapBuffers();

		// print errors
		for (GLenum err; (err = glGetError()) != GL_NO_ERROR;)
		{
			printf("Error: %s\n", gluErrorString(err));
		}
	}

	static void __cdecl reshapeFunc(int width, int height) {
		window.width = width;
		window.height = height;
		glViewport(0, 0, width, height);
	}

	static void __cdecl motionFunc(int x, int y) {
		cameraController.inputMotion(x, y);
	}

	static void __cdecl mouseFunc(int button, int state, int x, int y) {
		cameraController.inputMouse(button, state, x, y);
	}

	static void __cdecl mouseWheelFunc(int wheel, int direction, int x, int y) {
		cameraController.inputWheel(wheel, direction, x, y);
	}
};

int main(int argc, char *argv[]) {
	//glutInitContextVersion(4, 3);
	//glutInitContextFlags(GLUT_COMPATIBILITY_PROFILE);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
	glutCreateWindow("ShadowMap");
	glewInit();

	GLUTCallbacks::initFunc();
	glutDisplayFunc(GLUTCallbacks::displayFunc);
	glutReshapeFunc(GLUTCallbacks::reshapeFunc);
	glutMotionFunc(GLUTCallbacks::motionFunc);
	glutMouseFunc(GLUTCallbacks::mouseFunc);
	glutMouseWheelFunc(GLUTCallbacks::mouseWheelFunc);
	glutIdleFunc(GLUTCallbacks::idleFunc);

	glutMainLoop();
	return 0;
}