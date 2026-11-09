// Thin GL helpers
#pragma once
#include "base.h"
#include "gl.h"

GLuint gfxProgram(const char* vs, const char* fs, const char* name);

struct DynBuffer {
  GLuint id = 0; size_t cap = 0; GLenum target = GL_ARRAY_BUFFER;
  void init(GLenum t = GL_ARRAY_BUFFER) { target = t; glGenBuffers(1, &id); cap = 0; }
  void upload(const void* data, size_t bytes) {
    glBindBuffer(target, id);
    if (bytes > cap) { cap = std::max(bytes, cap * 2); glBufferData(target, cap, nullptr, GL_DYNAMIC_DRAW); }
    if (bytes) glBufferSubData(target, 0, bytes, data);
  }
  void destroy() { if (id) glDeleteBuffers(1, &id); id = 0; cap = 0; }
};

struct RenderTarget {
  GLuint fbo = 0, color = 0, depth = 0; int w = 0, h = 0;
  void create(int W, int H, bool withDepth = true, bool linear = false);
  void destroy();
};

struct Tex2D { GLuint id = 0; int w = 0, h = 0; };
Tex2D gfxTexRGBA(int w, int h, const u8* rgba, bool nearest, bool mips, bool repeat);
void gfxCheck(const char* where);
