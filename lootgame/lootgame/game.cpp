#include "game.h"
#include "app.h"

#include "imgui.h"
#include "scf.h"
#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>
#include <stb/stb_image.h>
#include "win.h"
#include "render.h"

namespace u {
   static StringView color = "uColor";
   static StringView diffuse = "uDiffuse";
   static StringView texMatrix = "uTexMatrix";
   static StringView modelMatrix = "uModelMatrix";
   static StringView viewMatrix = "uViewMatrix";
}

enum {
   GameTextures_Dude = 0,
   GameTextures_Target,
   GameTextures_Light,
   GameTextures_Circle,
   GameTextures_ShittySword,
   GameTextures_Tile,

   GameTexture_COUNT
}GameTextures_;
   
TextureHandle g_textures[GameTexture_COUNT];

enum SwingPhase {
   SwingPhase_Windup = 0,
   SwingPhase_Swing,
   SwingPhase_Cooldown
};

struct AttackSwing {
   f32 lungeSpeed; // character will luinge forward during swingphase
   f32 swipeAngle; // full range of the weapon swipe, in degrees
   Time swingDur; // total time for the attack
   Time windupDur; // vulnerability period before swing
   Time cooldownDur; // period before user is free again
   Rectf hitbox; // axis-aligned, The origin here is the bottom center of the attack while facing up
                 // attack will rotate around that origin
};

struct Dude {
   enum State {
      State_FREE = 0,
      State_ATTACKING,
   };

   State state = State_FREE;

   Float2 renderSize;
   Float2 pos;
   f32 size; // radius of the hitbox circle

   Float2 moveVector;
   Float2 velocity;

   Float2 faceVector;
   Float2 facing = { 0, -1 };

   TextureHandle texture = 0;
   Time lastUpdated;

   Float2 weaponVector;
   AttackSwing swing;
   SwingPhase swingPhase;
   Time phaseStart;
   int swingDir;
   bool swingTimingSuccess; // set to false if hitting attack again before cooldown phase
   int combo;
};

static Constants g_const;
Constants &ConstantsGet() { return g_const; }





struct Game {
   GameData data;

   ShaderHandle shader = 0, colorShader = 0;

   Mesh mesh, meshUncentered;
   FBO fbo, lightfbo;

   Dude dude = {};
};

static Game* g_game = nullptr;
GameData* gameDataGet() {
   return &g_game->data;
}

static void _gameDataInit(GameData* game, StringView assetsFolder) {
   
}

Game* gameCreate(StringView assetsFolder) {
   g_game = new Game();
   _gameDataInit(&g_game->data, assetsFolder);
   return g_game;
}

static TextureHandle _textureBuildFromFile(const char* path, TextureConfig const& cfg = {}) {
   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary(path, &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   auto out = render::textureBuild((ColorRGBA*)png, { x, y }, cfg);

   free(mem);
   free(png);
   return out;
}

static void _createGraphicsObjects(Game* game){
   auto vertex = fileReadString("assets/vertex.glsl");
   auto colorfrag = fileReadString("assets/coloronlyfrag.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   game->shader = render::shaderBuild(vertex, fragment);
   game->colorShader = render::shaderBuild(vertex, colorfrag);

   Vertex vbo[] = { 
      { { -0.5f, -0.5f },{ 0.0f, 0.0f } },
      { {  0.5f, -0.5f },{ 1.0f, 0.0f } },
      { { -0.5f,  0.5f },{ 0.0f, 1.0f } },

      { {  0.5f, -0.5f },{ 1.0f, 0.0f } },
      { {  0.5f,  0.5f },{ 1.0f, 1.0f } },
      { { -0.5f,  0.5f },{ 0.0f, 1.0f } },
   };

   Vertex vboUncentered[] = { 
      { { 0.0f, 0.0f },{ 0.0f, 0.0f } },
      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },

      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 1.0f, 1.0f },{ 1.0f, 1.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },
   };

   game->mesh = render::meshBuild(vbo, 6);
   game->meshUncentered = render::meshBuild(vboUncentered, 6);

   auto& res = ConstantsGet().resolution;
   game->fbo = render::fboBuild({ res.x, res.y });
   game->lightfbo = render::fboBuild({ res.x, res.y });

   g_textures[GameTextures_Dude] = _textureBuildFromFile("assets/dude.png");
   g_textures[GameTextures_Target] = _textureBuildFromFile("assets/target.png");
   g_textures[GameTextures_Light] = _textureBuildFromFile("assets/light.png");
   g_textures[GameTextures_Circle] = _textureBuildFromFile("assets/circle.png");
   g_textures[GameTextures_ShittySword] = _textureBuildFromFile("assets/shittysword.png");
   g_textures[GameTextures_Tile] = _textureBuildFromFile("assets/tile2.png", { RepeatType_REPEAT , FilterType_NEAREST });
   
   free(vertex);
   free(fragment);
}

static Dude _createDude(Game* game) {
   Dude out;
   out.pos = { 50,50 };
   out.size = 30.0f;
   out.renderSize = { 100, 60 };
   out.texture = g_textures[GameTextures_Dude];
   out.lastUpdated = appGetTime();
   return out;
}

#define AXIS_DEADZONE 0.15f

void gameBegin(Game*game) {
   
   _createGraphicsObjects(game);
   game->dude = _createDude(game);

}

static void _renderSwing(Dude&dude) {
   auto origin = dude.pos + dude.weaponVector * dude.size;

   auto model = Matrix::identity();
   model *= Matrix::translate2f(origin);
   model *= Matrix::rotate2D(atan2f(dude.weaponVector.x, -dude.weaponVector.y));
   model *= Matrix::translate2f({dude.swing.hitbox.x, dude.swing.hitbox.y});
   model *= Matrix::scale2f({ dude.swing.hitbox.w, dude.swing.hitbox.h });

   render::uSetColor(u::color, White);
   render::uSetMatrix(u::modelMatrix, model);
   render::textureBind(g_textures[GameTextures_ShittySword], 0);
   render::meshRender(g_game->meshUncentered);
}

static void _renderDude(Dude& dude) {
   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   model *= Matrix::translate2f(dude.pos);
   model *= Matrix::rotate2D(atan2f(dude.facing.x, -dude.facing.y));
   model *= Matrix::scale2f(dude.renderSize);

   render::uSetColor(u::color, White);
   render::uSetMatrix(u::texMatrix, texmat);
   render::uSetMatrix(u::modelMatrix, model);
   
   render::uSetTextureSlot(u::diffuse, 0);
   render::textureBind(dude.texture, 0);
   render::meshRender(g_game->mesh);

   //model = Matrix::translate2f(dude.pos);
   //model *= Matrix::scale2f({dude.size *  2, dude.size * 2});
   //render::uSetColor(u::color, {0.5f,1.f,1.f,0.75f});
   //render::uSetMatrix(u::modelMatrix, model);
   //render::textureBind(g_textures[GameTextures_Circle], 0);   
   //render::meshRender(g_game->mesh);
   
   if (dude.state == Dude::State_ATTACKING) {
      _renderSwing(dude);
   }
}

static void _renderTarget(Float2 pos, ColorRGBAf color, f32 sz) {
   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   model *= Matrix::translate2f(pos);
   model *= Matrix::scale2f({ sz,sz });

   render::uSetColor(u::color, color);
   render::uSetMatrix(u::texMatrix, texmat);
   render::uSetMatrix(u::modelMatrix, model);
   render::uSetTextureSlot(u::diffuse, 0);
   render::textureBind(g_textures[GameTextures_Target], 0);
   render::meshRender(g_game->mesh);
}

static void _populateLightLayer(Game* game) {
   auto& c = ConstantsGet();
   auto& res = c.resolution;

   auto view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
   render::shaderSetActive(game->shader);
   render::uSetMatrix(u::viewMatrix, view);

   render::fboBind(game->lightfbo);
   render::setBlendMode(BlendMode_PURE_ADD);
   render::viewport({ 0,0, res.x, res.y });
   
   render::clear({0.f,0.f,0.f,1.0f});
   //render::clear({1.0f, 1.0f, 1.0f, 0.f});

   auto model = Matrix::translate2f(game->dude.pos);
   model *= Matrix::scale2f({ 500,500 });

   render::uSetColor(u::color, White);
   render::uSetMatrix(u::modelMatrix, model);
   render::textureBind(g_textures[GameTextures_Light], 0);
   render::meshRender(g_game->mesh);

   model = Matrix::translate2f({420, 500});
   model *= Matrix::scale2f({ 400,500 });
   render::uSetColor(u::color, White);
   render::uSetMatrix(u::modelMatrix, model);
   render::meshRender(g_game->mesh);

   model = Matrix::translate2f({ 1000,700 });
   model *= Matrix::scale2f({ 500,800 });
   render::uSetColor(u::color, White);
   render::uSetMatrix(u::modelMatrix, model);
   render::meshRender(g_game->mesh);

   view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
   render::shaderSetActive(game->colorShader);
   render::uSetMatrix(u::viewMatrix, view);

   model = Matrix::scale2f({ (f32)res.x, (f32)res.y});
   render::uSetColor(u::color, {1, 1, 1, 0.5f});
   render::uSetMatrix(u::modelMatrix, model);
   render::textureBind(0, 0);
   render::meshRender(g_game->meshUncentered);
}

static void _renderFloor(Game* game) {
   auto& c = ConstantsGet();
   auto& res = c.resolution;

   Float2 fres = { (f32)res.x, (f32)res.y };
   f32 r = fres.x / fres.y;

   auto texmat = Matrix::scale2f({ fres.x / 50, fres.y/50});
   auto model = Matrix::scale2f(fres);

   render::uSetColor(u::color, White);
   render::uSetMatrix(u::texMatrix, texmat);
   render::uSetMatrix(u::modelMatrix, model);
   render::uSetTextureSlot(u::diffuse, 0);
   render::textureBind(g_textures[GameTextures_Tile], 0);
   render::meshRender(g_game->meshUncentered);
}

static void _renderLightLayer(Game* game) {

   render::setBlendMode(BlendMode_LIGHTING);
   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   model *= Matrix::scale2f({ (float)game->lightfbo.sz.x, (float)game->lightfbo.sz.y });

   render::uSetColor(u::color, White);
   render::uSetMatrix(u::texMatrix, texmat);
   render::uSetMatrix(u::modelMatrix, model);
   render::uSetTextureSlot(u::diffuse, 0);

   render::textureBind(game->lightfbo.tex, 0);
   render::meshRender(g_game->meshUncentered);

}

static void _renderScene(Game* game) {
   auto& c = ConstantsGet();
   auto& res = ConstantsGet().resolution;

   _populateLightLayer(game);

   auto view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
   render::shaderSetActive(game->shader);
   render::uSetMatrix(u::viewMatrix, view);

   render::fboBind(game->fbo);

   render::setBlendMode(BlendMode_NORMAL);
   render::viewport({ 0,0, res.x, res.y });
   render::clear(White);

   _renderFloor(game);

   _renderDude(game->dude);   

   if (game->data.imgui.showMovementDebugging) {
      const static f32 moveTargetDist = 100.0f;
      const static f32 aimTargetDist = 200.0f;
      auto &io = game->data.io;
      auto &dude = game->dude;

      _renderTarget(dude.pos + dude.velocity * moveTargetDist, Cyan, 40);
      _renderTarget(dude.pos + v2Normalized(dude.facing) * aimTargetDist, Red, 40);

      _renderTarget(dude.pos + io.leftStick * moveTargetDist, DkGreen, 30);
      _renderTarget(dude.pos + dude.moveVector * moveTargetDist, Magenta, 30);   

      _renderTarget(dude.pos + io.rightStick * aimTargetDist, Yellow, 30);
      _renderTarget(dude.pos + dude.faceVector * aimTargetDist, LtGray, 30);
   }
   
   _renderLightLayer(game);


   render::fboBind({});
}

bool gameProcessEvent(Game*game, SDL_Event* event) {
   auto &io = game->data.io;

   switch (event->type) {
   case SDL_CONTROLLERDEVICEADDED:
      SDL_GameControllerOpen(0);
      return true;

   case SDL_CONTROLLERBUTTONUP:
   case SDL_CONTROLLERBUTTONDOWN:
      switch (event->cbutton.button) {
      case SDL_CONTROLLER_BUTTON_A: break;
      case SDL_CONTROLLER_BUTTON_B: break;
      case SDL_CONTROLLER_BUTTON_X: break;
      case SDL_CONTROLLER_BUTTON_Y: break;
      case SDL_CONTROLLER_BUTTON_BACK: break;
      case SDL_CONTROLLER_BUTTON_START: break;
      case SDL_CONTROLLER_BUTTON_LEFTSTICK: break;
      case SDL_CONTROLLER_BUTTON_RIGHTSTICK: break;
      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: break;
      case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: break;
      case SDL_CONTROLLER_BUTTON_DPAD_UP: break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: break;
      default: return false;
      }      
      return true;

   case SDL_CONTROLLERAXISMOTION:
   {
      auto value = event->caxis.value / (float)(INT16_MAX + 1);
      bool pressed = true;
      GameButton btn;
      if (fabs(value) < AXIS_DEADZONE) {
         value = 0.0f;
         pressed = false;
      }

      switch (event->caxis.axis) {
      case SDL_CONTROLLER_AXIS_LEFTX: io.leftStick.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_LEFTY: io.leftStick.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_RIGHTX: io.rightStick.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: io.rightStick.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT: io.leftTrigger = value; btn = GameButton_LT; break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: io.rightTrigger = value; btn = GameButton_RT; break;
      default: return false;
      }

      if (!io.buttonDown[btn] && pressed) {
         io.buttonPressed[btn] = true;
      }

      if (io.buttonDown[btn] && !pressed) {
         io.buttonReleased[btn] = true;
      }

      io.buttonDown[btn] = pressed;
   } return true;;
   }

   return false;
}

void gameHandleInput(Game*game) {
   auto& res = ConstantsGet().resolution;
   auto& vpScreen = game->data.imgui.vpScreenArea;
   auto& mPos = ImGui::GetIO().MousePos;

   auto& io = game->data.io;

   io.mousePos = {
      (mPos.x - vpScreen.x) / vpScreen.w * res.x,
      (mPos.y - vpScreen.y) / vpScreen.h * res.y
   };

   // update buttons
   for (GameButton b = 0; b < GameButton_COUNT; ++b) {
      io.buttonPressed[b] = io.buttonReleased[b] = false;
   }
}

void gameRender(Game*game) {
   _renderScene(game);
}

static void _beginAttack(Dude& dude, Time t, int dir, f32 lunge) {
   dude.state = Dude::State_ATTACKING;
   dude.moveVector = dude.faceVector = { 0.0f, 0.0f };

   dude.swing.swipeAngle = 180.0f;

   dude.swing.lungeSpeed = lunge;
   dude.swing.windupDur = timeMillis(100);
   dude.swing.swingDur = timeMillis(250);
   dude.swing.cooldownDur = timeMillis(500);

   dude.swing.hitbox = { -10, -50, 20, 50 };
   dude.weaponVector = v2Normalized(v2Rotate(dude.facing, v2FromAngle(dir * dude.swing.swipeAngle / 2.0f * DEG2RAD)));
   dude.swingPhase = SwingPhase_Windup;
   dude.phaseStart = t;
   dude.swingDir = dir;

   dude.swingTimingSuccess = true;
}

static void _updateDude(Game* game) {
   auto time = appGetTime();
   auto dt = time - game->dude.lastUpdated;
   if (dt.toMilliseconds() == 0) {
      return;
   }

   auto&m = game->data.io.mousePos;
   auto&dp = game->dude.pos;
   auto &io = game->data.io;
   auto &dude = game->dude;

   auto& c = ConstantsGet();

   // handle inputs
   if (game->dude.state == Dude::State_FREE) {

      // movement/aiming
      dude.moveVector = v2MoveTowards(dude.moveVector, io.leftStick, c.stickTrackingSpeed * dt.toMilliseconds());
      Float2 aimStick;
      if (fabs(io.rightStick.x) > AXIS_DEADZONE || fabs(io.rightStick.y) > AXIS_DEADZONE) {
         aimStick = io.rightStick;
      }
      else if (fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE) {
         aimStick = io.leftStick;
      }
      dude.faceVector = v2MoveTowards(dude.faceVector, aimStick, c.stickTrackingSpeed * dt.toMilliseconds());

      // triggers
      if (io.buttonPressed[GameButton_RT]) {
         game->dude.combo = 0;
         _beginAttack(game->dude, time, -1, 0.0f);
      }
   }
   else if (game->dude.state == Dude::State_ATTACKING) {
      auto swingdt = time - game->dude.phaseStart;

      switch (dude.swingPhase) {
      case SwingPhase_Windup:
         if (io.buttonPressed[GameButton_RT]) {
            dude.swingTimingSuccess = false;
         }
         if (swingdt > dude.swing.windupDur) {
            dude.phaseStart += dude.swing.windupDur;
            dude.swingPhase = SwingPhase_Swing;
         }
         break;
      case SwingPhase_Swing: {
         if (io.buttonPressed[GameButton_RT]) {
            dude.swingTimingSuccess = false;
         }

         auto radsPerMs = (dude.swing.swipeAngle * DEG2RAD) / dude.swing.swingDur.toMilliseconds();
         dude.weaponVector = v2Rotate(dude.weaponVector, v2FromAngle(-dude.swingDir * radsPerMs * dt.toMilliseconds()));

         dp += dude.facing * dude.swing.lungeSpeed * dt.toMilliseconds();

         if (swingdt > dude.swing.swingDur) {
            dude.phaseStart += dude.swing.swingDur;
            dude.weaponVector = v2Normalized(v2Rotate(dude.facing, v2FromAngle(-dude.swingDir * dude.swing.swipeAngle / 2.0f * DEG2RAD)));
            dude.swingPhase = SwingPhase_Cooldown;
         }
      }  break;
      case SwingPhase_Cooldown:
         if (io.buttonPressed[GameButton_RT] && dude.swingTimingSuccess && dude.combo < 3) {
            ++dude.combo;
            f32 lunge = 0.0f;
            switch (dude.combo) {
            case 1: lunge = 0.25f; break;
            case 2: lunge = 0.75f; break;
            }
            _beginAttack(game->dude, time, -dude.swingDir, lunge);
         }
         else {
            if (swingdt > dude.swing.cooldownDur) {
               dude.phaseStart += dude.swing.cooldownDur;
               dude.state = Dude::State_FREE;
            }
         }
         
         break;
      }
   }

   // update movement and aiming vectors
   if (v2LenSquared(dude.faceVector) > 0.0f) {
      dude.facing = v2Normalized(v2RotateTowards(dude.facing, dude.faceVector, v2FromAngle(c.dudeRotationSpeed * dt.toMilliseconds())));
   }
   dude.velocity = v2MoveTowards(dude.velocity, dude.moveVector, c.dudeAcceleration * dt.toMilliseconds());

   if (v2LenSquared(dude.velocity) > 0.0f) {
      auto velnorm = v2Normalized(dude.velocity);
      auto facnorm = v2Normalized(dude.facing);

      // scale half of mvspeed based on facing;
      f32 facing = v2Dot(velnorm, facnorm);
      auto scaledSpeed = (c.dudeMoveSpeed * 0.75f) + (c.dudeMoveSpeed * 0.25f * facing);

      dp += dude.velocity * scaledSpeed * dt.toMilliseconds();
   }

   game->dude.lastUpdated = appGetTime();
}

void gameUpdate(Game* game) {   
   
   
   _updateDude(game);
   


   gameDoUI(game);
}

void gameDestroy(Game* game) {
   delete game;
}

FBO const& gameGetOutputFBO(Game* game) {
   return game->fbo;
}
