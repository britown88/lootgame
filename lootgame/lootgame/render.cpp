#include "render.h"

#include <GL/glew.h>
#include <vector>
#include <string>

using namespace render;

static ShaderHandle g_activeShader = 0;


void render::clear(ColorRGBAf const& c) {
   glClearColor(c.r, c.g, c.b, c.a);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void render::viewport(Recti const& r) {
   glViewport(r.x, r.y, r.w, r.h);
}

void render::setBlendMode(BlendMode mode) {
   switch (mode) {
   case BlendMode_DISABLED:
      glDisable(GL_BLEND);
      break;
   case BlendMode_PURE_ADD:
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      //glBlendEquation(GL_FUNC_SUBTRACT);
      break;
   case BlendMode_NORMAL:
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      //glBlendEquation(GL_FUNC_ADD);
      break;

   case BlendMode_LIGHTING:
      glEnable(GL_BLEND);
      glBlendFunc(GL_ZERO, GL_SRC_COLOR);
      //glBlendEquation(GL_FUNC_SUBTRACT);
      break;

   }
}

// shaders
static u32 _compile(const char* shader, i32 type) {
   u32 handle = glCreateShader(type);
   if (handle) {
      int compileStatus;
      const GLchar **source = &shader;
      glShaderSource(handle, 1, source, nullptr);
      glCompileShader(handle);

      glGetShaderiv(handle, GL_COMPILE_STATUS, &compileStatus);
      if (!compileStatus) {

         int infoLen = 0;
         glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLen);
         std::vector<GLchar> infoLog(infoLen);
         glGetShaderInfoLog(handle, infoLen, &infoLen, &infoLog[0]);
         std::string err = infoLog.data();

         return 0;
      }
   }

   return handle;
}
static ShaderHandle _link(u32 vertex, u32 fragment) {
   ShaderHandle handle = glCreateProgram();
   if (handle) {
      int linkStatus;
      if (vertex == -1 || fragment == -1) {
         return 0;
      }

      glBindAttribLocation(handle, (GLuint)VAttrib_Pos2, "aPos");
      glBindAttribLocation(handle, (GLuint)VAttrib_Tex2, "aTex");

      glAttachShader(handle, vertex);
      glAttachShader(handle, fragment);
      glLinkProgram(handle);

      glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus);
      if (!linkStatus) {
         GLsizei log_length = 0;
         GLchar message[1024];
         glGetProgramInfoLog(handle, 1024, &log_length, message);

         GLsizei srclen = 0;
         GLchar vsrc[10240], fsrc[10240];
         glGetShaderSource(vertex, 10240, &srclen, vsrc);
         glGetShaderSource(fragment, 10240, &srclen, fsrc);

         return 0;
      }
   }
   return handle;
}
ShaderHandle render::shaderBuild(const char* vertex, const char* fragment) {
   return _link(_compile(vertex, GL_VERTEX_SHADER), _compile(fragment, GL_FRAGMENT_SHADER));
}
void render::shaderDestroy(ShaderHandle s) {
   if (g_activeShader == s) {
      g_activeShader =0;
   }
   glDeleteProgram(s);
}
void render::shaderSetActive(ShaderHandle s) {
   //if (g_activeShader == s) {
   //   return;
   //}
   glUseProgram(s);
   g_activeShader = s;
}

// textures
TextureHandle render::textureBuild(ColorRGBA const* pixels, Int2 const& sz, TextureConfig const& cfg) {
   TextureHandle out = 0;
   
   glEnable(GL_TEXTURE_2D);
   glGenTextures(1, &out);
   glBindTexture(GL_TEXTURE_2D, out);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   switch (cfg.filterType)
   {
   case FilterType_LINEAR:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      break;
   case FilterType_NEAREST:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      break;
   };

   switch (cfg.repeatType)
   {
   case RepeatType_REPEAT:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      break;
   case RepeatType_CLAMP:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      break;
   };

   auto fcol = new ColorRGBAf[sz.x*sz.y];

   static f32 ibyte = 1.0f / 255.0f;

   for (int i = 0; i < sz.x*sz.y; ++i) {
      auto& c = pixels[i];
      f32 scale = c.a * ibyte;
      fcol[i] = {
         (c.r * ibyte) * scale,
         (c.g * ibyte) * scale,
         (c.b * ibyte) * scale,
         scale,
      };
   }

   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz.x, sz.y, 0, GL_RGBA, GL_FLOAT, fcol);

   delete[] fcol;

   glBindTexture(GL_TEXTURE_2D, 0);

   return out;
}
void render::textureDestroy(TextureHandle t) {
   glDeleteTextures(1, &t);
}
void render::textureBind(TextureHandle t, TextureSlot slot) {
   glActiveTexture(GL_TEXTURE0 + slot);
   glBindTexture(GL_TEXTURE_2D, t);
}

// FBO
FBO render::fboBuild(Int2 sz) {
   FBO out;

   out.sz = sz;

   glGenTextures(1, &out.tex);
   glBindTexture(GL_TEXTURE_2D, out.tex);

   //todo: format ops?
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz.x, sz.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   glGenFramebuffers(1, &out.fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
   //Attach 2D texture to this FBO

   int textureType = GL_TEXTURE_2D;
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureType, out.tex, 0);

   GLenum status;
   status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (GL_FRAMEBUFFER_COMPLETE != status)
   {
      //handle this error on the outside
      return {};
   }
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   return out;
}
void render::fboDestroy(FBO& fbo) {
   glDeleteTextures(1, &fbo.tex);
   glDeleteFramebuffers(1, &fbo.fbo);
}

void render::fboBind(FBO const& fbo) {
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.fbo);
   glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo.tex, 0);
}

// uniform sets
void render::uSetBool(const char* u, bool value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform1i(uHandle, value);
}
void render::uSetUint(const char* u, u32 value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform1ui(uHandle, value);
}
void render::uSetFloat2(const char* u, Float2 const& value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform2f(uHandle, value.x, value.y);
}
void render::uSetMatrix(const char* u, Matrix const& value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniformMatrix4fv(uHandle, 1, false, (float*)&value);
}
void render::uSetColor(const char* u, ColorRGBAf const& value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform4fv(uHandle, 1, (float*)&value);
}
void render::uSetTextureSlot(const char* u, TextureSlot const& value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform1i(uHandle, value);
}

Mesh render::meshBuild(Vertex const* vertices, u32 vCount) {
   Mesh out;
   out.vSize = (i32)sizeof(Vertex);
   out.vCount = vCount;

   glGenBuffers(1, (GLuint*)&out.handle);
   glBindBuffer(GL_ARRAY_BUFFER, out.handle);
   glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vCount, vertices, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   return out;
}
void render::meshDestroy(Mesh& m) {
   glDeleteBuffers(1, &m.handle);
}

void render::meshRender(Mesh const& m) {

   glBindBuffer(GL_ARRAY_BUFFER, m.handle);

   for (u32 i = 0; i < VAttrib_COUNT; ++i) {
      glDisableVertexAttribArray(i);
   }

   auto p = (void*)offsetof(Vertex, pos2);
   auto t = (void*)offsetof(Vertex, tex2);

   glEnableVertexAttribArray((u32)VAttrib_Pos2);
   glVertexAttribPointer((u32)VAttrib_Pos2, 2, GL_FLOAT, GL_FALSE, m.vSize, (void*)offsetof(Vertex, pos2));

   glEnableVertexAttribArray((u32)VAttrib_Tex2);
   glVertexAttribPointer((u32)VAttrib_Tex2, 2, GL_FLOAT, GL_FALSE, m.vSize, (void*)offsetof(Vertex, tex2));

   glDrawArrays(GL_TRIANGLES, 0, m.vCount);
}




