#include "stdafx.h"

#include "game.h"

static void _renderToEga(GameInstance& gi) {
   //egaClear(gi.ega, 0);

   auto w = EGA_RES_WIDTH / 16;
   for(int x = 0; x < 16; ++x){
      //egaRenderRect(gi.ega, { x * w, 0, w, 16 }, x);
   }

   static Int2 cursor = { 0,0 };
   static Int2 cursorVec = {1,1 };

   for (int i = 0; i < 5000; ++i) {
      cursor.x += cursorVec.x;
      cursor.y += cursorVec.y;
      if (cursor.x < 0) { cursor.x = 0; cursorVec.x = 1; }
      if (cursor.y < 0) { cursor.y = 0; cursorVec.y = 1; }
      if (cursor.x >= EGA_RES_WIDTH) { cursorVec.x = -1; }
      if (cursor.y >= EGA_RES_HEIGHT) { cursorVec.y = -1; }

      egaRenderPoint(gi.ega, cursor, cursor.x % EGA_PALETTE_COLORS);
      egaRenderPoint(gi.ega, {cursor.x, cursor.y+1}, EGA_PALETTE_COLORS);
   }
   

   


   //egaRenderLine(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, rand()%EGA_PALETTE_COLORS);

   //egaRenderPoint(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, rand() % EGA_PALETTE_COLORS);
   //egaRenderLineRect(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y, (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, rand() % EGA_PALETTE_COLORS);

   //egaRenderCircle(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, (int32_t)rand() % gi.ega.sz.x + 1, rand() % EGA_PALETTE_COLORS);
}

static void _renderOutput(GameInstance& gi) {
   Rectf vp = { 0, 0, EGA_RES_WIDTH, EGA_RES_HEIGHT };

   render::fboBind(gi.outputFbo);

   uber::resetToDefault();
   render::clear(Black);

   render::setBlendMode(BlendMode_NORMAL);
   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (float)vp.w, (float)vp.h }));
   uber::set(Uniform_ViewMatrix, Matrix::ortho(0, vp.w, 0, vp.h, 1, -1));

   uber::bindTexture(Uniform_DiffuseTexture, gi.egaOutputTexture.handle);
   render::meshRender(Graphics.meshUncentered);
}

void gameDraw(GameInstance& gi) {
   auto& c = Const;

   // render gamestate to the ega buffer
   _renderToEga(gi);

   // output the ega buffer to an rgba texture
   egaTextureDecode(gi.ega, gi.egaOutputTexture, gi.state.palette);

   // render the texture to the output fbo
   render::shaderSetActive(Graphics.shader);
   uber::resetToDefault(true);
   _renderOutput(gi);

   render::fboBind({});
}
