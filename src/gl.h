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
// X-macro list of every GL ES 3.0 entry point the game uses
#define OMNI_GL_FUNCS(X) \
  X(void, glActiveTexture, (GLenum)) \
  X(void, glAttachShader, (GLuint, GLuint)) \
  X(void, glBindBuffer, (GLenum, GLuint)) \
  X(void, glBindFramebuffer, (GLenum, GLuint)) \
  X(void, glBindRenderbuffer, (GLenum, GLuint)) \
  X(void, glBindTexture, (GLenum, GLuint)) \
  X(void, glBindVertexArray, (GLuint)) \
  X(void, glBlendFunc, (GLenum, GLenum)) \
  X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
  X(void, glBlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
  X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
  X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*)) \
  X(GLenum, glCheckFramebufferStatus, (GLenum)) \
  X(void, glClear, (GLbitfield)) \
  X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
  X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
  X(void, glCompileShader, (GLuint)) \
  X(GLuint, glCreateProgram, (void)) \
  X(GLuint, glCreateShader, (GLenum)) \
  X(void, glCullFace, (GLenum)) \
  X(void, glDeleteBuffers, (GLsizei, const GLuint*)) \
  X(void, glDeleteFramebuffers, (GLsizei, const GLuint*)) \
  X(void, glDeleteProgram, (GLuint)) \
  X(void, glDeleteRenderbuffers, (GLsizei, const GLuint*)) \
  X(void, glDeleteShader, (GLuint)) \
  X(void, glDeleteTextures, (GLsizei, const GLuint*)) \
  X(void, glDeleteVertexArrays, (GLsizei, const GLuint*)) \
  X(void, glDepthFunc, (GLenum)) \
  X(void, glDepthMask, (GLboolean)) \
  X(void, glDisable, (GLenum)) \
  X(void, glDisableVertexAttribArray, (GLuint)) \
  X(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
  X(void, glDrawArraysInstanced, (GLenum, GLint, GLsizei, GLsizei)) \
  X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void*)) \
  X(void, glDrawElementsInstanced, (GLenum, GLsizei, GLenum, const void*, GLsizei)) \
  X(void, glEnable, (GLenum)) \
  X(void, glEnableVertexAttribArray, (GLuint)) \
  X(void, glFinish, (void)) \
  X(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
  X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
  X(void, glFrontFace, (GLenum)) \
  X(void, glGenBuffers, (GLsizei, GLuint*)) \
  X(void, glGenFramebuffers, (GLsizei, GLuint*)) \
  X(void, glGenRenderbuffers, (GLsizei, GLuint*)) \
  X(void, glGenTextures, (GLsizei, GLuint*)) \
  X(void, glGenVertexArrays, (GLsizei, GLuint*)) \
  X(void, glGenerateMipmap, (GLenum)) \
  X(GLenum, glGetError, (void)) \
  X(void, glGetIntegerv, (GLenum, GLint*)) \
  X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
  X(void, glGetProgramiv, (GLuint, GLenum, GLint*)) \
  X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
  X(void, glGetShaderiv, (GLuint, GLenum, GLint*)) \
  X(const GLubyte*, glGetString, (GLenum)) \
  X(GLint, glGetUniformLocation, (GLuint, const GLchar*)) \
  X(void, glLinkProgram, (GLuint)) \
  X(void, glPixelStorei, (GLenum, GLint)) \
  X(void, glPolygonOffset, (GLfloat, GLfloat)) \
  X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
  X(void, glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
  X(void, glScissor, (GLint, GLint, GLsizei, GLsizei)) \
  X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
  X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
  X(void, glTexImage3D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
  X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
  X(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)) \
  X(void, glUniform1f, (GLint, GLfloat)) \
  X(void, glUniform1i, (GLint, GLint)) \
  X(void, glUniform2f, (GLint, GLfloat, GLfloat)) \
  X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
  X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
  X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
  X(void, glUseProgram, (GLuint)) \
  X(void, glVertexAttrib4f, (GLuint, GLfloat, GLfloat, GLfloat, GLfloat)) \
  X(void, glVertexAttribDivisor, (GLuint, GLuint)) \
  X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
  X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))

#ifdef OMNIBONK_GL_LOADER
// SDL build (handhelds): no link-time dependency on a GLES library name. The entry points are function
// pointers filled through SDL_GL_GetProcAddress after the context exists (main_sdl.cpp).
#define X(ret, name, args) extern ret (*omni_##name) args;
OMNI_GL_FUNCS(X)
#undef X
#define glActiveTexture omni_glActiveTexture
#define glAttachShader omni_glAttachShader
#define glBindBuffer omni_glBindBuffer
#define glBindFramebuffer omni_glBindFramebuffer
#define glBindRenderbuffer omni_glBindRenderbuffer
#define glBindTexture omni_glBindTexture
#define glBindVertexArray omni_glBindVertexArray
#define glBlendFunc omni_glBlendFunc
#define glBlendFuncSeparate omni_glBlendFuncSeparate
#define glBlitFramebuffer omni_glBlitFramebuffer
#define glBufferData omni_glBufferData
#define glBufferSubData omni_glBufferSubData
#define glCheckFramebufferStatus omni_glCheckFramebufferStatus
#define glClear omni_glClear
#define glClearColor omni_glClearColor
#define glColorMask omni_glColorMask
#define glCompileShader omni_glCompileShader
#define glCreateProgram omni_glCreateProgram
#define glCreateShader omni_glCreateShader
#define glCullFace omni_glCullFace
#define glDeleteBuffers omni_glDeleteBuffers
#define glDeleteFramebuffers omni_glDeleteFramebuffers
#define glDeleteProgram omni_glDeleteProgram
#define glDeleteRenderbuffers omni_glDeleteRenderbuffers
#define glDeleteShader omni_glDeleteShader
#define glDeleteTextures omni_glDeleteTextures
#define glDeleteVertexArrays omni_glDeleteVertexArrays
#define glDepthFunc omni_glDepthFunc
#define glDepthMask omni_glDepthMask
#define glDisable omni_glDisable
#define glDisableVertexAttribArray omni_glDisableVertexAttribArray
#define glDrawArrays omni_glDrawArrays
#define glDrawArraysInstanced omni_glDrawArraysInstanced
#define glDrawElements omni_glDrawElements
#define glDrawElementsInstanced omni_glDrawElementsInstanced
#define glEnable omni_glEnable
#define glEnableVertexAttribArray omni_glEnableVertexAttribArray
#define glFinish omni_glFinish
#define glFramebufferRenderbuffer omni_glFramebufferRenderbuffer
#define glFramebufferTexture2D omni_glFramebufferTexture2D
#define glFrontFace omni_glFrontFace
#define glGenBuffers omni_glGenBuffers
#define glGenFramebuffers omni_glGenFramebuffers
#define glGenRenderbuffers omni_glGenRenderbuffers
#define glGenTextures omni_glGenTextures
#define glGenVertexArrays omni_glGenVertexArrays
#define glGenerateMipmap omni_glGenerateMipmap
#define glGetError omni_glGetError
#define glGetIntegerv omni_glGetIntegerv
#define glGetProgramInfoLog omni_glGetProgramInfoLog
#define glGetProgramiv omni_glGetProgramiv
#define glGetShaderInfoLog omni_glGetShaderInfoLog
#define glGetShaderiv omni_glGetShaderiv
#define glGetString omni_glGetString
#define glGetUniformLocation omni_glGetUniformLocation
#define glLinkProgram omni_glLinkProgram
#define glPixelStorei omni_glPixelStorei
#define glPolygonOffset omni_glPolygonOffset
#define glReadPixels omni_glReadPixels
#define glRenderbufferStorage omni_glRenderbufferStorage
#define glScissor omni_glScissor
#define glShaderSource omni_glShaderSource
#define glTexImage2D omni_glTexImage2D
#define glTexImage3D omni_glTexImage3D
#define glTexParameteri omni_glTexParameteri
#define glTexSubImage2D omni_glTexSubImage2D
#define glUniform1f omni_glUniform1f
#define glUniform1i omni_glUniform1i
#define glUniform2f omni_glUniform2f
#define glUniform3f omni_glUniform3f
#define glUniform4f omni_glUniform4f
#define glUniformMatrix4fv omni_glUniformMatrix4fv
#define glUseProgram omni_glUseProgram
#define glVertexAttrib4f omni_glVertexAttrib4f
#define glVertexAttribDivisor omni_glVertexAttribDivisor
#define glVertexAttribPointer omni_glVertexAttribPointer
#define glViewport omni_glViewport
#else
extern "C" {
#define X(ret, name, args) ret name args;
OMNI_GL_FUNCS(X)
#undef X
}
#endif
#endif
