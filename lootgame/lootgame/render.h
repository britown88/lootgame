#pragma once

#include "defs.h"
#include "math.h"


typedef uint32_t ShaderHandle;
typedef uint32_t TextureHandle;
typedef uint32_t TextureSlot;
typedef uint32_t VBOHandle;
typedef uint32_t FBOHandle;

// @reflect{ 
// @bitfield
enum TextureFlag_ : uint16_t {
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
}; // }
typedef uint16_t TextureFlag;

// @reflect{
struct Texture {
   // @image @file
   std::string filepath;

   // @readonly
   Int2 sz;

   // @type_override:TextureFlag_
   TextureFlag flags;

   Blob storedImageData;

   // @ignore{
   TextureHandle handle = 0; // }
}; // }

// @reflect{
struct FBO {
   Int2 sz;

   // @ignore{
   std::vector<Texture> out;
   FBOHandle fbo = 0; // }
}; // }

typedef enum {
   VAttrib_Pos2 = 0,
   VAttrib_Tex2,

   VAttrib_COUNT
}VAttrib_;
typedef uint16_t VAttrib;

struct Vertex {
   Float2 pos2, tex2;
};

struct Mesh {
   VBOHandle handle = 0;
   uint32_t vCount = 0;
   int32_t vSize = 0;
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
typedef uint32_t Uniform;



namespace uber {
   void resetToDefault(bool pushAll = false);

   void set(Uniform u, Matrix const& m, bool setAsDefault = false);

   // deprecating, use the textureBinding helper instead
   //void set(Uniform u, TextureSlot t, bool setAsDefault = false);

   void set(Uniform u, ColorRGBAf const& c, bool setAsDefault = false);
   void set(Uniform u, float f, bool setAsDefault = false);
   void set(Uniform u, Float2 v, bool setAsDefault = false);
   void set(Uniform u, Float3 v, bool setAsDefault = false);
   void set(Uniform u, bool b, bool setAsDefault = false);

   // helper that obfuscates texture slots
   // texture uniforms will default to unique slots
   // and this function will do the hookup and bind for you
   // as long as pushAll was done at least once
   void bindTexture(Uniform u, TextureHandle handle);
}


namespace render{

   // wrap srgb toggles for imgui drawlist
   void enableSRGB();
   void disableSRGB();

   void clear(ColorRGBAf const& c);
   void viewport(Recti const& r);
   void setBlendMode(BlendMode mode);

   // shaders
   ShaderHandle shaderBuild(const char* vertex, const char* fragment);
   void shaderDestroy(ShaderHandle s);
   void shaderSetActive(ShaderHandle s);
      
   // textures
   Texture textureBuild(Int2 const& sz, TextureFlag flags = TextureFlag_Defaults, ColorRGBA const* pixels = nullptr);

   void textureDestroy(TextureHandle t);
   void textureBind(TextureHandle t, TextureSlot slot = 0);

   // uniform sets
   void uSetBool(const char* u, bool value);
   void uSetUint(const char* u, uint32_t value);
   void uSetFloat(const char* u, float value);
   void uSetFloat2(const char* u, Float2 const& value);
   void uSetFloat3(const char* u, Float3 const& value);
   void uSetMatrix(const char* u, Matrix const& value);
   void uSetColor(const char* u, ColorRGBAf const& value);
   void uSetTextureSlot(const char* u, TextureSlot const& value);

   // FBO
   FBO fboBuild(Int2 sz);  // creates one output with default texture flags
   FBO fboBuild(Int2 sz, std::initializer_list<Texture> outputs);
   void fboDestroy(FBO& fbo);

   void fboBind(FBO const& fbo);

   // VBO
   Mesh meshBuild(Vertex const* vertices, uint32_t vCount);
   void meshDestroy(Mesh& m);

   void meshRender(Mesh const& m);
}
