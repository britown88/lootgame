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
      

   uber::bindTexture(Uniform_DiffuseTexture, Textures.ShittySword->handle);
   uber::bindTexture(Uniform_NormalsTexture, Textures.SwordNormals->handle);

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
   uber::bindTexture(Uniform_NormalsTexture, Textures.DudeNormals->handle);
   render::meshRender(Graphics.mesh);

   //uber::set(Uniform_OutlineOnly, true);
   //render::meshRender(g_game->mesh);

   if (dude.state == DudeState_ATTACKING) {
      _renderSwing(dude);
   }
}

static void _renderFloor(GameState& game) {
   auto& c = Const;

   Float2 fres = { game.map->size.x, game.map->size.y };
   Float2 tileSize = { 16, 16 };

   uber::resetToDefault();
   uber::set(Uniform_Height, Const.floorHeight);
   uber::set(Uniform_TextureMatrix, Matrix::scale2f({ fres.x / tileSize.x, fres.y / tileSize.y }));
   uber::set(Uniform_ModelMatrix, Matrix::scale2f(fres));

   uber::bindTexture(Uniform_DiffuseTexture, Textures.Tile->handle);
   uber::bindTexture(Uniform_NormalsTexture, Textures.TileNormals->handle);
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
   
   //TODO: old const values, this function goes away
   uber::set(Uniform_LightAttrs, Float3{ 0.0f, 0.4f, 50.0f });
   uber::set(Uniform_Height, 0.2f);


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

   auto mHandle = render::meshBuild(mesh.data(), (int)mesh.size());

   uber::set(Uniform_Alpha, 1.0f);
   uber::set(Uniform_ModelMatrix, Matrix::identity());
   render::meshRender(mHandle);

   render::meshDestroy(mHandle);
}

static void _renderLight(GameState&g, Light& light) {
   uber::set(Uniform_Color, light.color);
   uber::set(Uniform_Alpha, 1.0f);

   uber::set(Uniform_Height, light.height);
   uber::set(Uniform_LightAttrs, Float3{
      light.linearPortion,
      light.smoothingFactor,
      light.intensity });

   uber::set(Uniform_ModelMatrix, Matrix::translate2f(light.pos.toViewport(g)) * Matrix::scale2f({ light.radius * 2, light.radius * 2 }));
   render::meshRender(Graphics.mesh);
}

void renderLightLayer(GameState& game) {

   Array<ConvexPoly> blockers;
   for (auto w : game.map->walls) {
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

   

   //uber::set(Uniform_LightIntensity, cLightIntensity);
   auto candleColor = sRgbToLinear(ColorRGB{ 255,147,41 });

   //_addLight({ vp.w,vp.w }, { vp.w / 2.0f, vp.h / 2.0f }, Yellow);
   _addLight(game, 80, Coords::fromWorld(game.maindude.phy.pos), Green, blockers);

   //_addLight(game, 150, Coords::fromWorld({ 200, 200 }), candleColor, blockers);

   int i = 0;
   for (auto&& d : game.baddudes) {
      if (dudeAlive(d)) {
         static ColorRGBAf c[] = { Red, Green, Blue };
         _addLight(game, 80, Coords::fromWorld(d.phy.pos), candleColor, blockers);
      }
   }

   for (auto&&light : game.map->lights) {
      _renderLight(game, light);
   }
   if (game.ui.editing && game.ui.mode == GameEditMode_Lights) {
      _renderLight(game, game.ui.newLight);
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

void renderUI(GameState& g) {
   auto& vp = g.camera.viewport;

   render::fboBind(Graphics.UI);

   uber::resetToDefault();
   render::clear(Cleared);

   render::setBlendMode(BlendMode_NORMAL);
   

   Float2 fbosz = { (float)Const.vpSize.x, (float)Const.vpSize.y };
   uber::set(Uniform_ViewMatrix, Matrix::ortho(0, fbosz.x, 0, fbosz.y, 1, -1));

   /*if (game->maindude.stamina < game->maindude.staminaMax)*/ {
      auto &tfilled = *Textures.GemFilled;
      auto &tempty = *Textures.GemEmpty;
      auto &tcracked = *Textures.GemCracked;

      auto &thfilled = *Textures.HeartFilled;
      auto &thempty = *Textures.HeartEmpty;

      auto tsz = tfilled.sz;
      Float2 gemSize = { (float)tsz.x, (float)tsz.y };
      float gemSpace = 1.0f;
      float hpstamSpace = 5.0f;

      auto stamCount = g.maindude.status.stamina.size();
      auto stamina = 0;
      for (auto&& s : g.maindude.status.stamina) {
         if (s.state == PipState_Full) { ++stamina; }
      }

      auto w = (gemSize.x + gemSpace) * (stamCount + g.maindude.status.healthMax) + hpstamSpace;


      auto dudePos = Coords::fromWorld(g.maindude.phy.pos).toViewport(g);
      float heightOffset = g.maindude.phy.circle.size + 20.0f;
      //Float2 staminaCorner = dudePos + Float2{-w/2.0f, heightOffset };

      //Float2 staminaCorner = { game->maindude.phy.pos.x - w / 2.0f, game->maindude.phy.pos.y + game->maindude.phy.circle.size + 20 };
      Float2 staminaCorner ={ 10,10 };// Coords::fromWorld(g.maindude.phy.pos).toViewport(g);

      if (stamina != 0) {
         uber::set(Uniform_Alpha, 0.5f);
         //uber::set(Uniform_Color, Red);
      }

      for (int i = 0; i < g.maindude.status.healthMax; ++i) {

         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);
         uber::set(Uniform_ModelMatrix, model);
         uber::bindTexture(Uniform_DiffuseTexture, i < g.maindude.status.health ? thfilled.handle : thempty.handle);
         render::meshRender(Graphics.meshUncentered);

         staminaCorner.x += gemSize.x + gemSpace;
      }
      staminaCorner.x += hpstamSpace;

      if (stamina == 0) {
         //uber::set(Uniform_Alpha, 1.0f);
         uber::set(Uniform_Color, Red);
      }
      for (int i = 0; i < stamCount; ++i) {
         auto &pip = g.maindude.status.stamina[i];
         auto model = Matrix::translate2f(staminaCorner) *  Matrix::scale2f(gemSize);

         uber::set(Uniform_ModelMatrix, model);
         if (pip.state == PipState_Full) {
            uber::set(Uniform_Alpha, 1.0f);
            uber::bindTexture(Uniform_DiffuseTexture, tfilled.handle);
            render::meshRender(Graphics.meshUncentered);
         }
         else {
            uber::set(Uniform_Alpha, 1.0f);
            uber::bindTexture(Uniform_DiffuseTexture, tempty.handle);
            render::meshRender(Graphics.meshUncentered);

            auto ratio = (float)pip.charge / pip.fullCharge;
            uber::set(Uniform_Alpha, ratio);
            uber::bindTexture(Uniform_DiffuseTexture, tfilled.handle);
            render::meshRender(Graphics.meshUncentered);

            if (pip.state == PipState_Cracked) {
               uber::set(Uniform_Alpha, 1.0f - ratio);
               uber::bindTexture(Uniform_DiffuseTexture, tcracked.handle);
               render::meshRender(Graphics.meshUncentered);
            }

         }

         staminaCorner.x += gemSize.x + gemSpace;
      }

      //uber::resetToDefault();
      //uber::set(Uniform_ViewMatrix, Matrix::ortho(0, fbosz.x, 0, fbosz.y, 1, -1));

      
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
