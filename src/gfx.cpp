#include "gfx.h"

static GLuint compileShader(GLenum type, const char* src, const char* name) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096]; GLsizei n = 0; glGetShaderInfoLog(s, sizeof(log), &n, log);
    LOGE("Shader compile failed (%s %s):\n%s", name, type == GL_VERTEX_SHADER ? "VS" : "FS", log);
  }
  return s;
}

GLuint gfxProgram(const char* vs, const char* fs, const char* name) {
  GLuint p = glCreateProgram();
  GLuint v = compileShader(GL_VERTEX_SHADER, vs, name), f = compileShader(GL_FRAGMENT_SHADER, fs, name);
  glAttachShader(p, v); glAttachShader(p, f);
  glLinkProgram(p);
  GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) { char log[4096]; GLsizei n = 0; glGetProgramInfoLog(p, sizeof(log), &n, log); LOGE("Link failed (%s):\n%s", name, log); }
  glDeleteShader(v); glDeleteShader(f);
  return p;
}

void RenderTarget::create(int W, int H, bool withDepth, bool linear) {
  destroy();
  w = W; h = H;
  glGenTextures(1, &color);
  glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  if (withDepth) {
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
  }
  GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (st != GL_FRAMEBUFFER_COMPLETE) LOGE("FBO incomplete 0x%x (%dx%d)", st, w, h);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::destroy() {
  if (fbo) glDeleteFramebuffers(1, &fbo);
  if (color) glDeleteTextures(1, &color);
  if (depth) glDeleteRenderbuffers(1, &depth);
  fbo = color = depth = 0; w = h = 0;
}

Tex2D gfxTexRGBA(int w, int h, const u8* rgba, bool nearest, bool mips, bool repeat) {
  Tex2D t; t.w = w; t.h = h;
  glGenTextures(1, &t.id);
  glBindTexture(GL_TEXTURE_2D, t.id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  if (mips) glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? (nearest ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR) : (nearest ? GL_NEAREST : GL_LINEAR));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  return t;
}

void gfxCheck(const char* where) {
  GLenum e = glGetError();
  if (e != GL_NO_ERROR) LOGE("GL error 0x%x at %s", e, where);
}
