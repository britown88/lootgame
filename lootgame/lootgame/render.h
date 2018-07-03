#pragma once

#include "defs.h"
#include "math.h"

namespace render{
   typedef uPtr ShaderHandle;
   typedef uPtr TextureHandle;
   typedef uPtr TextureSlot;

   // general
   void init();
   void swapBuffers();

   void clear(ColorRGBAf const& c);
   void viewport(Recti const& r);

   void enableDepth(bool enabled);
   void enableAlphaBlending(bool enabled);

   // shaders
   ShaderHandle compileShader(const char* vertex, const char* fragment);
   void destroyShader(ShaderHandle s);
   void setShader(ShaderHandle s);
      
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

   TextureHandle buildTexture(ColorRGBA const* pixels, Int2 const& sz, TextureConfig const& cfg);
   void destroyTexture(TextureHandle t);
   void bindTexture(TextureHandle t, TextureSlot slot);

   // uniform sets
   void uSetBool(const char* u, bool value);
   void uSetUint(const char* u, u32 value);
   void uSetFloat2(const char* u, Float2 const& value);
   void uSetMatrix(const char* u, Matrix const& value);
   void uSetColor(const char* u, ColorRGBAf const& value);
   void uSetTextureSlot(const char* u, TextureSlot const& value);

   
}
