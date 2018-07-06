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

void gameCreate(StringView assetsFolder) {
   g_game = new Game();
   _gameDataInit(&g_game->data, assetsFolder);
   
}

static void _createGraphicsObjects(){
   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");
   auto game = g_game;

   game->shader = render::shaderBuild(vertex, fragment);

   Vertex vbo[] = { 
      { { 0.0f, 0.0f },{ 0.0f, 0.0f } },
      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },

      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 1.0f, 1.0f },{ 1.0f, 1.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },
   };

   game->mesh = render::meshBuild(vbo, 6);

   auto& res = game->data.constants.resolution;
   game->fbo = render::fboBuild({ res.x, res.y });

   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary("assets/goku.png", &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   game->gokuTex = render::textureBuild((ColorRGBA*)png, { x, y }, {});

   free(mem);
   free(png);
   free(vertex);
   free(fragment);
}

static Dude _createDude() {
   Dude out;
   out.pos = { 50,50 };
   out.size = { 150,250 };
   out.texture = g_game->gokuTex;
   out.mesh = &g_game->mesh;
   return out;
}

bool gameProcessEvent(SDL_Event* event) {
   return true;
}

void gameBegin() {
   
   _createGraphicsObjects();
   g_game->dude = _createDude();

}

static void _renderDude(Dude const& dude) {
   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   model *= Matrix::translate2f(dude.pos);
   model *= Matrix::translate2f({ dude.size.x / 2, dude.size.y / 2 });
   model *= Matrix::rotate2D(dude.rotationAngle);
   model *= Matrix::translate2f({ -dude.size.x / 2, -dude.size.y / 2 });
   model *= Matrix::scale2f(dude.size);

   render::uSetMatrix("uTexMatrix", texmat);
   render::uSetMatrix("uModelMatrix", model);
   
   render::uSetTextureSlot("uDiffuse", 0);
   render::textureBind(dude.texture, 0);

   render::meshRender(*dude.mesh);
}

static void _renderScene() {
   auto game = g_game;
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

void gameUpdate(Window* wnd) {   
   auto game = g_game;

   auto& res = game->data.constants.resolution;
   auto& vpScreen = game->data.imgui.vpScreenArea;
   auto& mPos = ImGui::GetIO().MousePos;

   game->data.io.mousePos = {
      (mPos.x - vpScreen.x) / vpScreen.w * res.x,
      (mPos.y - vpScreen.y) / vpScreen.h * res.y
   };

   auto&m = game->data.io.mousePos;
   auto&dp = game->dude.pos;
   auto&dsz = game->dude.size;

   Float2 rotPos = { dp.x + dsz.x / 2, dp.y + dsz.y / 2 };

   game->dude.rotationAngle = -atan2f(m.x - rotPos.x, m.y - rotPos.y);

   auto up = ImGui::IsKeyDown(SDL_SCANCODE_W);
   auto down = ImGui::IsKeyDown(SDL_SCANCODE_S);
   auto left = ImGui::IsKeyDown(SDL_SCANCODE_A);
   auto right = ImGui::IsKeyDown(SDL_SCANCODE_D);

   float mvSpeed = 5.0f;

   if (up) {
      dp.y -= mvSpeed;
   }
   else if (down) {
      dp.y += mvSpeed;
   }

   if (left) {
      dp.x -= mvSpeed;
   }
   else if (right) {
      dp.x += mvSpeed;
   }

   _renderScene();
   gameDoUI(wnd);
}

void gameDestroy() {
   delete g_game;
}

FBO const& gameGetOutputFBO() {
   return g_game->fbo;
}
