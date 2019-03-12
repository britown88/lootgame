#include "stdafx.h"

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
      

   uber::bindTexture(Uniform_DiffuseTexture, TextureMap.map[intern("ShittySword")].handle);
   uber::bindTexture(Uniform_NormalsTexture, TextureMap.map[intern("SwordNormals")].handle);

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
   uber::bindTexture(Uniform_DiffuseTexture, dude.texture->handle);   
   uber::bindTexture(Uniform_NormalsTexture, TextureMap.map[intern("DudeNormals")].handle);
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
   
   uber::bindTexture(Uniform_DiffuseTexture, TextureMap.map[intern("Circle")].handle);
   render::meshRender(Graphics.mesh);
}



static void _renderTarget(Float2 pos, ColorRGBAf color) {
   auto model = Matrix::identity();

   auto& t = TextureMap.map[intern("Target")];

   auto sz = t.sz;

   model *= Matrix::translate2f(pos);
   model *= Matrix::scale2f({ (float)sz.x, (float)sz.y });

   uber::resetToDefault();
   uber::set(Uniform_Color, color);
   uber::set(Uniform_ModelMatrix, model);
   uber::bindTexture(Uniform_DiffuseTexture, t.handle);
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

   uber::bindTexture(Uniform_DiffuseTexture, TextureMap.map[intern("Tile")].handle);
   uber::bindTexture(Uniform_NormalsTexture, TextureMap.map[intern("TileNormals")].handle);
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

static void _addLight(GameState& g, float size, Coords pos, ColorRGBAf c, Array<ConvexPoly>& blockers) {
   uber::set(Uniform_Color, c);
   
   //uber::set(Uniform_PointLightRadius, size/2.0f);
   uber::set(Uniform_LightAttrs, Float3{
      Const.lightLinearPortion,
      Const.lightSmoothingFactor,
      Const.lightIntensity });


   auto p = pos.toWorld();
   auto vertices = render::buildShadowsBuffer(p, size *0.5f, blockers);   
   auto origin = p - Float2{ size*0.5f, size*0.5f };
   
   Array<Vertex> mesh;
   mesh.reserve(vertices.size());
   for (auto&& v : vertices) {
      Float2 pos = v - origin;
      Float2 tex = pos / size;
      mesh.push_back({ Coords::fromWorld(v).toViewport(g), tex });
   }


   //uber::set(Uniform_Alpha, 0.5f);
   //uber::set(Uniform_ModelMatrix, Matrix::translate2f(pos.toViewport(g)) * Matrix::scale2f({ size, size }));
   //render::meshRender(Graphics.mesh);

   auto mHandle = render::meshBuild(mesh.data(), mesh.size());

   uber::set(Uniform_Alpha, 1.0f);
   uber::set(Uniform_ModelMatrix, Matrix::identity());
   render::meshRender(mHandle);

   render::meshDestroy(mHandle);
}

static void _renderLight(GameState&g, Light& light) {
   uber::set(Uniform_Color, light.color);
   uber::set(Uniform_Alpha, 1.0f);

   uber::set(Uniform_LightAttrs, Float3{
      light.linearPortion,
      light.smoothingFactor,
      light.intensity });

   uber::set(Uniform_ModelMatrix, Matrix::translate2f(light.pos.toViewport(g)) * Matrix::scale2f({ light.radius * 2, light.radius * 2 }));
   render::meshRender(Graphics.mesh);
}

void renderLightLayer(GameState& game) {

   Array<ConvexPoly> blockers;
   for (auto w : game.map.walls) {
      if (w.poly.points.size() >= 3) {
         blockers.push_back(w.poly);
      }
   }

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
   auto candleColor = sRgbToLinear(ColorRGB{ 255,147,41 });

   //_addLight({ vp.w,vp.w }, { vp.w / 2.0f, vp.h / 2.0f }, Yellow);
   //_addLight(game, 200, Coords::fromWorld(game.maindude.phy.pos), candleColor, blockers);

   _addLight(game, 150, Coords::fromWorld({ 200, 200 }), candleColor, blockers);

   int i = 0;
   for (auto&& d : game.baddudes) {
      static ColorRGBAf c[] = { Red, Green, Blue };
      _addLight(game, 80, Coords::fromWorld(d.phy.pos), candleColor, blockers);
   }

   for (auto&&light : game.map.lights) {
      _renderLight(game, light);
   }
   if (game.ui.editing && game.ui.mode == GameEditMode_Lights) {
      _renderLight(game, game.ui.editingLight);
   }
   
   //_addLight(game, 300, game.io.mousePos, White, blockers);
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

   Float2 fbosz = { (float)Const.vpSize.x, (float)Const.vpSize.y };
   uber::set(Uniform_ViewMatrix, Matrix::ortho(0, fbosz.x, 0, fbosz.y, 1, -1));

   /*if (game->maindude.stamina < game->maindude.staminaMax)*/ {
      auto &tfilled = TextureMap.map[intern("GemFilled")];
      auto &tempty = TextureMap.map[intern("GemEmpty")];

      auto &thfilled = TextureMap.map[intern("HeartFilled")];
      auto &thempty = TextureMap.map[intern("HeartEmpty")];

      auto tsz = tfilled.sz;
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
         uber::bindTexture(Uniform_DiffuseTexture, i < game.maindude.status.stamina ? tfilled.handle : tempty.handle);
         render::meshRender(Graphics.meshUncentered);

         staminaCorner.x += gemSize.x + gemSpace;
      }

      //uber::resetToDefault();
      //uber::set(Uniform_ViewMatrix, Matrix::ortho(0, fbosz.x, 0, fbosz.y, 1, -1));

      if (game.maindude.status.stamina != 0) {
         uber::set(Uniform_Alpha, 0.5f);
         //uber::set(Uniform_Color, Red);
      }

      for (int i = 0; i < game.maindude.status.healthMax; ++i) {

         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         uber::bindTexture(Uniform_DiffuseTexture, i < game.maindude.status.health ? thfilled.handle: thempty.handle);
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

   if (game.ui.showGameUI) {
      uber::bindTexture(Uniform_DiffuseTexture, Graphics.UI.out[0].handle);
      render::meshRender(Graphics.meshUncentered);
   }

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
