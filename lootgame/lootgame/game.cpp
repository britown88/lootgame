#include "game.h"
#include "app.h"
#include "win.h"
#include "render.h"
#include "imgui.h"
#include "phy.h"

#include <vector>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stb/stb_image.h>



#define AXIS_DEADZONE 0.25f
#define DUDE_COUNT 100

static Constants g_const;
Constants &ConstantsGet() { return g_const; }

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
   DynamicArray<AttackSwing> swings;
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

const f32 dudeSpeedCapEasing = 0.01f;
const f32 dudeAcceleration = 0.001f;
const f32 dudeMoveSpeed = 0.4f;
const f32 dudeRotationSpeed = 0.01f;

struct Movement {
   f32 moveSpeedCap = 0.0f;       // updated per frame, interpolates toward moveSpeedCapTarget
   f32 moveSpeedCapTarget = 0.0f; // updated per frame, max speed based on length of move vector and velocity direction vs facing
   Float2 moveVector = { 0.0f, 0.0f };   // vector length 0-1 for movement amount/direction
   Float2 faceVector = { 0.0f, 0.0f };  // unit vector for target facing, facing will interpolate toward this angle
   Float2 facing = { 1, 0 };  // unit vector for character facing  
};

struct Behavior {
   Float2 targetPos = { 0,0 };
   Time started;
};

struct Dude {
   enum State {
      State_FREE = 0,
      State_ATTACKING,
   };

   State state = State_FREE;

   Float2 renderSize;
   ColorRGBAf c;

   PhyObject phy;
   Movement mv;
   Behavior ai;

   TextureHandle texture = 0;
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
   
};






void dudeUpdateVelocity(Dude& d) {
   // we assume at this point the moveVector is current

   // scale mvspeed based on facing;
   f32 facedot = v2Dot(v2Normalized(d.phy.velocity), d.mv.facing);
   auto scaledSpeed = (dudeMoveSpeed * 0.85f) + (dudeMoveSpeed * 0.15f * facedot);

   // set the target speed
   d.mv.moveSpeedCapTarget = scaledSpeed * v2Len(d.mv.moveVector);

   // ease speed cap toward target
   if (d.mv.moveSpeedCap < d.mv.moveSpeedCapTarget) {
      d.mv.moveSpeedCap += dudeSpeedCapEasing;
   }
   else {
      d.mv.moveSpeedCap -= dudeSpeedCapEasing;
   }

   // add the movevector scaled against acceleration to velocity and cap it
   d.phy.velocity = v2CapLength(d.phy.velocity + d.mv.moveVector * dudeAcceleration, d.mv.moveSpeedCap);
}

// rotate facing vector toward the facevector
void dudeUpdateRotation(Dude& d) {
   if (v2LenSquared(d.mv.faceVector) > 0.0f) {
      d.mv.facing = v2Normalized(v2RotateTowards(d.mv.facing, d.mv.faceVector, v2FromAngle(dudeRotationSpeed)));
   }
}


struct Game {
   GameData data;

   ShaderHandle shader = 0, colorShader = 0;

   Mesh mesh, meshUncentered;
   FBO fbo, lightfbo;

   Dude maindude;
   DynamicArray<Dude> baddudes;

   Time lastMouseMove;
   Time lastUpdate;
   bool mouseActive;
};

static Game* g_game = nullptr;
GameData* gameDataGet() {
   return &g_game->data;
}


static bool rightStickActive() {
   auto &io = g_game->data.io;
   return fabs(io.rightStick.x) > AXIS_DEADZONE || fabs(io.rightStick.y) > AXIS_DEADZONE;
}
static bool leftStickActive() {
   auto &io = g_game->data.io;
   return fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE;
}

void dudeApplyInputMovement(Dude& d) {
   auto& io = gameDataGet()->io;

   d.mv.moveVector = io.leftStick;

   bool rstick = rightStickActive();
   bool lstick = leftStickActive();

   Float2 aimStick;
   if (rstick) {
      aimStick = io.rightStick;
      g_game->mouseActive = false;
   }
   else if (lstick) {
      aimStick = io.leftStick;
   }

   if (g_game->mouseActive) {
      aimStick = io.mousePos - d.phy.pos;
   }

   if (v2LenSquared(aimStick) > 0) {
      d.mv.faceVector = v2Normalized(aimStick);
   }
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

static TextureHandle _mklighttexture() {
   Float2 sz = { 720, 720 };
   auto mem = new ColorRGBA[sz.x * sz.y];
   for (int y = 0; y < sz.y; ++y) {
      for (int x = 0; x < sz.x; ++x) {
         auto dist = v2Dist({ (f32)x, (f32)y }, { 360.0f, 360.0f }) / 360.0f;
         auto idx = y * 360 + x;

         ColorRGBAf fc;

         if (dist > 0) {
            fc = { 1 - dist , 1 - dist , 1 - dist , 1 - dist };
         }
         else {
            fc = { 0 , 0 , 0 , 0 };
         }

         mem[idx] = linearToSrgb(fc);
      }
   }

   auto out = render::textureBuild((ColorRGBA*)mem, { 720, 720 }, { RepeatType_CLAMP , FilterType_NEAREST });
   delete[] mem;
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
   g_textures[GameTextures_Light] = _textureBuildFromFile("assets/light3.png");
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
   out.phy.pos = { 50,50 };
   out.phy.circle.size = 30.0f;
   out.phy.maxSpeed = dudeMoveSpeed;
   out.phy.invMass = 0.0f;
   out.renderSize = { 60, 100 };
   out.texture = g_textures[GameTextures_Dude];

   out.stamina = out.staminaMax = 4;

   return out;
}

static Dude _createEnemy(Float2 pos, f32 size) {
   Dude out;

   out.moveset = _createMoveSet();
   out.c = {1.0f, 0.3f, 0.3f, 1.0f};
   out.phy.pos = pos;
   out.phy.circle.size = size;
   out.phy.velocity = { 0,0 };
   out.phy.maxSpeed = dudeMoveSpeed;
   out.phy.invMass =(f32)(rand()%50 + 5) / 100.0f;
   out.renderSize = { size * 2.5f, size * 4.2f };
   out.texture = g_textures[GameTextures_Dude];
   out.stamina = out.staminaMax = 4;

   out.ai.targetPos = { (f32)(rand() % 1820) + 100, (f32)(rand() % 980) + 100 };
   out.ai.started = appGetTime();

   return out;
}



void gameBegin(Game*game) {
   game->lastUpdate = appGetTime();

   _createGraphicsObjects(game);
   game->maindude = _createDude(game);

   for (int i = 0; i < DUDE_COUNT; ++i) {
      auto e = _createEnemy({ (f32)(rand() % 1820) + 100, (f32)(rand() % 980) + 100 }, 30.0f);
      game->baddudes.push_back(e);
   }

}

static void _renderSwing(Dude&dude) {
   auto origin = dude.phy.pos + dude.weaponVector * dude.phy.circle.size;

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

   model *= Matrix::translate2f(dude.phy.pos);
   model *= Matrix::rotate2D(v2Angle(dude.mv.facing));
   model *= Matrix::scale2f(dude.renderSize);

   render::uSetColor(u::color, dude.c);
   render::uSetMatrix(u::texMatrix, texmat);
   render::uSetMatrix(u::modelMatrix, model);
   
   render::uSetTextureSlot(u::diffuse, 0);
   render::textureBind(dude.texture, 0);
   render::meshRender(g_game->mesh);

   if (gameDataGet()->imgui.showCollisionDebugging) {

      model = Matrix::translate2f(dude.phy.pos);
      model *= Matrix::scale2f({ dude.phy.circle.size * 2, dude.phy.circle.size * 2 });
      render::uSetColor(u::color, Cyan);
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

   _addLight({ 500, 500 }, game->maindude.phy.pos, Yellow);

   f32 lsz = 250;
   _addLight({ lsz, lsz }, { 200, 600 }, Yellow);
   _addLight({ lsz, lsz }, { 800, 300 }, Yellow);
   _addLight({ lsz, lsz }, { 100, 1000 }, Yellow);
   _addLight({ lsz, lsz }, { 1200, 500 }, Yellow);
   _addLight({ lsz, lsz }, { 1800, 200 }, Yellow);
   _addLight({ lsz, lsz }, { 1800, 800 }, Yellow);
   _addLight({ lsz, lsz }, { 900, 800 }, Yellow);


   render::shaderSetActive(game->colorShader);
   render::uSetMatrix(u::viewMatrix, view);

   auto model = Matrix::scale2f({ (f32)res.x, (f32)res.y});
   auto al = gameDataGet()->imgui.ambientLight;
   render::uSetColor(u::color, sRgbToLinear(ColorRGBAf{ al,al,al,al }));
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
      render::uSetColor(u::color, circleVsAabb(game->maindude.phy.pos, game->maindude.phy.circle.size, testrect) ? Red : Cyan);
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

   for (int i = 0; i < game->maindude.staminaMax; ++i) {
      auto model = Matrix::translate2f(staminaCorner + Float2{(gemSize.x + gemSpace) * i, 0}) *  Matrix::scale2f(gemSize);
      render::uSetMatrix(u::modelMatrix, model);
      render::textureBind(g_textures[i < game->maindude.stamina ? GameTextures_GemFilled : GameTextures_GemEmpty], 0);
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

   for (auto&& d : game->baddudes) {
      _renderDude(d);
   }

   _renderDude(game->maindude);

   //_renderDude(*game->maindude);

   if (game->data.imgui.showMovementDebugging) {
      const static f32 moveTargetDist = 100.0f;
      const static f32 aimTargetDist = 200.0f;
      auto &io = game->data.io;
      auto &dude = game->maindude;

      _renderTarget(dude.phy.pos + dude.phy.velocity * moveTargetDist, Cyan, 40);
      _renderTarget(dude.phy.pos + v2Normalized(dude.mv.facing) * aimTargetDist, Red, 40);

      _renderTarget(dude.phy.pos + io.leftStick * moveTargetDist, DkGreen, 30);
      _renderTarget(dude.phy.pos + dude.mv.moveVector * moveTargetDist, Magenta, 30);

      _renderTarget(dude.phy.pos + io.rightStick * aimTargetDist, Yellow, 30);
      _renderTarget(dude.phy.pos + dude.mv.faceVector * aimTargetDist, LtGray, 30);
   }
   
   _renderLightLayer(game);

   _renderGameUI(game);

   

   //render::setBlendMode(BlendMode_NORMAL);
   //glBlendFunc(GL_ONE, GL_ZERO);

   //render::shaderSetActive(game->shader);
   //render::fboBind( game->outputfbo );
   //
   //render::viewport({ 0,0, res.x, res.y });
   //render::clear({ 1, 1, 1, 1 });

   //render::uSetColor(u::color, {1, 1, 1, 1});
   //render::uSetMatrix(u::viewMatrix, Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1));
   //render::uSetMatrix(u::texMatrix, Matrix::identity());
   //render::uSetMatrix(u::modelMatrix, Matrix::scale2f({ (float)game->outputfbo.sz.x, (float)game->outputfbo.sz.y }));
   //render::uSetTextureSlot(u::diffuse, 0);
   //render::textureBind(game->fbo.tex, 0);

   //glEnable(GL_FRAMEBUFFER_SRGB);
   //render::meshRender(g_game->meshUncentered);
   //glDisable(GL_FRAMEBUFFER_SRGB);

   render::fboBind({});

   

   //render::shaderSetActive(game->shader);
   //render::viewport({ 0, 0, res.x, res.y });
   //render::clear({ 1, 1, 1, 1 });
   //render::uSetColor(u::color, { 1, 1, 1, 1 });
   //render::uSetMatrix(u::viewMatrix, Matrix::ortho(0, (float)res.x, (float)res.y, 0, -1, 1));
   //render::uSetMatrix(u::texMatrix, Matrix::identity());
   //render::uSetMatrix(u::modelMatrix, Matrix::scale2f({ (float)game->outputfbo.sz.x, (float)game->outputfbo.sz.y }));
   //render::uSetTextureSlot(u::diffuse, 0);
   //render::textureBind(game->outputfbo.tex, 0);

   //
   //render::meshRender(g_game->meshUncentered);
   

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
   auto &imio = ImGui::GetIO();
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




bool _attackCollision(Dude& attacker, Dude& defender) {
   auto origin = attacker.phy.pos + attacker.weaponVector * attacker.phy.circle.size;
   auto defendPos = v2Rotate(defender.phy.pos - origin, v2Conjugate(attacker.weaponVector));

   auto wbox = attacker.moveset.swings[attacker.combo].hitbox;
   //wbox.x += origin.x;
   //wbox.y += origin.y;

   return circleVsAabb(defendPos, defender.phy.circle.size, wbox);
}


static void _beginAttack(Dude& dude, Time t, int dir, int combo) {
   dude.combo = combo;
   dude.swing = dude.moveset.swings[combo];

   dude.state = Dude::State_ATTACKING;
   dude.mv.moveVector = dude.mv.faceVector = { 0.0f, 0.0f };
   
   dude.weaponVector = v2Normalized(v2Rotate(dude.mv.facing, v2FromAngle(dir * dude.swing.swipeAngle / 2.0f * DEG2RAD)));
   dude.swingPhase = SwingPhase_Windup;
   dude.phaseStart = t;
   dude.swingDir = dir;

   dude.swingTimingSuccess = true;
}



void dudeUpdateBehavior(Dude& dude) {
   if (v2Dist(dude.phy.pos, dude.ai.targetPos) < dude.phy.circle.size || appGetTime() - dude.ai.started > timeSecs(10)) {
      dude.ai.targetPos = { (f32)(rand() % 1820), (f32)(rand() % 980) };
      dude.ai.started = appGetTime();
   }
   else{
      auto vec = v2Normalized(dude.ai.targetPos - dude.phy.pos);
      dude.mv.moveVector = dude.mv.faceVector = vec;
   }
}

static void _updateDude(Game* game, Milliseconds ms) {
   auto time = appGetTime();
   
   if (ms == 0) {
      return;
   }

   auto&m = game->data.io.mousePos;
   auto& dp = game->maindude.phy.pos;
   auto& io = game->data.io;
   auto& dude = game->maindude;

   auto& c = ConstantsGet();


   // movement/aiming
   

   // handle inputs
   if (game->maindude.state == Dude::State_FREE) {

      auto freedt = time - dude.lastFree;
      if (freedt > timeMillis(500) && dude.stamina < dude.staminaMax) {
         dude.stamina = dude.staminaMax;
      }

      // triggers
      if (dude.stamina > 0 && io.buttonPressed[GameButton_RT]) {
         _beginAttack(game->maindude, time, -1, 0);
      }
   }
   else if (game->maindude.state == Dude::State_ATTACKING) {
      auto swingdt = time - game->maindude.phaseStart;

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
         dude.weaponVector = v2Rotate(dude.weaponVector, v2FromAngle(-dude.swingDir * radsPerMs * ms));

         if (dude.swing.lungeSpeed > 0.0f) {
            //Float2 lungeVel = dude.facing * dude.swing.lungeSpeed;
            //bool collided = true;
            //int tries = 0;
            //while (collided) {
            //   collided = false;
            //   for (auto& d : g_game->dudes) {
            //      if (_dudeCollision(dude.size, dude.pos, lungeVel, d.size, d.pos, dt, lungeVel)) {
            //         collided = true;
            //         ++tries;
            //         break;
            //      }
            //   }
            //   if (tries >= 3) {
            //      lungeVel = { 0,0 };
            //      break;
            //   }
            //}
            //dp += lungeVel * (f32)dt.toMilliseconds();
         }
         
         

         

         if (swingdt > dude.swing.swingDur) {
            dude.phaseStart += dude.swing.swingDur;
            dude.weaponVector = v2Normalized(v2Rotate(dude.mv.facing, v2FromAngle(-dude.swingDir * dude.swing.swipeAngle / 2.0f * DEG2RAD)));
            dude.swingPhase = SwingPhase_Cooldown;
         }
      }  break;
      case SwingPhase_Cooldown:
         if (io.buttonPressed[GameButton_RT] && dude.swingTimingSuccess && ++dude.combo < dude.moveset.swings.size()) {
            if (dude.stamina > 0) {
               _beginAttack(game->maindude, time, -dude.swingDir, dude.combo);
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

}

void gameUpdate(Game* game) {   
   auto time = appGetTime();
   auto dt = time - game->lastUpdate;
   auto ms = dt.toMilliseconds();

   if (ms > 32) {
      // something bad happened and we spiked hard
      ms = 32;
      game->lastUpdate = time - timeMillis(ms);
   }

   // build physics system
   DynamicArray<PhyObject*> pObjs;
   pObjs.push_back(&game->maindude.phy);
   for (auto && d : game->baddudes) {
      pObjs.push_back(&d.phy);
   }

   for (Milliseconds i = 0; i < ms; ++i) {

      dudeApplyInputMovement(game->maindude);   
      dudeUpdateRotation(game->maindude);
      dudeUpdateVelocity(game->maindude);
      

      for (auto && d : game->baddudes) {
         dudeUpdateBehavior(d);  
         dudeUpdateRotation(d);
         dudeUpdateVelocity(d);
      }

      updatePhyPositions(pObjs);
   }

   game->lastUpdate += timeMillis(ms);

   // imgui output
   gameDoUI(game);
}

void gameDestroy(Game* game) {
   delete game;
}

FBO const& gameGetOutputFBO(Game* game) {
   return game->fbo;
}
