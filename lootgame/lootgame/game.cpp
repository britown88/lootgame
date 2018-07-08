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

struct Dude {
   Float2 size;
   Float2 pos;

   Float2 moveVector;
   Float2 velocity;

   Float2 faceVector;
   Float2 facing = { 0, -1 };


   TextureHandle texture = 0;
   Time lastUpdated;
};

static Constants g_const;
Constants &ConstantsGet() { return g_const; }

struct Attack {
   f32 swipeAngle; // full range of the weapon swipe, in degrees
   Time timeLength; // total time for the attack
   Rectf hitbox; // axis-aligned, The origin here is the bottom center of the attack while facing up
                 // attack will rotate around that origin
};



struct Game {
   GameData data;

   ShaderHandle shader = 0;

   TextureHandle dudeTex = 0;
   TextureHandle targetTex = 0;
   TextureHandle lightTex = 0;

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

static TextureHandle _textureBuildFromFile(const char* path) {
   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary(path, &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   auto out = render::textureBuild((ColorRGBA*)png, { x, y }, {});

   free(mem);
   free(png);
   return out;
}

static void _createGraphicsObjects(Game* game){
   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   game->shader = render::shaderBuild(vertex, fragment);

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

   game->dudeTex = _textureBuildFromFile("assets/dude.png");
   game->targetTex = _textureBuildFromFile("assets/target.png");
   game->lightTex = _textureBuildFromFile("assets/light.png");

   
   free(vertex);
   free(fragment);
}

static Dude _createDude(Game* game) {
   Dude out;
   out.pos = { 50,50 };
   out.size = { 100, 60 };
   out.texture = game->dudeTex;
   out.lastUpdated = appGetTime();
   return out;
}

#define AXIS_DEADZONE 0.15f

bool gameProcessEvent(Game*game, SDL_Event* event) {
   auto &io = game->data.io;
   
   switch (event->type) {
   case SDL_CONTROLLERDEVICEADDED:
      SDL_GameControllerOpen(0);
      return true;

   case SDL_CONTROLLERAXISMOTION:
   {  
      auto value = event->caxis.value / (float)(INT16_MAX + 1);
      if (fabs(value) < AXIS_DEADZONE) {
         value = 0.0f;
      }

      switch (event->caxis.axis) {
      case SDL_CONTROLLER_AXIS_LEFTX: io.leftStick.x = value; break;
      case SDL_CONTROLLER_AXIS_LEFTY: io.leftStick.y = value; break;
      case SDL_CONTROLLER_AXIS_RIGHTX: io.rightStick.x = value; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: io.rightStick.y = value; break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT: io.leftTrigger = value; break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: io.rightTrigger = value; break;
      default: return false;
      }
   } return true;;
   }

   return false;
}

void gameBegin(Game*game) {
   
   _createGraphicsObjects(game);
   game->dude = _createDude(game);

}

static void _renderDude(Dude const& dude) {
   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   model *= Matrix::translate2f(dude.pos);
   model *= Matrix::rotate2D(atan2f(dude.facing.x, -dude.facing.y));
   model *= Matrix::scale2f(dude.size);

   render::uSetColor("uColor", White);
   render::uSetMatrix("uTexMatrix", texmat);
   render::uSetMatrix("uModelMatrix", model);
   
   render::uSetTextureSlot("uDiffuse", 0);
   render::textureBind(dude.texture, 0);

   render::meshRender(g_game->mesh);
}

static void _renderTarget(Float2 pos, ColorRGBAf color, f32 sz) {
   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   model *= Matrix::translate2f(pos);
   model *= Matrix::scale2f({ sz,sz });

   render::uSetColor("uColor", color);
   render::uSetMatrix("uTexMatrix", texmat);
   render::uSetMatrix("uModelMatrix", model);
   render::uSetTextureSlot("uDiffuse", 0);
   render::textureBind(g_game->targetTex, 0);
   render::meshRender(g_game->mesh);
}

static void _renderLightLayer(Game* game) {

}

static void _renderScene(Game* game) {
   auto& c = ConstantsGet();
   auto& res = ConstantsGet().resolution;

   auto view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
   render::shaderSetActive(game->shader);
   render::uSetMatrix("uViewMatrix", view);


   //{
   //   render::fboBind(game->lightfbo);
   //   render::setBlendMode(BlendMode_NORMAL);
   //   render::viewport({ 0,0, res.x, res.y });
   //   render::clear({0.0f, 0.0f, 0.0f, 0.5f});

   //   auto model = Matrix::identity();
   //   auto texmat = Matrix::identity();

   //   model *= Matrix::translate2f(game->dude.pos);
   //   model *= Matrix::scale2f({ 1000,1000 });

   //   render::uSetColor("uColor", {1.0f, 0.5f, 0.5f, 1.0f});
   //   render::uSetMatrix("uTexMatrix", texmat);
   //   render::uSetMatrix("uModelMatrix", model);
   //   render::uSetTextureSlot("uDiffuse", 0);
   //   render::textureBind(g_game->lightTex, 0);
   //   render::meshRender(g_game->mesh);

   //   {auto model = Matrix::identity();
   //   auto texmat = Matrix::identity();

   //   model *= Matrix::translate2f({0, 0});
   //   model *= Matrix::scale2f({ 1000,1000 });
   //   render::uSetMatrix("uModelMatrix", model);
   //   render::meshRender(g_game->mesh);
   //   }

   //}
   



   render::fboBind(game->fbo);

   render::setBlendMode(BlendMode_NORMAL);
   render::viewport({ 0,0, res.x, res.y });
   render::clear(DkBlue);



   _renderDude(game->dude);

   const static f32 moveTargetDist = 100.0f;
   const static f32 aimTargetDist = 200.0f;
   auto &io = game->data.io;
   auto &dude = game->dude;

   if (game->data.imgui.showMovementDebugging) {
      _renderTarget(dude.pos + dude.velocity * moveTargetDist, Cyan, 40);
      _renderTarget(dude.pos + v2Normalized(dude.facing) * aimTargetDist, Red, 40);

      _renderTarget(dude.pos + io.leftStick * moveTargetDist, DkGreen, 30);
      _renderTarget(dude.pos + dude.moveVector * moveTargetDist, Magenta, 30);   

      _renderTarget(dude.pos + io.rightStick * aimTargetDist, Yellow, 30);
      _renderTarget(dude.pos + dude.faceVector * aimTargetDist, LtGray, 30);
   }

   //{
   //   render::setBlendMode(BlendMode_LIGHTING);
   //   auto model = Matrix::identity();
   //   auto texmat = Matrix::identity();

   //   model *= Matrix::scale2f({ (float)game->lightfbo.sz.x, (float)game->lightfbo.sz.y });

   //   render::uSetColor("uColor", White);
   //   render::uSetMatrix("uTexMatrix", texmat);
   //   render::uSetMatrix("uModelMatrix", model);
   //   render::uSetTextureSlot("uDiffuse", 0);
   //   render::textureBind(game->lightfbo.tex, 0);
   //   render::meshRender(g_game->meshUncentered);
   //}

   render::fboBind({});
}

void gameHandleInput(Game*game) {
   auto& res = ConstantsGet().resolution;
   auto& vpScreen = game->data.imgui.vpScreenArea;
   auto& mPos = ImGui::GetIO().MousePos;

   game->data.io.mousePos = {
      (mPos.x - vpScreen.x) / vpScreen.w * res.x,
      (mPos.y - vpScreen.y) / vpScreen.h * res.y
   };
}

void gameRender(Game*game) {
   _renderScene(game);
}

static void _updateDude(Game* game) {
   auto dt = appGetTime() - game->dude.lastUpdated;
   if (dt.toMilliseconds() == 0) {
      return;
   }

   auto&m = game->data.io.mousePos;
   auto&dp = game->dude.pos;
   auto &io = game->data.io;
   auto &dude = game->dude;

   auto& c = ConstantsGet();

   dude.moveVector = v2MoveTowards(dude.moveVector, io.leftStick, c.stickTrackingSpeed * dt.toMilliseconds());

   Float2 aimStick;
   bool aiming = false;
   if (fabs(io.rightStick.x) > AXIS_DEADZONE || fabs(io.rightStick.y) > AXIS_DEADZONE) {
      aimStick = io.rightStick;
      aiming = true;
   }
   else if (fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE) {
      aimStick = io.leftStick;
      aiming = true;
   }


   dude.faceVector = v2MoveTowards(dude.faceVector, aimStick, c.stickTrackingSpeed * dt.toMilliseconds());

   if (v2Dot(dude.faceVector, dude.faceVector) > 0.0f) {
      dude.facing = v2RotateTowards(dude.facing, dude.faceVector, v2FromAngle(c.dudeRotationSpeed * dt.toMilliseconds()));
   }
   


   dude.velocity = v2MoveTowards(dude.velocity, dude.moveVector, c.dudeAcceleration * dt.toMilliseconds());

   // scale half of mvspeed based on facing;
   f32 facing = 0.0f;
   if (v2Len(dude.velocity) > 0.0001f) {
      facing = v2Dot(v2Normalized(dude.velocity), v2Normalized(dude.facing));
   }

   auto scaledSpeed = (c.dudeMoveSpeed * 0.75f) + (c.dudeMoveSpeed * 0.25f * facing);

   dp += dude.velocity * scaledSpeed;

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
