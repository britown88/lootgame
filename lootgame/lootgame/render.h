#pragma once

#include "defs.h"
#include "math.h"
#include "dynarray.h"

typedef u32 ShaderHandle;
typedef u32 TextureHandle;
typedef u32 TextureSlot;
typedef u32 VBOHandle;
typedef u32 FBOHandle;

enum TextureFlag_ {
   TextureFlag_WrapRepeat = (1 << 0),
   TextureFlag_WrapClamp = (1 << 1),
   TextureFlag_FilterLinear = (1 << 2),
   TextureFlag_FilterNearest = (1 << 3),

   TextureFlag_Color_SRGBA = (1 << 4),
   TextureFlag_Color_RGBA8 = (1 << 5),
   TextureFlag_Color_RGBA16F = (1 << 6),

   TextureFlag_DisablePremultiply = (1 << 7), // rgb data is premultiplied with alpha, this disables that step

   TextureFlag_ClampedLinear = TextureFlag_WrapClamp | TextureFlag_FilterLinear,
   TextureFlag_ClampedNearest = TextureFlag_WrapClamp | TextureFlag_FilterNearest,
   TextureFlag_RepeatedLinear = TextureFlag_WrapRepeat | TextureFlag_FilterLinear,
   TextureFlag_RepeatedNearest = TextureFlag_WrapRepeat | TextureFlag_FilterNearest,

   TextureFlag_Defaults = TextureFlag_ClampedNearest | TextureFlag_Color_SRGBA,
   TextureFlag_FBODefaults = TextureFlag_ClampedNearest | TextureFlag_Color_RGBA16F
};
typedef u16 TextureFlag;

struct Texture {
   Int2 sz;
   TextureHandle handle;
};

struct FBO {
   Int2 sz;
   FBOHandle fbo;
   DynamicArray<Texture> out;
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
   BlendMode_ADDITION,
   BlendMode_MULITPLY
};



enum Uniform_ {
#define UNIFORM(name, enumName, type, ...) Uniform_##enumName,
#include "uniforms.x"
#undef UNIFORM

   Uniform_COUNT
};
typedef u32 Uniform;



namespace uber {
   void resetToDefault(bool pushAll = false);

   void set(Uniform u, Matrix const& m, bool setAsDefault = false);
   void set(Uniform u, TextureSlot t, bool setAsDefault = false);
   void set(Uniform u, ColorRGBAf const& c, bool setAsDefault = false);
   void set(Uniform u, float f, bool setAsDefault = false);
   void set(Uniform u, Float2 v, bool setAsDefault = false);
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
   Texture textureBuild(Int2 const& sz, ColorRGBA const* pixels = nullptr, TextureFlag flags = TextureFlag_Defaults);

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
   FBO fboBuild(Int2 sz);  // creates one output with default texture flags
   FBO fboBuild(Int2 sz, std::initializer_list<Texture> outputs);
   void fboDestroy(FBO& fbo);

   void fboBind(FBO const& fbo);

   // VBO
   Mesh meshBuild(Vertex const* vertices, u32 vCount);
   void meshDestroy(Mesh& m);

   void meshRender(Mesh const& m);
}
