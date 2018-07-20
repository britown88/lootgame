#pragma once

#include "defs.h"
#include "math.h"

typedef u32 ShaderHandle;
typedef u32 TextureHandle;
typedef u32 TextureSlot;
typedef u32 VBOHandle;
typedef u32 FBOHandle;

typedef enum {
   RepeatType_REPEAT,
   RepeatType_CLAMP
}RepeatType_;
typedef byte RepeatType;

typedef enum {
   FilterType_LINEAR,
   FilterType_NEAREST
}FilterType_;
typedef byte FilterType;

struct TextureConfig {
   RepeatType repeatType = RepeatType_CLAMP;
   FilterType filterType = FilterType_NEAREST;
};

struct FBO {
   Int2 sz;
   TextureHandle tex;
   FBOHandle fbo;
};

typedef enum {
   VAttrib_Pos2 = 0,
   VAttrib_Tex2,

   VAttrib_COUNT
}VAttrib_;
typedef u16 VAttrib;

struct Vertex {
   Float2 pos2, tex2;
};

struct Mesh {
   VBOHandle handle;
   u32 vCount;
   i32 vSize;
};

enum BlendMode {
   BlendMode_DISABLED = 0,
   BlendMode_NORMAL,
   BlendMode_PURE_ADD,
   BlendMode_LIGHTING
};

enum Uniform_ {
   Uniform_TextureMatrix = 0,
   Uniform_ModelMatrix,
   Uniform_ViewMatrix,
   Uniform_TextureSlot,

   Uniform_Color,
   Uniform_Alpha,
   
   Uniform_ColorOnly,

   Uniform_COUNT
};
typedef u32 Uniform;

namespace uber {
   void resetToDefault(bool pushAll = false);

   void set(Uniform u, Matrix const& m, bool setAsDefault = false);
   void set(Uniform u, TextureSlot t, bool setAsDefault = false);
   void set(Uniform u, ColorRGBAf const& c, bool setAsDefault = false);
   void set(Uniform u, float f, bool setAsDefault = false);
   void set(Uniform u, bool b, bool setAsDefault = false);
}


namespace render{

   void clear(ColorRGBAf const& c);
   void viewport(Recti const& r);
   void setBlendMode(BlendMode mode);

   // shaders
   ShaderHandle shaderBuild(const char* vertex, const char* fragment);
   void shaderDestroy(ShaderHandle s);
   void shaderSetActive(ShaderHandle s);
      
   // textures
   TextureHandle textureBuild(ColorRGBA const* pixels, Int2 const& sz, TextureConfig const& cfg);
   void textureDestroy(TextureHandle t);
   void textureBind(TextureHandle t, TextureSlot slot = 0);

   // uniform sets
   void uSetBool(const char* u, bool value);
   void uSetUint(const char* u, u32 value);
   void uSetFloat(const char* u, f32 value);
   void uSetFloat2(const char* u, Float2 const& value);
   void uSetMatrix(const char* u, Matrix const& value);
   void uSetColor(const char* u, ColorRGBAf const& value);
   void uSetTextureSlot(const char* u, TextureSlot const& value);

   // FBO
   FBO fboBuild(Int2 sz);
   void fboDestroy(FBO& fbo);

   void fboBind(FBO const& fbo);

   // VBO
   Mesh meshBuild(Vertex const* vertices, u32 vCount);
   void meshDestroy(Mesh& m);

   void meshRender(Mesh const& m);
}
