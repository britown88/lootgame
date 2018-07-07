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
   Float2 size = {};
   Float2 pos = {};
   float rotationAngle = 0.0f;

   TextureHandle texture = 0;
   Mesh const* mesh = nullptr;
};


struct Game {
   GameData data;

   ShaderHandle shader = 0;
   TextureHandle gokuTex = 0;
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

   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary("assets/dude.png", &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   game->gokuTex = render::textureBuild((ColorRGBA*)png, { x, y }, {});

   free(mem);
   free(png);
   free(vertex);
   free(fragment);
}

static Dude _createDude(Game* game) {
   Dude out;
   out.pos = { 50,50 };
   out.size = { 150, 90 };
   out.texture = game->gokuTex;
   out.mesh = &game->mesh;
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
      case SDL_CONTROLLER_AXIS_LEFTX: io.moveVector.x = value; break;
      case SDL_CONTROLLER_AXIS_LEFTY: io.moveVector.y = value; break;
      case SDL_CONTROLLER_AXIS_RIGHTX: io.aimVector.x = value; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: io.aimVector.y = value; break;
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
   model *= Matrix::rotate2D(dude.rotationAngle);
   model *= Matrix::scale2f(dude.size);

   render::uSetMatrix("uTexMatrix", texmat);
   render::uSetMatrix("uModelMatrix", model);
   
   render::uSetTextureSlot("uDiffuse", 0);
   render::textureBind(dude.texture, 0);

   render::meshRender(*dude.mesh);
}

static void _renderScene(Game* game) {
   auto& res = game->data.constants.resolution;

   render::fboBind(game->fbo);

   render::enableAlphaBlending(true);
   render::viewport({ 0,0, res.x, res.y });
   render::clear(DkGreen);

   auto view = Matrix::ortho(0, (float)res.x, 0, (float)res.y, -1, 1);
   render::shaderSetActive(game->shader);

   render::uSetMatrix("uViewMatrix", view);

   _renderDude(game->dude);

   render::fboBind({});
}

static void _updateMouse(Game* game) {
   auto& res = game->data.constants.resolution;
   auto& vpScreen = game->data.imgui.vpScreenArea;
   auto& mPos = ImGui::GetIO().MousePos;

   game->data.io.mousePos = {
      (mPos.x - vpScreen.x) / vpScreen.w * res.x,
      (mPos.y - vpScreen.y) / vpScreen.h * res.y
   };
}

void gameHandleInput(Game*game) {
   _updateMouse(game);
}

void gameRender(Game*game) {
   _renderScene(game);
}

void gameUpdate(Game* game) {   
   
   auto&m = game->data.io.mousePos;
   auto&dp = game->dude.pos;
   auto &io = game->data.io;

   if (fabs(io.aimVector.x) > AXIS_DEADZONE || fabs(io.aimVector.y) > AXIS_DEADZONE) {
      game->dude.rotationAngle = atan2f(io.aimVector.x, -io.aimVector.y);
   }
   else if (fabs(io.moveVector.x) > AXIS_DEADZONE || fabs(io.moveVector.y) > AXIS_DEADZONE) {
      game->dude.rotationAngle = atan2f(io.moveVector.x, -io.moveVector.y);
   }

   float mvSpeed = 5.0f;

   dp.y += mvSpeed * game->data.io.moveVector.y;
   dp.x += mvSpeed * game->data.io.moveVector.x;


   gameDoUI(game);
}

void gameDestroy(Game* game) {
   delete game;
}

FBO const& gameGetOutputFBO(Game* game) {
   return game->fbo;
}
