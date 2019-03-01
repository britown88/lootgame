#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

#include "defs.h"
#include "game.h"

#include "reflection_gen.h"
#include "reflection_gen.inl"

Blob blobCreate(byte const* data, size_t sz) {
   Blob out = { new byte[sz], sz };
   memcpy(out.data, data, sz);
   return out;
}
void blobDestroy(Blob& self) {
   if (self.data) {
      delete[] self.data;
      self.data = nullptr;
      self.sz = 0;
   }
}