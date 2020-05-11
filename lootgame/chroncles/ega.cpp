#include "stdafx.h"

#include "ega.h"

#include <string.h>
#include <list>
#include <vector>
#include <algorithm>

byte getBit(byte dest, byte pos/*0-7*/) {
   return !!(dest & (1 << (pos & 7)));
}

static void _buildColorTable(ColorRGB *table) {
   byte i;
   //                   00 01  10   11
   byte rgbLookup[] = { 0, 85, 170, 255 };
   for (i = 0; i < EGA_COLORS; ++i) {
      byte shift = 5;

      byte r = getBit(i, shift--);
      byte g = getBit(i, shift--);
      byte b = getBit(i, shift--);
      byte R = getBit(i, shift--);
      byte G = getBit(i, shift--);
      byte B = getBit(i, shift);

      byte rgb_r = rgbLookup[(R << 1) + r];
      byte rgb_g = rgbLookup[(G << 1) + g];
      byte rgb_b = rgbLookup[(B << 1) + b];

      table[i] = ColorRGB{ rgb_r, rgb_g, rgb_b };
   }
}

ColorRGB g_egaToRGBTable[64] = { 0 };
void egaStartup() {
   _buildColorTable(g_egaToRGBTable);
}

//EGAColor egaReduceRGB(ColorRGB c) {
//   auto lin = srgbToLinear(c);
//   byte r = (byte)lin.x * 4.0f;
//   byte g = (byte)lin.y * 4.0f;
//   byte b = (byte)lin.z * 4.0f;
//}

/*
pixel data organization

One byte per pixel, a palette index, unless it is EGA_ALPHA which means its transparent
a pixel cannot both have color and be transparent
*/


static void _freeTextureBuffers(EGATexture &self) {
   if (self.decodePixels) {
      delete[] self.decodePixels;
      self.decodePixels = nullptr;
   }

   blobDestroy(self.pixels);
}

EGATexture egaTextureCreate(uint32_t width, uint32_t height) {
   EGATexture self;

   egaTextureResize(self, width, height);

   return self;
}
EGATexture egaTextureCreateCopy(EGATexture const &other) {
   auto out = egaTextureCreate(other.sz.x, other.sz.y);
   memcpy(out.pixels.data, other.pixels.data, out.pixels.sz);
   return out;
}
void egaTextureDestroyContent(EGATexture &self) {
   _freeTextureBuffers(self);
}

#pragma region OLD ENCODING CODE

struct PaletteColor;

struct PaletteEntry {
   PaletteEntry *next, *prev;
   float distance;
   PaletteColor* color;
};

typedef PaletteEntry* pPaletteEntry;

struct PaletteColor {
   byte removable;
   byte pPos;
   byte EGAColor;
   float distance;
   PaletteEntry entries[64];
   PaletteColor() :removable(1) {}
   PaletteColor(byte c) :EGAColor(c), removable(1) {}
   PaletteColor(byte c, byte targetPalettPosition) :EGAColor(c), removable(0), pPos(targetPalettPosition) {}
};

struct ImageColor {
   pPaletteEntry closestColor;
   ImageColor() :closestColor(0) {}
};

float GCRGB(byte component) {
   static float GCRGBTable[256] = { 0.0f };
   static int loaded = 0;

   if (!loaded) {
      int i;
      for (i = 0; i < 256; ++i) {
         GCRGBTable[i] = pow(i / 255.0f, 2.2f);
      }
      loaded = 1;
   }

   return GCRGBTable[component];
}

static float sRGB(byte b) {
   float x = b / 255.0f;
   if (x <= 0.00031308f) {
      return 12.92f * x;
   }
   else {
      return 1.055f*pow(x, (1.0f / 2.4f)) - 0.055f;
   }
}

Float3 srgbToLinear(ColorRGB const&srgb) {
   return { sRGB(srgb.r), sRGB(srgb.g), sRGB(srgb.b) };
}
Float3 srgbToLinear(ColorRGBA const&srgb) {
   return { sRGB(srgb.r), sRGB(srgb.g), sRGB(srgb.b) };
}

ColorHSV srgbToHSV(ColorRGB const&srgb) {
   float r = srgb.r / 255.0f;
   float g = srgb.g / 255.0f;
   float b = srgb.b / 255.0f;

   auto cmax = MAX(r, MAX(g, b));
   auto cmin = MIN(r, MIN(g, b));
   auto delta = cmax - cmin;

   ColorHSV out = { 0 };
   if (cmax == r) {
      out.h = 60.0f * fmodf((g - b) / delta, 6.0f);
   }
   else if (cmax == g) {
      out.h = 60.0f * ((b - r) / delta + 2);
   }
   else {
      out.h = 60.0f * ((r - g) / delta + 4);
   }

   if (fabs(cmax) > 0.0001f) {
      out.s = delta / cmax;
   }

   out.v = cmax;
   return out;
}

float vDistSquared(Float3 const& a, Float3 const& b) {
   return pow(b.x - a.x, 2) + pow(b.y - a.y, 2) + pow(b.z - a.z, 2);
}

float colorDistance(ColorRGBA c1, ColorRGBA c2) {
   float r, g, b;
   r = GCRGB(c1.r) - GCRGB(c2.r);
   g = GCRGB(c1.g) - GCRGB(c2.g);
   b = GCRGB(c1.b) - GCRGB(c2.b);

   return r * r + g * g + b * b;
}

float colorDistanceLinear(ColorRGBA c1, ColorRGBA c2) {
   return vDistSquared(srgbToLinear(c1), srgbToLinear(c2));
}

ColorRGBA EGAColorLookup(byte c) {
   auto ci = egaGetColor(c);
   ColorRGBA r = { ci.r, ci.g, ci.b, 255 };
   return r;
}

void insertSortedPaletteEntry(ImageColor &color, PaletteColor &parent, byte target, byte current, PaletteEntry &out) {

   out.color = &parent;
   out.distance = sqrt(colorDistance(EGAColorLookup(target), EGAColorLookup(current)));

   auto iter = color.closestColor;
   if (!iter) {
      out.next = out.prev = nullptr;
      color.closestColor = &out;
   }
   else {
      PaletteEntry *prev = nullptr;
      while (iter && out.distance > iter->distance) {
         prev = iter;
         iter = iter->next;
      }

      if (!prev) {
         out.prev = nullptr;
         out.next = color.closestColor;
         out.next->prev = &out;
         color.closestColor = &out;
      }
      else {
         out.next = iter;
         out.prev = prev;
         if (out.next) {
            out.next->prev = &out;
         }

         out.prev->next = &out;
      }
   }
}

bool isClosest(ImageColor &color, PaletteEntry &entry) {
   return color.closestColor == &entry;
}

void removeColorEntries(PaletteColor *removedColor, ImageColor *colors) {
   for (auto& entry : removedColor->entries) {
      for (auto color = colors; color != colors + 64; ++color) {
         if (color->closestColor == &entry) {
            color->closestColor = entry.next;
         }
      }
      if (entry.prev) entry.prev->next = entry.next;
      if (entry.next) entry.next->prev = entry.prev;
   }
}

struct rgbega {
   int rgb;
   byte ega;
   rgbega() {}
   rgbega(int _rgb, byte _ega) :rgb(_rgb), ega(_ega) {}
   bool operator<(int other) {
      return rgb < other;
   }
};

byte closestEGA(int rgb) {
   float lowest = 1000.0;
   int closest = 0;

   for (byte i = 0; i < 64; ++i) {
      auto c = EGAColorLookup(i);

      float diff = colorDistance(*(ColorRGBA*)&rgb, c);

      if (diff < lowest) {
         lowest = diff;
         closest = i;
      }
   }

   return closest;
}

#pragma endregion

EGATexture egaTextureCreateFromTextureEncode(Texture &source, EGAPalette &targetPalette, EGAPalette &resultPalette) {
   int colorCounts[64];
   auto texSize = source.sz;

   auto pixelCount = texSize.x * texSize.y;
   byte* alpha = new byte[pixelCount];
   byte* pixelMap = new byte[pixelCount];
   std::vector<int> cArray(pixelCount);

   memset(resultPalette.colors, 0, 16);
   memset(colorCounts, 0, sizeof(int) * 64);
   memset(alpha, 0, pixelCount);
   memset(pixelMap, 0, pixelCount);

   auto texColors = render::textureGetPixels(source);

   //push every pixel into a vector
   for (int i = 0; i < texSize.x * texSize.y; ++i) {
      alpha[i] = texColors[i].a == 255;
      cArray[i] = *(int*)&texColors[i];
   }

   //sort and unique
   std::sort(begin(cArray), end(cArray));
   cArray.erase(std::unique(cArray.begin(), cArray.end()), cArray.end());

   //map the unique colors to their ega equivalents
   std::vector<rgbega> colorMap(cArray.size());

   for (unsigned int i = 0; i < cArray.size(); ++i)
      colorMap[i] = rgbega(cArray[i], closestEGA(cArray[i]));

   //go throuygh the image and log how often each EGA color appears
   for (int i = 0; i < pixelCount; ++i) {
      int c = *(int*)&texColors[i];

      if (texColors[i].a != 255) {
         pixelMap[i] = 0;
         continue;
      }

      byte ega = std::lower_bound(begin(colorMap), end(colorMap), c)->ega;

      pixelMap[i] = ega;
      colorCounts[ega]++;
   }


   auto p = targetPalette.colors;

   byte forced[64];
   memset(forced, EGA_COLOR_UNUSED, 64);

   byte totalCount = 0;
   for (int i = 0; i < 16; ++i) {
      if (p[i] != EGA_COLOR_UNUSED) {
         if (p[i] != EGA_COLOR_UNDEFINED) {
            forced[p[i]] = totalCount;
         }

         ++totalCount;
      }
   }

   if (!totalCount) {
      return EGATexture();
   }

   std::list<PaletteColor> palette;
   ImageColor colors[64];
   for (int i = 0; i < 64; ++i)
   {
      if (forced[i] != EGA_COLOR_UNUSED) {
         palette.push_back(PaletteColor(i, forced[i]));
      }
      else {
         palette.push_back(PaletteColor(i));
      }

      for (int j = 0; j < 64; ++j)
      {
         insertSortedPaletteEntry(colors[j], palette.back(), j, i, palette.back().entries[j]);
      }
   }

   while (palette.size() > totalCount)
   {
      //worst color, worst error...
      float lowestDistance = FLT_MAX;
      std::list<PaletteColor>::iterator rarestColor;

      for (auto color = palette.begin(); color != palette.end(); ++color) //do this with iterators to erase.
      {
         float distance = 0.0f;
         for (auto& entry : color->entries)
         {
            if (isClosest(colors[color->EGAColor], entry))
            {
               distance += colorCounts[color->EGAColor] * (entry.next->distance - entry.distance);
            }
         }
         if (distance < lowestDistance && color->removable)
         {
            lowestDistance = distance;
            rarestColor = color;
         }
      }

      //remove rarest color, and all of its palette entries in the colors array....
      removeColorEntries(&*rarestColor, colors);
      palette.erase(rarestColor);
   }

   //eliminate unused colors
   for (auto color = palette.begin(); color != palette.end();) {
      if (color->removable) {
         color->distance = 0.0f;

         for (auto& entry : color->entries) {
            if (isClosest(colors[color->EGAColor], entry)) {
               color->distance += colorCounts[color->EGAColor] * (entry.next->distance - entry.distance);
            }
         }

         if (color->distance == 0.0f) {
            //color wasnt used
            removeColorEntries(&*color, colors);
            color = palette.erase(color);
            continue;
         }
      }
      ++color;
   }

   palette.sort([&](const PaletteColor&r1, const PaletteColor&r2) {return r1.distance > r2.distance; });


   //this also gives you the look-up table on output...
   byte paletteOut[16];
   memset(paletteOut, EGA_COLOR_UNUSED, 16);
   int LUTcolor = 0;

   //two passes, first to inster colors who have locked positions in the palette
   for (auto& color : palette) {
      if (!color.removable) {
         paletteOut[color.pPos] = color.EGAColor;
         color.EGAColor = color.pPos;
      }
   }

   //next is to fill in the blanks with the rest
   for (auto& color : palette)
   {
      if (color.removable) {
         while (paletteOut[LUTcolor] != EGA_COLOR_UNUSED) { LUTcolor += 1; };
         paletteOut[LUTcolor] = color.EGAColor;
         color.EGAColor = LUTcolor++;
      }
   }

   byte colorLUT[64]; //look-up table from 64 colors down the 16 remaining colors.
   LUTcolor = 0;
   for (auto& color : colors) {
      colorLUT[LUTcolor++] = color.closestColor->color->EGAColor;
   }

   memcpy(resultPalette.colors, paletteOut, 16);

   auto out = egaTextureCreate(texSize.x, texSize.y);
   egaClearAlpha(out);
   for (int i = 0; i < pixelCount; ++i) {


      if (texColors[i].a == 255) {
         EGAColor c = colorLUT[pixelMap[i]];

         auto x = i % texSize.x;
         auto y = i / texSize.x;

         egaRenderPoint(out, { x, y }, c);
      }
   }

   delete[] alpha;
   delete[] pixelMap;

   return out;
}

// target must exist and must match ega's size, returns !0 on success
int egaTextureDecode(EGATexture &self, Texture& target, EGAPalette &palette) {

   auto texSize = target.sz;
   if (texSize.x != self.sz.x || texSize.y != self.sz.y) {
      return 0;
   }

   if (!target.storedImageData) {
      auto buff = new byte[self.pixels.sz * sizeof(ColorRGBA)];
      target.storedImageData = blobCreate(buff, self.pixels.sz * sizeof(ColorRGBA));
      delete[] buff;
   }

   if (!self.decodePixels) {
      self.decodePixels = new ColorRGBA[self.pixels.sz];
      self.dirty |= Tex_DECODE_DIRTY;
   }

   //palette changed!
   if (memcmp(palette.colors, self.lastDecodedPalette.colors, sizeof(EGAPalette))) {
      self.lastDecodedPalette = palette;
      self.dirty |= Tex_DECODE_DIRTY;
   }

   if (self.dirty&Tex_DECODE_DIRTY) {
      memset(self.decodePixels, 0, self.pixels.sz * sizeof(ColorRGBA));

      for (uint32_t i = 0; i < self.pixels.sz; ++i) {
         auto c = ((byte*)self.pixels.data)[i];
         if (c < EGA_PALETTE_COLORS) {
            ColorRGB rgb = egaGetColor(palette.colors[c]);
            self.decodePixels[i] = ColorRGBA{ rgb.r, rgb.g, rgb.b, 255 };
         }
      }
      self.dirty &= ~Tex_DECODE_DIRTY;
   }

   // copy our pixels into the texture
   memcpy(target.storedImageData.data, self.decodePixels, self.pixels.sz * sizeof(ColorRGBA));
   render::textureRefreshFromBuffer(target);
   return 1;
}

void egaTextureResize(EGATexture &self, uint32_t width, uint32_t height) {
   if (width == self.sz.x && height == self.sz.y) {
      return;
   }

   // copy over to new size if you have anything
   if (self.pixels) {
      auto copyWidth = MIN(width, (uint32_t)self.sz.x);
      auto copyHeight = MIN(height, (uint32_t)self.sz.y);
      auto newPixelCount = width * height;

      auto newPixelData = new byte[newPixelCount];
      memset(newPixelData, EGA_ALPHA, newPixelCount);

      byte *destSL = newPixelData;
      byte *srcSL = (byte*)self.pixels.data;
      for (uint32_t y = 0; y < copyHeight; ++y) {
         memcpy(destSL, srcSL, copyWidth);
         destSL += width;
         srcSL += self.sz.x;
      }

      self.sz.x = width;
      self.sz.y = height;
      self.pixels.sz = newPixelCount;
      delete[] self.pixels.data;
      self.pixels.data = newPixelData;

      if (self.decodePixels) {
         delete[] self.decodePixels;
         self.decodePixels = nullptr;
      }
   }
   else {
      self.sz.x = width;
      self.sz.y = height;
      self.pixels.sz = self.sz.x * self.sz.y;
      self.pixels.data = new byte[self.pixels.sz];
   }

   self.fullRegion = EGARegion{ 0, 0, (int32_t)self.sz.x, (int32_t)self.sz.y };
   self.dirty = Tex_ALL_DIRTY;
}

Int2 egaTextureGetSize(EGATexture const &self) { return self.sz; }
EGARegion *egaTextureGetFullRegion(EGATexture &self) { return &self.fullRegion; }

EGAPColor egaTextureGetColorAt(EGATexture &self, uint32_t x, uint32_t y, EGARegion *vp) {
   if (!vp) { vp = &self.fullRegion; }

   if (x >= (uint32_t)vp->w || y >= (uint32_t)vp->h) {
      return EGA_COLOR_UNDEFINED;
   }

   x += vp->x;
   y += vp->y;

   if (x >= (uint32_t)self.sz.x || y >= (uint32_t)self.sz.y ||
      x < 0 || y < 0) {
      return EGA_COLOR_UNDEFINED;
   }

   auto c = ((byte*)self.pixels.data)[y * self.sz.x + x];
   if (c < EGA_PALETTE_COLORS) {
      return c;
   }

   return EGA_COLOR_UNDEFINED;
}

struct EGAFontFactory {
   EMPTY_STRUCT;
};

struct EGAFont {
   EMPTY_STRUCT;
};

EGAFontFactory *egaFontFactoryCreate(EGATexture &font) {
   return nullptr;
}
void egaFontFactoryDestroy(EGAFontFactory *self) {

}
EGAFont *egaFontFactoryGetFont(EGAFontFactory *self, EGAColor bgColor, EGAColor fgColor) {
   return nullptr;
}

void egaClear(EGATexture &target, EGAPColor color, EGARegion *vp) {
   if (!vp) {
      //fast clear
      memset(target.pixels.data, color, target.pixels.sz);
      target.dirty = Tex_ALL_DIRTY;
   }
   else {
      //region clear is just a rect render on the vp
      egaRenderRect(target, *vp, color);
   }
}
void egaClearAlpha(EGATexture &target) {
   memset(target.pixels.data, EGA_ALPHA, target.pixels.sz);
   target.dirty = Tex_ALL_DIRTY;
}

static void _renderTextureEX(EGATexture &dest, EGATexture &src, Recti const& srcRect, Int2 const& destPos) {
   byte *srcPixels = (byte*)src.pixels.data + (srcRect.y * src.sz.x + srcRect.x);
   byte *destPixels = (byte*)dest.pixels.data + (destPos.y * dest.sz.x + destPos.x);

   for (int y = 0; y < srcRect.h; ++y) {
      byte *srcSL = srcPixels;
      byte *destSL = destPixels;

      for (int x = 0; x < srcRect.w; ++x) {
         if (*srcSL < EGA_PALETTE_COLORS) {
            *destSL = *srcSL;
         }
         ++srcSL; ++destSL;
      }
      srcPixels += src.sz.x;
      destPixels += dest.sz.x;
   }
   dest.dirty = Tex_ALL_DIRTY;
}

void egaColorReplace(EGATexture &target, EGAPColor oldColor, EGAPColor newColor) {
   for (uint32_t i = 0; i < target.pixels.sz; ++i) {
      if (((byte*)target.pixels.data)[i] == oldColor) {
         ((byte*)target.pixels.data)[i] = newColor;
      }
   }
   target.dirty = Tex_ALL_DIRTY;
}

void egaRenderTexture(EGATexture &target, Int2 pos, EGATexture &tex, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }

   Int2 offsetPos = { pos.x + vp->x, pos.y + vp->y };

   if (offsetPos.x >= vp->w || offsetPos.y >= vp->h ||
      offsetPos.x < -(int32_t)tex.sz.x || offsetPos.y < -(int32_t)tex.sz.y) {
      //outside bounds, return
      return;
   }

   Recti srcRect = { 0 };
   srcRect.x = offsetPos.x < 0 ? -offsetPos.x : 0;
   srcRect.y = offsetPos.y < 0 ? -offsetPos.y : 0;
   srcRect.w = MIN(vp->w, MIN(vp->w - offsetPos.x, (int32_t)tex.sz.x - srcRect.x));
   srcRect.h = MIN(vp->h, MIN(vp->h - offsetPos.y, (int32_t)tex.sz.y - srcRect.y));

   Int2 destPos = { 0 };
   destPos.x = offsetPos.x < 0 ? 0 : pos.x;
   destPos.y = offsetPos.y < 0 ? 0 : pos.y;

   _renderTextureEX(target, tex, srcRect, destPos);
}
void egaRenderTexturePartial(EGATexture &target, Int2 pos, EGATexture &tex, Recti uv, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }

   Int2 offsetPos = { pos.x + vp->x, pos.y + vp->y };

   if (offsetPos.x >= vp->w || offsetPos.y >= vp->h ||
      offsetPos.x < -(int32_t)uv.w || offsetPos.y < -(int32_t)uv.h) {
      //outside bounds, return
      return;
   }

   Recti srcRect = { 0 };
   srcRect.x = pos.x < 0 ? uv.x - pos.x : uv.x;
   srcRect.y = pos.y < 0 ? uv.y - pos.y : uv.y;

   srcRect.w = MIN(vp->w, MIN(uv.w, uv.w - (srcRect.x - uv.x)));
   srcRect.h = MIN(vp->h, MIN(uv.h, uv.h - (srcRect.y - uv.y)));

   Int2 destPos = { 0 };
   destPos.x = offsetPos.x < 0 ? 0 : pos.x;
   destPos.y = offsetPos.y < 0 ? 0 : pos.y;

   _renderTextureEX(target, tex, srcRect, destPos);
}
void egaRenderPoint(EGATexture &target, Int2 pos, EGAPColor color, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }

   if (pos.x >= vp->w || pos.y >= vp->h) {
      return;
   }

   pos.x += vp->x;
   pos.y += vp->y;

   if (pos.x >= (int32_t)target.sz.x || pos.y >= (int32_t)target.sz.y ||
      pos.x < 0 || pos.y < 0) {
      return;
   }

   ((byte*)target.pixels.data)[pos.y * target.sz.x + pos.x] = color;
   target.dirty = Tex_ALL_DIRTY;
}
void egaRenderLine(EGATexture &target, Int2 pos1, Int2 pos2, EGAPColor color, EGARegion *vp) {
   int dx = abs(pos2.x - pos1.x);
   int dy = abs(pos2.y - pos1.y);
   int x0, x1, y0, y1;
   float x, y, slope;

   //len=0
   if (!dx && !dy) {
      //TODO: not sure if i want to do this? line size (0,0) draws a point?
      egaRenderPoint(target, pos1, color, vp);
      return;
   }

   if (dx > dy) {
      if (pos1.x > pos2.x) {//flip
         x0 = pos2.x; y0 = pos2.y;
         x1 = pos1.x; y1 = pos1.y;
      }
      else {
         x0 = pos1.x; y0 = pos1.y;
         x1 = pos2.x; y1 = pos2.y;
      }

      x = (float)x0;
      y = (float)y0;
      slope = (float)(y1 - y0) / (float)(x1 - x0);

      while (x < x1) {
         egaRenderPoint(target, { (int32_t)x, (int32_t)y }, color, vp);

         x += 1.0f;
         y += slope;
      }

      egaRenderPoint(target, { (int32_t)x1, (int32_t)y1 }, color, vp);
   }
   else {
      if (pos1.y > pos2.y) {//flip
         x0 = pos2.x; y0 = pos2.y;
         x1 = pos1.x; y1 = pos1.y;
      }
      else {
         x0 = pos1.x; y0 = pos1.y;
         x1 = pos2.x; y1 = pos2.y;
      }

      x = (float)x0;
      y = (float)y0;
      slope = (float)(x1 - x0) / (float)(y1 - y);

      while (y < y1) {

         egaRenderPoint(target, { (int32_t)x, (int32_t)y }, color, vp);

         y += 1.0f;
         x += slope;
      }

      egaRenderPoint(target, { (int32_t)x1, (int32_t)y1 }, color, vp);
   }
}
void egaRenderLineRect(EGATexture &target, Recti r, EGAPColor color, EGARegion *vp) {
   egaRenderLine(target, { r.x, r.y }, { r.x + r.w - 1, r.y }, color, vp);
   egaRenderLine(target, { r.x + r.w - 1, r.y }, { r.x + r.w - 1, r.y + r.h - 1 }, color, vp);
   egaRenderLine(target, { r.x, r.y + r.h - 1 }, { r.x + r.w - 1, r.y + r.h - 1 }, color, vp);
   egaRenderLine(target, { r.x, r.y }, { r.x, r.y + r.h - 1 }, color, vp);
}
void egaRenderRect(EGATexture &target, Recti r, EGAPColor color, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }

   if (r.x >= vp->w || r.y >= vp->h ||
      r.x < -r.w || r.y < -r.h) {
      return;
   }

   Recti drawRect = { 0 };
   drawRect.x = r.x < 0 ? vp->x : r.x + vp->x;
   drawRect.y = r.y < 0 ? vp->y : r.y + vp->y;
   drawRect.w = r.x < 0 ? MIN(vp->w, r.w + r.x) : MIN(r.w, vp->w - r.x);
   drawRect.h = r.y < 0 ? MIN(vp->h, r.h + r.y) : MIN(r.h, vp->h - r.y);

   byte *destPixels = (byte*)target.pixels.data + (drawRect.y * target.sz.x + drawRect.x);
   for (int y = 0; y < drawRect.h; ++y) {
      memset(destPixels, color, drawRect.w);
      destPixels += target.sz.x;
   }
   target.dirty = Tex_ALL_DIRTY;
}

void egaRenderCircle(EGATexture &target, Int2 pos, int radius, EGAPColor color, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }
}
void egaRenderEllipse(EGATexture &target, Recti r, EGAPColor color, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }
}
void egaRenderEllipseQB(EGATexture &target, Int2 pos, int radius, double aspect, EGAPColor color, EGARegion *vp) {
   if (!vp) { vp = &target.fullRegion; }
}

void egaRenderTextSingleChar(EGATexture &target, const char c, Int2 pos, EGAFont *font, int spaces) {

}
void egaRenderText(EGATexture &target, const char *text, Int2 pos, EGAFont *font) {

}
void egaRenderTextWithoutSpaces(EGATexture &target, const char *text, Int2 pos, EGAFont *font) {

}