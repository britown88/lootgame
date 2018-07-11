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
   GameTextures_GemEmpty,
   GameTextures_GemFilled,

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

struct MoveSet {
   std::vector<AttackSwing> swings;
};

static MoveSet _createMoveSet() {
   MoveSet out;
   out.swings.resize(3);
   Rectf hitbox = { 0, -25, 80, 50 };

   out.swings[0].swipeAngle = 90.0f;
   out.swings[0].lungeSpeed = 0.0f;
   out.swings[0].windupDur = timeMillis(100);
   out.swings[0].swingDur = timeMillis(250);
   out.swings[0].cooldownDur = timeMillis(500);
   out.swings[0].hitbox = hitbox;

   out.swings[1].swipeAngle = 180.0f;
   out.swings[1].lungeSpeed = 0.25f;
   out.swings[1].windupDur = timeMillis(100);
   out.swings[1].swingDur = timeMillis(250);
   out.swings[1].cooldownDur = timeMillis(250);
   out.swings[1].hitbox = hitbox;

   out.swings[2].swipeAngle = 0.0f;
   out.swings[2].lungeSpeed = 0.5f;
   out.swings[2].windupDur = timeMillis(100);
   out.swings[2].swingDur = timeMillis(250);
   out.swings[2].cooldownDur = timeMillis(100);
   out.swings[2].hitbox = hitbox;

   return out;
}

struct Dude {
   enum State {
      State_FREE = 0,
      State_ATTACKING,
   };

   State state = State_FREE;

   Float2 renderSize;
   Float2 pos;
   f32 size; // radius of the hitbox circle
   ColorRGBAf c;

   Float2 moveVector;
   Float2 velocity;

   Float2 faceVector;
   Float2 facing = { 1, 0 };

   TextureHandle texture = 0;
   Time lastUpdated;
   Time lastFree; // tracked for time since entering free state

   int stamina, staminaMax;

   Float2 weaponVector;
   MoveSet moveset;
   AttackSwing swing;
   SwingPhase swingPhase;
   Time phaseStart;
   int swingDir;
   bool swingTimingSuccess; // set to false if hitting attack again before cooldown phase
   int combo = 0;

   bool hit = false;
};

static Constants g_const;
Constants &ConstantsGet() { return g_const; }





struct Game {
   GameData data;

   ShaderHandle shader = 0, colorShader = 0;

   Mesh mesh, meshUncentered;
   FBO fbo, lightfbo;

   Dude dude = {};
   std::vector<Dude> dudes;

   Time lastMouseMove;
   bool mouseActive;
};

static Game* g_game = nullptr;
GameData* gameDataGet() {
   return &g_game->data;
}

static void _gameDataInit(GameData* game, StringView assetsFolder) {
#ifndef  _DEBUG
   game->imgui.showUI = false;
#endif //  NDEBUG

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
   g_textures[GameTextures_Circle] = _textureBuildFromFile("assets/circle.png", { RepeatType_CLAMP , FilterType_LINEAR });
   g_textures[GameTextures_ShittySword] = _textureBuildFromFile("assets/shittysword.png");
   g_textures[GameTextures_GemEmpty] = _textureBuildFromFile("assets/gemempty.png");
   g_textures[GameTextures_GemFilled] = _textureBuildFromFile("assets/gemfilled.png");
   g_textures[GameTextures_Tile] = _textureBuildFromFile("assets/tile2.png", { RepeatType_REPEAT , FilterType_NEAREST });
   
   free(vertex);
   free(fragment);
}

static Dude _createDude(Game* game) {
   Dude out;
   out.c = White;
   out.moveset = _createMoveSet();
   out.pos = { 50,50 };
   out.size = 30.0f;
   out.renderSize = { 60, 100 };
   out.texture = g_textures[GameTextures_Dude];
   out.lastUpdated = out.lastFree = appGetTime();

   out.stamina = out.staminaMax = 4;
   return out;
}

static Dude _createEnemy(Float2 pos, f32 size) {
   Dude out;
   out.moveset = _createMoveSet();
   out.c = {1.0f, 0.3f, 0.3f, 1.0f};
   out.pos = pos;
   out.size = size;
   out.renderSize = { size * 2.5f, size * 4.2f };
   out.texture = g_textures[GameTextures_Dude];
   out.lastUpdated = out.lastFree = appGetTime();
   out.stamina = out.staminaMax = 4;

   auto fx = ((rand() % 200) - 100) / 100.0f;
   auto fy = ((rand() % 200) - 100) / 100.0f;
   out.facing = v2Normalized({fx, fy});

   return out;
}

#define AXIS_DEADZONE 0.25f

bool circleVsAabb(Float2 co, f32 cr, Rectf const& aabb) {
   Float2 a = { aabb.x, aabb.y };
   Float2 b = { a.x + aabb.w, a.y + aabb.h };

   Float2 offset = { 0,0 };

   if (co.x > b.x) { offset.x = co.x - b.x; }
   else if (co.x < a.x) { offset.x = a.x - co.x; }

   if (co.y > b.y) { offset.y = co.y - b.y; }
   else if (co.y < a.y) { offset.y = a.y - co.y; }

   // if offset length is less than the radius, collision!
   return v2Len(offset) < cr;
}

bool _attackCollision(Dude& attacker, Dude& defender) {
   auto origin = attacker.pos + attacker.weaponVector * attacker.size;
   auto defendPos = v2Rotate(defender.pos - origin, v2Conjugate(attacker.weaponVector));

   auto wbox = attacker.moveset.swings[attacker.combo].hitbox;
   //wbox.x += origin.x;
   //wbox.y += origin.y;

   return circleVsAabb(defendPos, defender.size, wbox);
}

void gameBegin(Game*game) {
   
   _createGraphicsObjects(game);
   game->dude = _createDude(game);

   for (int i = 0; i < 125; ++i) {
      game->dudes.push_back(_createEnemy({ (f32)(rand() % 1820) + 100, (f32)(rand()%980) + 100}, 30.0f));
   }

   
  // game->dudes.push_back(_createEnemy({ 300, 300 }, 50.0f));
   //game->dudes.push_back(_createEnemy({ 800, 600 }, 20.0f));
   //game->dudes.push_back(_createEnemy({ 1000, 100 }, 80.0f));

}

static void _renderSwing(Dude&dude) {
   auto origin = dude.pos + dude.weaponVector * dude.size;

   auto model = Matrix::identity();
   model *= Matrix::translate2f(origin);
   model *= Matrix::rotate2D(v2Angle(dude.weaponVector));
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
   model *= Matrix::rotate2D(v2Angle(dude.facing));
   model *= Matrix::scale2f(dude.renderSize);

   render::uSetColor(u::color, dude.c);
   render::uSetMatrix(u::texMatrix, texmat);
   render::uSetMatrix(u::modelMatrix, model);
   
   render::uSetTextureSlot(u::diffuse, 0);
   render::textureBind(dude.texture, 0);
   render::meshRender(g_game->mesh);

   if (gameDataGet()->imgui.showCollisionDebugging) {
      model = Matrix::translate2f(dude.pos);
      model *= Matrix::scale2f({ dude.size * 2, dude.size * 2 });
      render::uSetColor(u::color, dude.hit ? Red : Cyan);
      render::uSetMatrix(u::modelMatrix, model);
      render::textureBind(g_textures[GameTextures_Circle], 0);
      render::meshRender(g_game->mesh);
   }
   
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

static void _renderEnemy(Dude& dude) {

   auto c = dude.hit ? Red : Cyan;

   auto model = Matrix::translate2f(dude.pos);
   model *= Matrix::scale2f({ dude.size * 2, dude.size * 2 });
   render::uSetColor(u::color, c);
   render::uSetMatrix(u::modelMatrix, model);
   render::uSetMatrix(u::texMatrix, Matrix::identity());
   render::uSetTextureSlot(u::diffuse, 0);
   render::textureBind(g_textures[GameTextures_Circle], 0);
   render::meshRender(g_game->mesh);

}

static void _addLight(Float2 size, Float2 pos, ColorRGBAf c) {
   auto model = Matrix::translate2f(pos);
   model *= Matrix::scale2f(size);

   render::uSetColor(u::color, c);
   render::uSetMatrix(u::modelMatrix, model);
   render::textureBind(g_textures[GameTextures_Light], 0);
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
   
   render::clear({0.f,0.f,0.f,0.0f});
   //render::clear({1.0f, 1.0f, 1.0f, 0.f});

   _addLight({ 500, 500 }, game->dude.pos, White);

   f32 lsz = 800;
   _addLight({ lsz, lsz }, { 200, 600 }, Red);
   _addLight({ lsz, lsz }, { 800, 300 }, Yellow);
   _addLight({ lsz, lsz }, { 100, 1000 }, Blue);
   _addLight({ lsz, lsz }, { 1200, 500 }, Green);
   _addLight({ lsz, lsz }, { 1800, 200 }, Magenta);
   _addLight({ lsz, lsz }, { 1800, 800 }, Cyan);
   _addLight({ lsz, lsz }, { 900, 800 }, DkGreen);


   render::shaderSetActive(game->colorShader);
   render::uSetMatrix(u::viewMatrix, view);

   auto model = Matrix::scale2f({ (f32)res.x, (f32)res.y});
   auto al = gameDataGet()->imgui.ambientLight;
   render::uSetColor(u::color, { al,al,al,al });
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

   if (gameDataGet()->imgui.showCollisionDebugging) {
      Rectf testrect = { 300,300, 500, 200 };
      render::shaderSetActive(game->colorShader);
      render::uSetColor(u::color, circleVsAabb(game->dude.pos, game->dude.size, testrect) ? Red : Cyan);
      render::uSetMatrix(u::modelMatrix, Matrix::translate2f({ testrect.x, testrect.y }) * Matrix::scale2f({ testrect.w, testrect.h }));
      render::textureBind(0, 0);
      render::meshRender(g_game->meshUncentered);

      render::shaderSetActive(game->shader);
   }


   
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

static void _renderGameUI(Game* game) {
   render::setBlendMode(BlendMode_NORMAL);

   render::uSetTextureSlot(u::diffuse, 0);
   render::uSetMatrix(u::texMatrix, Matrix::identity());
   render::uSetColor(u::color, White);

   Float2 staminaCorner = { 5,5 };
   Float2 gemSize = { 25, 50 };
   f32 gemSpace = 5.0f;

   for (int i = 0; i < game->dude.staminaMax; ++i) {
      auto model = Matrix::translate2f(staminaCorner + Float2{(gemSize.x + gemSpace) * i, 0}) *  Matrix::scale2f(gemSize);
      render::uSetMatrix(u::modelMatrix, model);
      render::textureBind(g_textures[i < game->dude.stamina ? GameTextures_GemFilled : GameTextures_GemEmpty], 0);
      render::meshRender(g_game->meshUncentered);
   }

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

   for (auto&& d : game->dudes) {
      _renderDude(d);
   }

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

   _renderGameUI(game);


   render::fboBind({});
}

bool gameProcessEvent(Game*game, SDL_Event* event) {
   auto &io = game->data.io;

   switch (event->type) {
   case SDL_KEYUP:
   case SDL_KEYDOWN: {
      bool pressed = event->type == SDL_KEYDOWN;
      GameButton btn;
      
      switch (event->key.keysym.scancode) {
      case SDL_SCANCODE_W: 
         btn = GameButton_UP; 

         if (pressed) {            
            if (!event->key.repeat) io.leftStick.y = -1.0f;
         }
         else if (!io.buttonDown[GameButton_DOWN]) {
            io.leftStick.y = 0.0f;
         }

         break;
      case SDL_SCANCODE_A:
         btn = GameButton_LEFT;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.x = -1.0f;
         }
         else if (!io.buttonDown[GameButton_RIGHT]) {
            io.leftStick.x = 0.0f;
         }
         break;
      case SDL_SCANCODE_S:
         btn = GameButton_DOWN;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.y = 1.0f;
         }
         else if (!io.buttonDown[GameButton_UP]) {
            io.leftStick.y = 0.0f;
         }
         break;
      case SDL_SCANCODE_D:
         btn = GameButton_RIGHT;

         if (pressed) {
            if (!event->key.repeat) io.leftStick.x = 1.0f;
         }
         else if (!io.buttonDown[GameButton_LEFT]) {
            io.leftStick.x = 0.0f;
         }
         break;
      default: return false;
      }

      if (!event->key.repeat) {
         io.leftStick = v2Normalized(io.leftStick);
      }
      
      if (!io.buttonDown[btn] && pressed) {
         io.buttonPressed[btn] = true;
      }
      if (io.buttonDown[btn] && !pressed) {
         io.buttonReleased[btn] = true;
      }
      io.buttonDown[btn] = pressed;

      return true;
   }  break;

   case SDL_MOUSEMOTION:
      game->lastMouseMove = appGetTime();
      game->mouseActive = true;
      break;

   case SDL_MOUSEBUTTONDOWN:
   case SDL_MOUSEBUTTONUP: {
      bool pressed = event->type == SDL_MOUSEBUTTONDOWN;
      if (!io.buttonDown[GameButton_RT] && pressed) {
         io.buttonPressed[GameButton_RT] = true;
      }

      if (io.buttonDown[GameButton_RT] && !pressed) {
         io.buttonReleased[GameButton_RT] = true;
      }

      io.buttonDown[GameButton_RT] = pressed;
      return true;
   }
      

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

static void _beginAttack(Dude& dude, Time t, int dir, int combo) {
   dude.combo = combo;
   dude.swing = dude.moveset.swings[combo];

   dude.state = Dude::State_ATTACKING;
   dude.moveVector = dude.faceVector = { 0.0f, 0.0f };
   
   dude.weaponVector = v2Normalized(v2Rotate(dude.facing, v2FromAngle(dir * dude.swing.swipeAngle / 2.0f * DEG2RAD)));
   dude.swingPhase = SwingPhase_Windup;
   dude.phaseStart = t;
   dude.swingDir = dir;

   dude.swingTimingSuccess = true;
}

static bool rightStickActive() {
   auto &io = g_game->data.io;
   return fabs(io.rightStick.x) > AXIS_DEADZONE || fabs(io.rightStick.y) > AXIS_DEADZONE;
}
static bool leftStickActive() {
   auto &io = g_game->data.io;
   return fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE;
}

static bool _dudeCollision(Dude& mover, Dude& other) {
   if (v2Dist(mover.pos, other.pos) < mover.size + other.size) {

      auto reflect = v2Orthogonal(other.pos - mover.pos);
      mover.velocity = v2Normalized( reflect * v2Dot(mover.velocity, reflect));
      return true;
   }
   return false;
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


   // movement/aiming
   dude.moveVector = io.leftStick;

   bool rstick = rightStickActive();
   bool lstick = leftStickActive();

   Float2 aimStick;
   if (rstick) {
      aimStick = io.rightStick;
      game->mouseActive = false;
   }
   else if (lstick) {
      aimStick = io.leftStick;
   }

   if (game->mouseActive) {
      aimStick = io.mousePos - game->dude.pos;
   }

   if (v2LenSquared(aimStick) > 0) {
      dude.faceVector = v2Normalized(aimStick);
   }

   // handle inputs
   if (game->dude.state == Dude::State_FREE) {

      auto freedt = time - dude.lastFree;
      if (freedt > timeMillis(500) && dude.stamina < dude.staminaMax) {
         dude.stamina = dude.staminaMax;
      }

      // triggers
      if (dude.stamina > 0 && io.buttonPressed[GameButton_RT]) {
         _beginAttack(game->dude, time, -1, 0);
      }

      // update movement and aiming vectors
      if (v2LenSquared(dude.faceVector) > 0.0f) {
         dude.facing = v2Normalized(v2RotateTowards(dude.facing, dude.faceVector, v2FromAngle(c.dudeRotationSpeed * dt.toMilliseconds())));
      }
      dude.velocity = v2MoveTowards(dude.velocity, dude.moveVector, c.dudeAcceleration * dt.toMilliseconds());
   }
   else if (game->dude.state == Dude::State_ATTACKING) {
      auto swingdt = time - game->dude.phaseStart;

      dude.velocity = v2MoveTowards(dude.velocity, {0,0}, c.dudeAcceleration * dt.toMilliseconds());

      switch (dude.swingPhase) {
      case SwingPhase_Windup:
         if (io.buttonPressed[GameButton_RT]) {
            dude.swingTimingSuccess = false;
         }
         if (swingdt > dude.swing.windupDur) {
            dude.phaseStart += dude.swing.windupDur;
            dude.swingPhase = SwingPhase_Swing;
            dude.stamina -= 1;
         }
         break;
      case SwingPhase_Swing: {
         
         if (io.buttonPressed[GameButton_RT]) {
            dude.swingTimingSuccess = false;
         }

         auto radsPerMs = (dude.swing.swipeAngle * DEG2RAD) / dude.swing.swingDur.toMilliseconds();
         dude.weaponVector = v2Rotate(dude.weaponVector, v2FromAngle(-dude.swingDir * radsPerMs * dt.toMilliseconds()));

         dp += dude.facing * dude.swing.lungeSpeed * (f32)dt.toMilliseconds();

         if (swingdt > dude.swing.swingDur) {
            dude.phaseStart += dude.swing.swingDur;
            dude.weaponVector = v2Normalized(v2Rotate(dude.facing, v2FromAngle(-dude.swingDir * dude.swing.swipeAngle / 2.0f * DEG2RAD)));
            dude.swingPhase = SwingPhase_Cooldown;
         }
      }  break;
      case SwingPhase_Cooldown:
         if (io.buttonPressed[GameButton_RT] && dude.swingTimingSuccess && ++dude.combo < dude.moveset.swings.size()) {
            if (dude.stamina > 0) {
               _beginAttack(game->dude, time, -dude.swingDir, dude.combo);
            }
            else {
               dude.swingTimingSuccess = false;
            }
         }
         else {
            if (swingdt > dude.swing.cooldownDur) {
               dude.phaseStart += dude.swing.cooldownDur;
               dude.state = Dude::State_FREE;
               dude.lastFree = time;
            }
         }
         
         break;
      }
   }



   if (v2LenSquared(dude.velocity) > 0.0f) {

      bool collide = true;
      int tries = 0;

      while (collide) {
         if (tries >= 3) {
            dude.velocity = { 0,0 };
            break;
         }

         collide = false;
         for (auto& d : g_game->dudes) {
            auto projectedDist = v2Dist(dude.pos + dude.velocity * (f32)dt.toMilliseconds(), d.pos);
            auto colDist = dude.size + d.size;
            if (projectedDist < colDist) {
               auto overlap = (colDist - projectedDist) / (f32)dt.toMilliseconds();

               auto overlapv = v2Normalized(dude.velocity) * overlap;
               dude.velocity -= overlapv;

               auto reflect = v2Normalized(v2Orthogonal(d.pos - dude.pos));
               reflect *= v2Dot(dude.velocity, reflect);

               dude.velocity = reflect;
               collide = true;
               ++tries;
               break;
            }
         }
      }

      

      auto velnorm = v2Normalized(dude.velocity);
      auto facnorm = v2Normalized(dude.facing);

      // scale half of mvspeed based on facing;
      f32 facing = v2Dot(velnorm, facnorm);
      auto scaledSpeed = (c.dudeMoveSpeed * 0.85f) + (c.dudeMoveSpeed * 0.15f * facing);

      dp += dude.velocity * scaledSpeed * (f32)dt.toMilliseconds();
   }

   game->dude.lastUpdated = appGetTime();
}

void gameUpdate(Game* game) {   
   
   
   _updateDude(game);

   for (auto& d : game->dudes) {
      d.hit = game->dude.state == Dude::State_ATTACKING && _attackCollision(game->dude, d);
   }
   


   gameDoUI(game);
}

void gameDestroy(Game* game) {
   delete game;
}

FBO const& gameGetOutputFBO(Game* game) {
   return game->fbo;
}
