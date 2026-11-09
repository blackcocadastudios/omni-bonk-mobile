// GL ES 3.0 include shim. Android uses the NDK headers; the desktop test
// harness (headless Mesa/llvmpipe) uses the minimal declarations below.
#pragma once
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#else
#include <cstddef>
#include <cstdint>
typedef unsigned int GLenum; typedef unsigned int GLuint; typedef int GLint; typedef int GLsizei;
typedef float GLfloat; typedef unsigned char GLboolean; typedef unsigned int GLbitfield; typedef char GLchar;
typedef ptrdiff_t GLsizeiptr; typedef ptrdiff_t GLintptr; typedef unsigned char GLubyte; typedef void GLvoid;
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_COLOR 0x0306
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_ALWAYS 0x0207
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_RED 0x1903
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_RGBA8 0x8058
#define GL_R8 0x8229
#define GL_R32F 0x822E
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_NO_ERROR 0
#define GL_FUNC_ADD 0x8006
#define GL_MAX_TEXTURE_SIZE 0x0D33
extern "C" {
void glActiveTexture(GLenum);
void glAttachShader(GLuint, GLuint);
void glBindBuffer(GLenum, GLuint);
void glBindFramebuffer(GLenum, GLuint);
void glBindRenderbuffer(GLenum, GLuint);
void glBindTexture(GLenum, GLuint);
void glBindVertexArray(GLuint);
void glBlendFunc(GLenum, GLenum);
void glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum);
void glBlitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
void glBufferData(GLenum, GLsizeiptr, const void*, GLenum);
void glBufferSubData(GLenum, GLintptr, GLsizeiptr, const void*);
GLenum glCheckFramebufferStatus(GLenum);
void glClear(GLbitfield);
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
void glColorMask(GLboolean, GLboolean, GLboolean, GLboolean);
void glCompileShader(GLuint);
GLuint glCreateProgram(void);
GLuint glCreateShader(GLenum);
void glCullFace(GLenum);
void glDeleteBuffers(GLsizei, const GLuint*);
void glDeleteFramebuffers(GLsizei, const GLuint*);
void glDeleteProgram(GLuint);
void glDeleteRenderbuffers(GLsizei, const GLuint*);
void glDeleteShader(GLuint);
void glDeleteTextures(GLsizei, const GLuint*);
void glDeleteVertexArrays(GLsizei, const GLuint*);
void glDepthFunc(GLenum);
void glDepthMask(GLboolean);
void glDisable(GLenum);
void glDisableVertexAttribArray(GLuint);
void glDrawArrays(GLenum, GLint, GLsizei);
void glDrawArraysInstanced(GLenum, GLint, GLsizei, GLsizei);
void glDrawElements(GLenum, GLsizei, GLenum, const void*);
void glDrawElementsInstanced(GLenum, GLsizei, GLenum, const void*, GLsizei);
void glEnable(GLenum);
void glEnableVertexAttribArray(GLuint);
void glFinish(void);
void glFramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint);
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint);
void glFrontFace(GLenum);
void glGenBuffers(GLsizei, GLuint*);
void glGenFramebuffers(GLsizei, GLuint*);
void glGenRenderbuffers(GLsizei, GLuint*);
void glGenTextures(GLsizei, GLuint*);
void glGenVertexArrays(GLsizei, GLuint*);
void glGenerateMipmap(GLenum);
GLenum glGetError(void);
void glGetIntegerv(GLenum, GLint*);
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
void glGetProgramiv(GLuint, GLenum, GLint*);
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
void glGetShaderiv(GLuint, GLenum, GLint*);
const GLubyte* glGetString(GLenum);
GLint glGetUniformLocation(GLuint, const GLchar*);
void glLinkProgram(GLuint);
void glPixelStorei(GLenum, GLint);
void glPolygonOffset(GLfloat, GLfloat);
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
void glRenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei);
void glScissor(GLint, GLint, GLsizei, GLsizei);
void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
void glTexImage3D(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
void glTexParameteri(GLenum, GLenum, GLint);
void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
void glUniform1f(GLint, GLfloat);
void glUniform1i(GLint, GLint);
void glUniform2f(GLint, GLfloat, GLfloat);
void glUniform3f(GLint, GLfloat, GLfloat, GLfloat);
void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*);
void glUseProgram(GLuint);
void glVertexAttrib4f(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
void glVertexAttribDivisor(GLuint, GLuint);
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
void glViewport(GLint, GLint, GLsizei, GLsizei);
}
#endif
