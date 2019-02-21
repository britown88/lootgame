#include "game.h"



static void _renderSwing(Dude&dude) {
   auto origin = dude.phy.pos + dude.atk.weaponVector * dude.phy.circle.size;

   auto model = Matrix::identity();
   model *= Matrix::translate2f(origin);
   model *= Matrix::rotate2D(v2Angle(dude.atk.weaponVector));
   model *= Matrix::translate2f({ dude.atk.swing.hitbox.x, dude.atk.swing.hitbox.y });
   model *= Matrix::scale2f({ dude.atk.swing.hitbox.w, dude.atk.swing.hitbox.h });

   uber::resetToDefault();
   uber::set(Uniform_TransformNormals, true);
   uber::set(Uniform_NormalTransform,
      Matrix::rotate2D(v2Angle({ dude.atk.weaponVector.x, -dude.atk.weaponVector.y }))
   );
   uber::set(Uniform_ModelMatrix, model);
   uber::set(Uniform_Height, Const.dudeHeight);
   uber::bindTexture(Uniform_DiffuseTexture, Textures[GameTextures_ShittySword].handle);
   uber::bindTexture(Uniform_NormalsTexture, Textures[GameTextures_SwordNormals].handle);

   render::meshRender(Graphics.meshUncentered);
}

static void _renderDude(GameState& g, Dude& dude) {
   auto model = Matrix::identity();

   Float2 rotate = dude.mv.facing;
   if (dude.state == DudeState_ATTACKING) {
      rotate += dude.atk.weaponVector;
      rotate /= 2.0f;
   }

   rotate = v2Normalized(rotate);

   model *= Matrix::translate2f(dude.phy.pos);
   model *= Matrix::rotate2D(v2Angle(rotate));
   model *= Matrix::scale2f(dude.renderSize);


   uber::resetToDefault();
   uber::set(Uniform_Height, Const.dudeHeight);

   if (dudeAlive(dude)) {
      uber::set(Uniform_Color, dude.c);
   }
   else {
      uber::set(Uniform_Color, DkRed);
   }

   // rotate the normals map with our texture
   uber::set(Uniform_TransformNormals, true);
   uber::set(Uniform_NormalTransform, Matrix::rotate2D(v2Angle({ rotate.x, -rotate.y })));

   uber::set(Uniform_ModelMatrix, model);
   uber::bindTexture(Uniform_DiffuseTexture, dude.texture);
   uber::bindTexture(Uniform_NormalsTexture, Textures[GameTextures_DudeNormals].handle);
   render::meshRender(Graphics.mesh);

   //uber::set(Uniform_OutlineOnly, true);
   //render::meshRender(g_game->mesh);

   if (dude.state == DudeState_ATTACKING) {
      _renderSwing(dude);
   }
}

static void _renderDudeCollision(GameState& g, Dude& dude) {
   auto& vp = g.camera.viewport;

   auto model = Matrix::identity();
   
   model = Matrix::translate2f(dude.phy.pos - Float2{ (float)vp.x, (float)vp.y });
   model *= Matrix::scale2f({ dude.phy.circle.size * 2, dude.phy.circle.size * 2 });
   uber::set(Uniform_Color, DkGreen);
   uber::set(Uniform_Alpha, 0.25f);
   uber::set(Uniform_ModelMatrix, model);

   uber::bindTexture(Uniform_DiffuseTexture, Textures[GameTextures_Circle].handle);
   render::meshRender(Graphics.mesh);
}



static void _renderTarget(Float2 pos, ColorRGBAf color) {
   auto model = Matrix::identity();

   auto sz = Textures[GameTextures_Target].sz;

   model *= Matrix::translate2f(pos);
   model *= Matrix::scale2f({ (float)sz.x, (float)sz.y });

   uber::resetToDefault();
   uber::set(Uniform_Color, color);
   uber::set(Uniform_ModelMatrix, model);
   uber::bindTexture(Uniform_DiffuseTexture, Textures[GameTextures_Target].handle);
   render::meshRender(Graphics.mesh);
}

static void _renderFloor(GameState& game) {
   auto& c = Const;

   Float2 fres = { game.map.size.x, game.map.size.y };
   Float2 tileSize = { 16, 16 };

   uber::resetToDefault();
   uber::set(Uniform_Height, Const.floorHeight);
   uber::set(Uniform_TextureMatrix, Matrix::scale2f({ fres.x / tileSize.x, fres.y / tileSize.y }));
   uber::set(Uniform_ModelMatrix, Matrix::scale2f(fres));
   uber::bindTexture(Uniform_DiffuseTexture, Textures[GameTextures_Tile].handle);
   uber::bindTexture(Uniform_NormalsTexture, Textures[GameTextures_TileNormals].handle);
   //uber::set(Uniform_ColorOnly, true);

   render::meshRender(Graphics.meshUncentered);

   //if (gameDataGet()->imgui.showCollisionDebugging) {
   //   Rectf testrect = { 300,300, 500, 200 };

   //   uber::set(Uniform_ColorOnly, true);
   //   uber::set(Uniform_Color, circleVsAabb(game->maindude.phy.pos, game->maindude.phy.circle.size, testrect) ? Red : Cyan);
   //   uber::set(Uniform_ModelMatrix, Matrix::translate2f({ testrect.x, testrect.y }) * Matrix::scale2f({ testrect.w, testrect.h }));
   //   render::meshRender(g_game->meshUncentered);
   //}
}


/*
Render Pipe

FBO: UnlitScene[2] = {Diffuse, Normals}
BlendMode(DISABLED)
DiscardAlpha
OutputNormals
Bind uDiffuse
Bind uNormals
Set uHeight
Render Floor, Dudes, etc

FBO: Lights[1] = {Diffuse}
BlendMode(ADDITIVE)
Bind UnlitScene.Normals as uNormals
PointLight
NormalLighting
Set uHeight
Clear(AmbientLight)
Render Each Light

FBO: LitScene[1] = {Diffuse}
BlendMode(DISABLED)
Bind UnlitScene.Diffuse as uDiffuse
Render UnlitScene

BlendMode(MULTIPLY)
Bind Lights.Diffuse as uDiffuse
Render Lights

(can now re-use Lights to draw a vignette mask and draw it over with multiply)

FBO: Output[1] = {Diffuse}
BlendMode(ALPHA_BLENDING)
Clear(Black)
Render LitScene
Render UI

*/


void renderUnlitScene(GameState& game) {
   auto vp = game.camera.viewport;
   auto view = Matrix::ortho(vp.x, vp.x + vp.w, vp.y, vp.y + vp.h, 1, -1);

   render::fboBind(Graphics.unlitScene);
   uber::set(Uniform_ViewMatrix, view, true);
   uber::set(Uniform_DiscardAlpha, true, true);
   uber::set(Uniform_OutputNormals, true, true);
   uber::resetToDefault();

   render::setBlendMode(BlendMode_DISABLED);
   render::clear(Black);

   _renderFloor(game);

   for (auto&& d : game.baddudes) {
      _renderDude(game, d);
   }
   _renderDude(game, game.maindude);

   // restore these flags
   uber::set(Uniform_DiscardAlpha, false, true);
   uber::set(Uniform_OutputNormals, false, true);
}

static void _addLight(float size, VPCoords pos, ColorRGBAf c) {
   uber::set(Uniform_Color, c);
   uber::set(Uniform_Alpha, 1.0f);
   //uber::set(Uniform_PointLightRadius, size/2.0f);
   uber::set(Uniform_LightAttrs, Float3{
      Const.lightLinearPortion,
      Const.lightSmoothingFactor,
      Const.lightIntensity });

   //uber::set(Uniform_LightIntensity, 1.0f);
   uber::set(Uniform_ModelMatrix, Matrix::translate2f(pos) * Matrix::scale2f({ size, size }));
   render::meshRender(Graphics.mesh);
}

void renderLightLayer(GameState& game) {
   auto vp = game.camera.viewport;
   auto al = game.DEBUG.ambientLight;

   render::fboBind(Graphics.lightLayer);
   uber::set(Uniform_ViewMatrix, Matrix::ortho(0, vp.w, 0, vp.h, 1, -1), true);
   uber::resetToDefault();

   render::setBlendMode(BlendMode_ADDITION);
   render::clear({ al, al, al, al });

   // bind deferred normals map
   uber::bindTexture(Uniform_NormalsTexture, Graphics.unlitScene.out[1].handle);
   uber::set(Uniform_PointLight, true);
   uber::set(Uniform_NormalLighting, true);

   uber::set(Uniform_Height, Const.lightHeight);


   //uber::set(Uniform_LightIntensity, cLightIntensity);

   //_addLight({ vp.w,vp.w }, { vp.w / 2.0f, vp.h / 2.0f }, Yellow);
   //_addLight({ 120, 120 }, game->maindude.phy.pos - Float2{ vp.x, vp.y }, Yellow);

   _addLight(150, Float2{ 200, 200 } -Float2{ vp.x, vp.y }, White);

   int i = 0;
   for (auto&& d : game.baddudes) {
      static ColorRGBAf c[] = { Red, Green, Blue };
      //_addLight(80, d.phy.pos - Float2{ vp.x, vp.y }, c[i++ % 3]);
   }
   auto candleColor = sRgbToLinear(ColorRGB{ 255,147,41 });
   _addLight(150, game.io.mousePos.toViewport(game), White);
}

void renderLitScene(GameState& game) {
   auto vp = game.camera.viewport;

   render::fboBind(Graphics.litScene);
   uber::resetToDefault();
   render::clear(Black);

   render::setBlendMode(BlendMode_DISABLED);
   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (float)vp.w, (float)vp.h }));
   uber::bindTexture(Uniform_DiffuseTexture, Graphics.unlitScene.out[0].handle);
   render::meshRender(Graphics.meshUncentered);

   render::setBlendMode(BlendMode_MULITPLY);
   uber::bindTexture(Uniform_DiffuseTexture, Graphics.lightLayer.out[0].handle);
   render::meshRender(Graphics.meshUncentered);


}

void renderUI(GameState& game) {
   auto& vp = game.camera.viewport;

   render::fboBind(Graphics.UI);

   uber::resetToDefault();
   render::clear(Cleared);

   render::setBlendMode(BlendMode_NORMAL);

   /*if (game->maindude.stamina < game->maindude.staminaMax)*/ {
      auto tsz = Textures[GameTextures_GemFilled].sz;
      Float2 gemSize = { (float)tsz.x, (float)tsz.y };
      float gemSpace = 0.0f;
      auto w = (gemSize.x + gemSpace) * game.maindude.status.staminaMax;

      //Float2 staminaCorner = { game->maindude.phy.pos.x - w / 2.0f, game->maindude.phy.pos.y + game->maindude.phy.circle.size + 20 };
      Float2 staminaCorner = { 10,10 };

      if (game.maindude.status.stamina == 0) {
         uber::set(Uniform_Alpha, 1.0f);
         uber::set(Uniform_Color, Red);
      }
      for (int i = 0; i < game.maindude.status.staminaMax; ++i) {

         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         uber::bindTexture(Uniform_DiffuseTexture, Textures[i < game.maindude.status.stamina ? GameTextures_GemFilled : GameTextures_GemEmpty].handle);
         render::meshRender(Graphics.meshUncentered);

         staminaCorner.x += gemSize.x + gemSpace;
      }

      uber::resetToDefault();
      if (game.maindude.status.stamina != 0) {
         uber::set(Uniform_Alpha, 0.5f);
         //uber::set(Uniform_Color, Red);
      }

      for (int i = 0; i < game.maindude.status.healthMax; ++i) {

         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         uber::bindTexture(Uniform_DiffuseTexture, Textures[i < game.maindude.status.health ? GameTextures_HeartFilled : GameTextures_HeartEmpty].handle);
         render::meshRender(Graphics.meshUncentered);

         staminaCorner.x += gemSize.x + gemSpace;
      }
   }

   if (game.DEBUG.showCollisionDebugging) {
      for (auto&& d : game.baddudes) {
         _renderDudeCollision(game, d);
      }
      _renderDudeCollision(game, game.maindude);
      
   }


   if (game.DEBUG.showMovementDebugging) {
      const static float moveTargetDist = 50.0f;
      const static float aimTargetDist = 100.0f;
      auto &io = game.io;
      auto &dude = game.maindude;

      auto pos = dude.phy.pos - Float2{ (float)vp.x, (float)vp.y };

      _renderTarget(pos + dude.phy.velocity * 100, Cyan);
      _renderTarget(pos + v2Normalized(dude.mv.facing) * aimTargetDist, Red);

      _renderTarget(pos + io.leftStick * moveTargetDist, DkGreen);
      _renderTarget(pos + dude.mv.moveVector * 10, Magenta);

      //_renderTarget(pos + io.rightStick * aimTargetDist, Yellow);
      _renderTarget(pos + dude.mv.faceVector * aimTargetDist, LtGray);
   }
}

static void renderOutput(GameState& game, FBO& output) {
   auto& vp = game.camera.viewport;

   render::fboBind(output);

   uber::resetToDefault();
   render::clear(Black);

   render::setBlendMode(BlendMode_NORMAL);
   uber::set(Uniform_ModelMatrix, Matrix::scale2f({ (float)vp.w, (float)vp.h }));

   uber::bindTexture(Uniform_DiffuseTexture, Graphics.litScene.out[0].handle);
   render::meshRender(Graphics.meshUncentered);

   uber::bindTexture(Uniform_DiffuseTexture, Graphics.UI.out[0].handle);
   render::meshRender(Graphics.meshUncentered);

   if (game.mode.type == ModeType_YOUDIED) {
      render::setBlendMode(BlendMode_MULITPLY);
      auto ratio = cosInterp(1.0f - ((float)game.mode.clock / 3000));
      uber::set(Uniform_Color, ColorRGBAf{ 1.0f, ratio, ratio, 1.0f });
      uber::set(Uniform_ColorOnly, true);
      render::meshRender(Graphics.meshUncentered);
   }
}

void gameDraw(GameState& g, FBO& output) {
   auto& c = Const;

   render::shaderSetActive(Graphics.shader);
   uber::resetToDefault(true);

   renderUnlitScene(g);
   renderLightLayer(g);
   renderLitScene(g);
   renderUI(g);

   renderOutput(g, output);

   render::fboBind({});
}
