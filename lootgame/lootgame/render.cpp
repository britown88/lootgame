#include "stdafx.h"

#include "render.h"

#include <GL/glew.h>
#include "win.h"
#include "app.h"
#include <stb/stb_image.h>




using namespace render;

typedef bool Bool;
typedef float Float;

static ShaderHandle g_activeShader = 0;

struct UberShader {
   struct Defaults {
#define UNIFORM(name, enumName, type, ...) type name = __VA_ARGS__;
#include "uniforms.x"
#undef UNIFORM
   } defaults;

   struct UData {
      enum Type {
         Matrix,
         TextureSlot,
         ColorRGBAf,
         Float,
         Float2,
         Float3,
         Bool
      };
      StringView name;
      size_t offset;
      Type type;
   };

   UData dataMap[Uniform_COUNT] = {
#define UNIFORM(name, enumName, type, ...) {#name, offsetof(Defaults, name), UData::type},
#include "uniforms.x"
#undef UNIFORM
   };

   bool modified[Uniform_COUNT] = { 0 };

   void resetToDefault(bool pushAll = false) {
      for(int i = 0; i < Uniform_COUNT; ++i){
         if (modified[i] || pushAll) {
            modified[i] = false;
            switch (dataMap[i].type) {
            case UData::Matrix:
               render::uSetMatrix(dataMap[i].name, *(Matrix*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            case UData::TextureSlot:
               render::uSetTextureSlot(dataMap[i].name, *(TextureSlot*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            case UData::ColorRGBAf:
               render::uSetColor(dataMap[i].name, *(ColorRGBAf*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            case UData::Float:
               render::uSetFloat(dataMap[i].name, *(float*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            case UData::Float2:
               render::uSetFloat2(dataMap[i].name, *(Float2*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            case UData::Float3:
               render::uSetFloat3(dataMap[i].name, *(Float3*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            case UData::Bool:
               render::uSetBool(dataMap[i].name, *(bool*)(((byte*)&defaults) + dataMap[i].offset));
               break;
            }
         }
      }
   }

   void set(Uniform u, Matrix const& m) {
      render::uSetMatrix(dataMap[u].name, m);
      modified[u] = true;
   }
   void set(Uniform u, TextureSlot t) {
      render::uSetTextureSlot(dataMap[u].name, t);
      modified[u] = true;
   }
   void set(Uniform u, ColorRGBAf const& c) {
      render::uSetColor(dataMap[u].name, c);
      modified[u] = true;
   }
   void set(Uniform u, float f) {
      render::uSetFloat(dataMap[u].name, f);
      modified[u] = true;
   }
   void set(Uniform u, bool b) {
      render::uSetBool(dataMap[u].name, b);
      modified[u] = true;
   }
   void set(Uniform u, Float2 v) {
      render::uSetFloat2(dataMap[u].name, v);
      modified[u] = true;
   }
   void set(Uniform u, Float3 v) {
      render::uSetFloat3(dataMap[u].name, v);
      modified[u] = true;
   }
};

UberShader g_uberShader;

// ubershader
void uber::resetToDefault(bool pushAll) {
   g_uberShader.resetToDefault(pushAll);
}

void uber::set(Uniform u, Matrix const& m, bool setAsDefault) {
   g_uberShader.set(u, m);
   if (setAsDefault) {
      *(Matrix*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = m;
      g_uberShader.modified[u] = false;
   }
}
//void uber::set(Uniform u, TextureSlot t, bool setAsDefault) {
//   g_uberShader.set(u, t);
//   if (setAsDefault) {
//      *(TextureSlot*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = t;
//      g_uberShader.modified[u] = false;
//   }
//}
void uber::set(Uniform u, ColorRGBAf const& c, bool setAsDefault) {
   g_uberShader.set(u, c);
   if (setAsDefault) {
      *(ColorRGBAf*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = c;
      g_uberShader.modified[u] = false;
   }
}
void uber::set(Uniform u, float f, bool setAsDefault) {
   g_uberShader.set(u, f);
   if (setAsDefault) {
      *(float*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = f;
      g_uberShader.modified[u] = false;
   }
}
void uber::set(Uniform u, Float2 v, bool setAsDefault) {
   g_uberShader.set(u, v);
   if (setAsDefault) {
      *(Float2*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = v;
      g_uberShader.modified[u] = false;
   }
}
void uber::set(Uniform u, Float3 v, bool setAsDefault) {
   g_uberShader.set(u, v);
   if (setAsDefault) {
      *(Float3*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = v;
      g_uberShader.modified[u] = false;
   }
}
void uber::set(Uniform u, bool b, bool setAsDefault) {
   g_uberShader.set(u, b);
   if (setAsDefault) {
      *(bool*)(((byte*)&g_uberShader.defaults) + g_uberShader.dataMap[u].offset) = b;
      g_uberShader.modified[u] = false;
   }
}

void uber::bindTexture(Uniform u, TextureHandle handle) {
   auto& data = g_uberShader.dataMap[u];
   assert(data.type == UberShader::UData::Type::TextureSlot);

   auto slot = *(TextureSlot*)(((byte*)&g_uberShader.defaults) + data.offset);
   render::uSetTextureSlot(data.name, slot);
   render::textureBind(handle, slot);
}

void render::enableSRGB() {
   glEnable(GL_FRAMEBUFFER_SRGB);
}
void render::disableSRGB() {
   glDisable(GL_FRAMEBUFFER_SRGB);
}

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
   case BlendMode_ADDITION:
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      break;
   case BlendMode_MULITPLY:
      glEnable(GL_BLEND);
      glBlendFunc(GL_ZERO, GL_SRC_COLOR);
      break;
   case BlendMode_NORMAL:
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
   }
}

// shaders
static uint32_t _compile(const char* shader, int32_t type) {
   uint32_t handle = glCreateShader(type);
   if (handle) {
      int compileStatus;
      const GLchar **source = &shader;
      glShaderSource(handle, 1, source, nullptr);
      glCompileShader(handle);

      glGetShaderiv(handle, GL_COMPILE_STATUS, &compileStatus);
      if (!compileStatus) {

         int infoLen = 0;
         glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLen);
         Array<GLchar> infoLog(infoLen);
         glGetShaderInfoLog(handle, infoLen, &infoLen, &infoLog[0]);
         std::string err = infoLog.data();

         return 0;
      }
   }

   return handle;
}
static ShaderHandle _link(uint32_t vertex, uint32_t fragment) {
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

TextureHandle render::buildTextureHandle(Int2 const& sz, TextureFlag flags, ColorRGBA const* pixels) {
   TextureHandle out = 0;

   glEnable(GL_TEXTURE_2D);
   glGenTextures(1, &out);
   glBindTexture(GL_TEXTURE_2D, out);
   //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   if (flags & TextureFlag_FilterNearest) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   }
   else if (flags & TextureFlag_FilterLinear) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   }

   if (flags & TextureFlag_WrapRepeat) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   }
   else if (flags & TextureFlag_WrapClamp) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   }

   GLuint colorFormat = 0;
   GLenum type = 0;
   if (flags&TextureFlag_Color_SRGBA) {
      colorFormat = GL_SRGB8_ALPHA8;
      type = GL_UNSIGNED_BYTE;
   }
   else if (flags&TextureFlag_Color_RGBA8) {
      colorFormat = GL_RGBA8;
      type = GL_UNSIGNED_BYTE;
   }
   else if (!pixels && flags&TextureFlag_Color_RGBA16F) {
      // only allowed if we didnt pass in a pixel buffer, since this assumes floats (used for fbos)
      colorFormat = GL_RGBA16F;
      type = GL_FLOAT;
   }

   if (colorFormat) {
      //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glTexImage2D(GL_TEXTURE_2D, 0, colorFormat, sz.x, sz.y, 0, GL_RGBA, type, pixels);

   }

   
   glBindTexture(GL_TEXTURE_2D, 0);
   return out;
}

void render::textureRefresh(Texture&t) {
   if (!t.filepath.empty()) {
      int32_t comp = 0;

      auto assetPath = AppConfig.assetPath / t.filepath;

      if (t.storedImageData) {
         blobDestroy(t.storedImageData);
      }
      uint64_t fsz = 0;
      auto mem = fileReadBinary(assetPath.string().c_str(), &fsz);
      t.storedImageData.data = stbi_load_from_memory(mem, (int32_t)fsz, &t.sz.x, &t.sz.y, &comp, 4);
      t.storedImageData.sz = t.sz.x * t.sz.y * comp;

      if (!(t.flags&TextureFlag_DisablePremultiply)) {
         auto pixels = ((ColorRGBA*)t.storedImageData.data);
         for (int i = 0; i < t.sz.x*t.sz.y; ++i) {
            pixels[i] = srgbPremultipleAlpha(pixels[i]);
         }
      }

      free(mem);

      if (t.handle) {
         render::textureDestroy(t.handle);
      }

      t.handle = buildTextureHandle(t.sz, t.flags, (ColorRGBA const*)t.storedImageData.data);

      LOG("Refreshed texture [%s] from file", t.id);
   }
}



// textures
Texture render::textureBuild(Int2 const& sz, TextureFlag flags, ColorRGBA const* pixels) {
   Texture out;
   out.sz = sz;
   out.handle = buildTextureHandle(sz, flags, pixels);  

   return out;
}

void render::textureDestroy(TextureHandle& t) {
   glDeleteTextures(1, &t);
   t = 0;
}
void render::textureBind(TextureHandle t, TextureSlot slot) {
   glActiveTexture(GL_TEXTURE0 + slot);
   glBindTexture(GL_TEXTURE_2D, t);
}

// FBO
FBO render::fboBuild(Int2 sz) {
   return fboBuild(sz, { textureBuild(sz, TextureFlag_FBODefaults) });
}
FBO render::fboBuild(Int2 sz, std::initializer_list<Texture> outputs) {
   FBO out;
   out.sz = sz;
   glGenFramebuffers(1, &out.fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);

   int i = 0;
   for (auto&& output : outputs) {      
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i++, GL_TEXTURE_2D, output.handle, 0);
      out.out.push_back(output);
   }

   GLenum status;
   status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

   assert(status == GL_FRAMEBUFFER_COMPLETE);

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   return out;
}

void render::fboDestroy(FBO& fbo) {
   for (auto&&texture : fbo.out) {
      textureDestroy(texture.handle);
   }

   glDeleteFramebuffers(1, &fbo.fbo);
}

void render::fboBind(FBO const& fbo) {
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.fbo);
   if (fbo.out.empty()) {
      return;
   }

   int i = 0;
   for (auto&&texture : fbo.out) {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i++, GL_TEXTURE_2D, texture.handle, 0);
   }

   // this is extremely dumb opengl
   static GLenum attachments[] = {
      GL_COLOR_ATTACHMENT0, 
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2,
      GL_COLOR_ATTACHMENT3,
      GL_COLOR_ATTACHMENT4,
   };
   glDrawBuffers((GLsizei)fbo.out.size(), attachments);

   render::viewport({ 0,0, fbo.sz.x, fbo.sz.y });
}

// uniform sets
void render::uSetBool(const char* u, bool value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform1i(uHandle, value);
}
void render::uSetUint(const char* u, uint32_t value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform1ui(uHandle, value);
}
void render::uSetFloat(const char* u, float value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform1f(uHandle, value);
}
void render::uSetFloat2(const char* u, Float2 const& value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform2f(uHandle, value.x, value.y);
}
void render::uSetFloat3(const char* u, Float3 const& value) {
   auto uHandle = glGetUniformLocation(g_activeShader, u);
   glUniform3f(uHandle, value.x, value.y, value.z);
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

Mesh render::meshBuild(Vertex const* vertices, uint32_t vCount) {
   Mesh out;
   out.vSize = (int32_t)sizeof(Vertex);
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

   for (uint32_t i = 0; i < VAttrib_COUNT; ++i) {
      glDisableVertexAttribArray(i);
   }

   auto p = (void*)offsetof(Vertex, pos2);
   auto t = (void*)offsetof(Vertex, tex2);

   glEnableVertexAttribArray((uint32_t)VAttrib_Pos2);
   glVertexAttribPointer((uint32_t)VAttrib_Pos2, 2, GL_FLOAT, GL_FALSE, m.vSize, (void*)offsetof(Vertex, pos2));

   glEnableVertexAttribArray((uint32_t)VAttrib_Tex2);
   glVertexAttribPointer((uint32_t)VAttrib_Tex2, 2, GL_FLOAT, GL_FALSE, m.vSize, (void*)offsetof(Vertex, tex2));

   glDrawArrays(GL_TRIANGLES, 0, m.vCount);
}




