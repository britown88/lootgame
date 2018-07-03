#pragma once

#include "defs.h"
#include "math.h"

namespace render{
   typedef u32 ShaderHandle;
   typedef u32 TextureHandle;
   typedef u32 TextureSlot;
   typedef u32 VBOHandle;

   // general
   void init();

   void clear(ColorRGBAf const& c);
   void viewport(Recti const& r);
   void enableAlphaBlending(bool enabled);

   // shaders
   ShaderHandle shaderBuild(const char* vertex, const char* fragment);
   void shaderDestroy(ShaderHandle s);
   void shaderSetActive(ShaderHandle s);
      
   // textures
   enum {
      RepeatType_REPEAT,
      RepeatType_CLAMP
   }RepeatType_;
   typedef byte RepeatType;

   enum {
      FilterType_LINEAR,
      FilterType_NEAREST
   }FilterType_;
   typedef byte FilterType;

   typedef struct {
      RepeatType repeatType = RepeatType_CLAMP;
      FilterType filterType = FilterType_NEAREST;
   } TextureConfig;

   TextureHandle textureBuild(ColorRGBA const* pixels, Int2 const& sz, TextureConfig const& cfg);
   void textureDestroy(TextureHandle t);
   void textureBind(TextureHandle t, TextureSlot slot);

   // uniform sets
   void uSetBool(const char* u, bool value);
   void uSetUint(const char* u, u32 value);
   void uSetFloat2(const char* u, Float2 const& value);
   void uSetMatrix(const char* u, Matrix const& value);
   void uSetColor(const char* u, ColorRGBAf const& value);
   void uSetTextureSlot(const char* u, TextureSlot const& value);

   // VBO
   enum {
      VAttrib_Pos2 = 0,
      VAttrib_Tex2,
      VAttrib_Col4,

      VAttrib_COUNT
   }VAttrib_;
   typedef u16 VAttrib;

   struct Vertex {
      Float2 pos2, tex2; ColorRGBAf col4;
   };

   VBOHandle vboBuild(Vertex const* vertices, u64 vCount);
   void vboDestroy(VBOHandle handle);

   void vboRender(VBOHandle handle, u32 vFirst, u32 vCount);
}
