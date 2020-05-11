#pragma once

#include "defs.h"
#include "math.h"
#include "render.h"

// These constants are EGA-Hardware limits depending on render mode
// They're mostly here for reference but most things won't be hardcoded with it
#define EGA_RES_WIDTH 320
#define EGA_RES_HEIGHT 200
#define EGA_TEXT_CHAR_WIDTH 8
#define EGA_TEXT_CHAR_HEIGHT 8
#define EGA_PIXEL_HEIGHT 1.2f
#define EGA_PIXEL_WIDTH 1.00f

#define EGA_COLORS 64
#define EGA_COLOR_UNDEFINED (EGA_COLORS) // used for palette generation, marks a color entry as able to be anything
#define EGA_COLOR_UNUSED (EGA_COLORS + 1) //usedf for palette generation, marks a color entry as not usable
#define EGA_PALETTE_COLORS 16
#define EGA_TEXT_RES_WIDTH (EGA_RES_WIDTH / EGA_TEXT_CHAR_WIDTH)
#define EGA_TEXT_RES_HEIGHT (EGA_RES_HEIGHT / EGA_TEXT_CHAR_HEIGHT)
#define EGA_PIXELS (EGA_RES_WIDTH * EGA_RES_HEIGHT)
#define EGA_BYTES (EGA_PIXELS / 8)
#define EGA_PLANES 4

// byte value for alpha
#define EGA_ALPHA 0xFF

// EGA Supports one of 64 colors
typedef byte EGAColor;
typedef byte EGAPColor; //(0-15)

// @reflect{
struct EGAPalette {
   // @readonly
   Symbol* id = nullptr;

   EGAColor colors[EGA_PALETTE_COLORS];

   // @ignore{
   bool markForDelete = false; // }
}; // }

void egaStartup();

// Takes an EGA color index (0-63) and returns the 3-byte RGB
// useful externally for reference graphics
extern ColorRGB g_egaToRGBTable[64];
#define egaGetColor(c) g_egaToRGBTable[c]

enum Tex_ {
   Tex_DECODE_DIRTY = (1 << 0),
   Tex_OFFSET_DIRTY = (1 << 1),
   Tex_ALL_DIRTY = (Tex_DECODE_DIRTY | Tex_OFFSET_DIRTY)
};
typedef byte TexCleanFlag;

// EGARegions are passed to all draw calls
// They define an origin and clipping rect for the draw
typedef Recti EGARegion;

// @reflect{
struct EGATexture {
   // @readonly
   Symbol* id = nullptr;
   Blob pixels;

   // @readonly
   Int2 sz = { 0, 0 };

   // @ignore{
   EGARegion fullRegion = { 0 };
   ColorRGBA *decodePixels = nullptr;
   EGAPalette lastDecodedPalette = { 0 };

   TexCleanFlag dirty = Tex_ALL_DIRTY;
   bool markForDelete = false;
   //}
}; // }

EGATexture egaTextureCreate(uint32_t width, uint32_t height);
EGATexture egaTextureCreateCopy(EGATexture const &other);
void egaTextureDestroyContent(EGATexture &self);
void egaTexturePostLoad(EGATexture &self); // call after deserialize to make sure the state is good

// encoding and decoding from an rgb texture
EGATexture egaTextureCreateFromTextureEncode(Texture &sourceTexture, EGAPalette &targetPalette, EGAPalette &resultPalette);

// target must exist and must match ega's size, returns !0 on success
int egaTextureDecode(EGATexture &self, Texture& target, EGAPalette &palette);

Int2 egaTextureGetSize(EGATexture const &self);

void egaTextureResize(EGATexture &self, uint32_t width, uint32_t height);



// useful in certian circumstances, dont use this for normal calls
// passing NULL to a render call will use this for the target automatically
EGARegion *egaTextureGetFullRegion(EGATexture &self);

EGAPColor egaTextureGetColorAt(EGATexture &self, uint32_t x, uint32_t y, EGARegion *vp = nullptr);


// The font factory manages fonts, theres only one "font" in EGA
// Font in this case means color, background/foreground
typedef struct EGAFontFactory EGAFontFactory;
typedef struct EGAFont EGAFont;

/*
Image must be:
- 256x112 with 256 8x14 characters organized according to ascii
- solid 1 alpha (no transparency)
- 2-color palette; 0 for background and 1 for foreground
*/
EGAFontFactory *egaFontFactoryCreate(EGATexture &font);
void egaFontFactoryDestroy(EGAFontFactory *self);
EGAFont *egaFontFactoryGetFont(EGAFontFactory *self, EGAColor bgColor, EGAColor fgColor);


void egaClear(EGATexture &target, EGAPColor color, EGARegion *vp = nullptr);
void egaClearAlpha(EGATexture &target);
void egaColorReplace(EGATexture &target, EGAPColor oldCOlor, EGAPColor newColor);
void egaRenderTexture(EGATexture &target, Int2 pos, EGATexture &tex, EGARegion *vp = nullptr);
void egaRenderTexturePartial(EGATexture &target, Int2 pos, EGATexture &tex, Recti uv, EGARegion *vp = nullptr);
void egaRenderPoint(EGATexture &target, Int2 pos, EGAPColor color, EGARegion *vp = nullptr);
void egaRenderLine(EGATexture &target, Int2 pos1, Int2 pos2, EGAPColor color, EGARegion *vp = nullptr);
void egaRenderLineRect(EGATexture &target, Recti r, EGAPColor color, EGARegion *vp = nullptr);
void egaRenderRect(EGATexture &target, Recti r, EGAPColor color, EGARegion *vp = nullptr);

void egaRenderCircle(EGATexture &target, Int2 pos, int radius, EGAPColor color, EGARegion *vp = nullptr);
void egaRenderEllipse(EGATexture &target, Recti r, EGAPColor color, EGARegion *vp = nullptr);
void egaRenderEllipseQB(EGATexture &target, Int2 pos, int radius, double aspect, EGAPColor color, EGARegion *vp = nullptr);

void egaRenderTextSingleChar(EGATexture &target, const char c, Int2 pos, EGAFont *font, int spaces);
void egaRenderText(EGATexture &target, const char *text, Int2 pos, EGAFont *font);
void egaRenderTextWithoutSpaces(EGATexture &target, const char *text, Int2 pos, EGAFont *font);
