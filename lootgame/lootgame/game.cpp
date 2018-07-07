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


struct Game {
   GameData data;

   ShaderHandle shader = 0;
   TextureHandle dudeTex = 0;
   TextureHandle targetTex = 0;

   Mesh mesh;
   FBO fbo;

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

   game->mesh = render::meshBuild(vbo, 6);

   auto& res = game->data.constants.resolution;
   game->fbo = render::fboBuild({ res.x, res.y });

   game->dudeTex = _textureBuildFromFile("assets/dude.png");
   game->targetTex = _textureBuildFromFile("assets/target.png");

   
   free(vertex);
   free(fragment);
}

static Dude _createDude(Game* game) {
   Dude out;
   out.pos = { 50,50 };
   out.size = { 150, 90 };
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

static void _renderScene(Game* game) {
   auto& res = game->data.constants.resolution;

   render::fboBind(game->fbo);

   render::enableAlphaBlending(true);
   render::viewport({ 0,0, res.x, res.y });
   render::clear(DkBlue);

   auto view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
   render::shaderSetActive(game->shader);

   render::uSetMatrix("uViewMatrix", view);

   _renderDude(game->dude);

   const static f32 moveTargetDist = 100.0f;
   const static f32 aimTargetDist = 200.0f;
   auto &io = game->data.io;
   auto &dude = game->dude;

   
   _renderTarget(dude.pos + dude.velocity * moveTargetDist, Cyan, 25);
   _renderTarget(dude.pos + v2Normalized(dude.facing) * aimTargetDist, Red, 25);

   _renderTarget(dude.pos + io.leftStick * moveTargetDist, DkGreen, 15);
   _renderTarget(dude.pos + dude.moveVector * moveTargetDist, Magenta, 15);   

   _renderTarget(dude.pos + io.rightStick * aimTargetDist, Yellow, 15);
   _renderTarget(dude.pos + dude.faceVector * aimTargetDist, Magenta, 15);
   

   render::fboBind({});
}

void gameHandleInput(Game*game) {
   auto& res = game->data.constants.resolution;
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

   const f32 trackingSpeed = 0.1f;
   const f32 acceleration = 0.005f;
   const f32 mvSpeed = 5.0f;
   const f32 rotSpeed = 0.01f;

   auto moveTrackDist = v2Dist(dude.moveVector, io.leftStick);
   if (moveTrackDist > 0.0001f) {
      //scale speed to distance
      auto speed = trackingSpeed * (moveTrackDist / 2.0f);
      dude.moveVector = v2MoveTowards(dude.moveVector, io.leftStick, speed * dt.toMilliseconds());
   }

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

   if (aiming) {
      auto aimTrackDist = v2Dist(dude.faceVector, aimStick);
      if (aimTrackDist > 0.0001f) {
         //scale speed to distance
         auto speed = trackingSpeed * (aimTrackDist / 2.0f);
         dude.faceVector = v2MoveTowards(dude.faceVector, aimStick, speed * dt.toMilliseconds());
      }

      dude.facing = v2RotateTowards(dude.facing, dude.faceVector, v2FromAngle(rotSpeed * dt.toMilliseconds()));
   }

   dude.velocity = v2MoveTowards(dude.velocity, dude.moveVector, acceleration * dt.toMilliseconds());

   // scale half of mvspeed based on facing;
   f32 facing = 0.0f;
   if (v2Len(dude.velocity) > 0.0001f) {
      facing = v2Dot(v2Normalized(dude.velocity), v2Normalized(dude.facing));
   }

   auto scaledSpeed = (mvSpeed * 0.75f) + (mvSpeed * 0.25f * facing);

   dp += dude.velocity * scaledSpeed;

   game->dude.lastUpdated = appGetTime();

   //Float2 targetFace = {};

   //if (fabs(io.rightStick.x) > AXIS_DEADZONE || fabs(io.rightStick.y) > AXIS_DEADZONE) {
   //   targetFace = v2Normalized({ io.rightStick.x, io.rightStick.y });
   //}
   //else if (fabs(io.leftStick.x) > AXIS_DEADZONE || fabs(io.leftStick.y) > AXIS_DEADZONE) {
   //   targetFace = v2Normalized({ io.leftStick.x, io.leftStick.y });

   //}
   //else {
   //   //targetFace = dude.facing;
   //}

   //f32 angleinc = 0.1f;
   ////dude.facing = v2RotateTowards(dude.facing, targetFace, v2FromAngle(0.1f));
   ////dude.rotationAngle = atan2f(dude.facing.x, -dude.facing.y);


   //const float mvSpeed = 5.0f;

   //dp.y += mvSpeed * io.leftStick.y;
   //dp.x += mvSpeed * io.leftStick.x;


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
