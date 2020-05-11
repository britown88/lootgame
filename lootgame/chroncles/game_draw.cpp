#include "stdafx.h"

#include "game.h"

static void _renderToEga(GameInstance& gi) {
   //egaClear(gi.ega, 0);

   //egaRenderLine(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, rand()%EGA_PALETTE_COLORS);

   //egaRenderPoint(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, rand() % EGA_PALETTE_COLORS);
   //egaRenderLineRect(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y, (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, rand() % EGA_PALETTE_COLORS);

   egaRenderCircle(gi.ega, { (int32_t)rand() % gi.ega.sz.x, (int32_t)rand() % gi.ega.sz.y }, (int32_t)rand() % gi.ega.sz.x + 1, rand() % EGA_PALETTE_COLORS);
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
