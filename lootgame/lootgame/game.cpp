#include "game.h"
#include "app.h"

#include "imgui.h"
#include "scf.h"
#include <unordered_map>
#include <vector>

#include <stb/stb_image.h>
#include "win.h"
#include "render.h"


struct Game {
   GameData data;

   ShaderHandle shader = 0;
   TextureHandle texture = 0;
   Mesh mesh;
   FBO fbo;
};

static GameData* g_gameData = nullptr;
GameData* gameGet() {
   return g_gameData;
}

static void _gameDataInit(GameData* game, StringView assetsFolder) {
   
}


Game* gameCreate(StringView assetsFolder) {
   auto out = new Game();
   _gameDataInit(&out->data, assetsFolder);
   g_gameData = &out->data;
   return out;
}

GameData* gameData(Game* game) { return &game->data; }

void gameBegin(Game* game, Window* wnd) {
   

   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   game->shader = render::shaderBuild(vertex, fragment);

   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;

   auto mem = fileReadBinary("assets/goku.png", &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);

   game->texture = render::textureBuild((ColorRGBA*)png, { x, y }, {});

   Vertex vbo[] = { 
      { { 0.0f, 0.0f },{ 0.0f, 0.0f } },
      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },

      { { 1.0f, 0.0f },{ 1.0f, 0.0f } },
      { { 1.0f, 1.0f },{ 1.0f, 1.0f } },
      { { 0.0f, 1.0f },{ 0.0f, 1.0f } },
   };

   game->mesh = render::meshBuild(vbo, 6);

   game->fbo = render::fboBuild({ 100, 100 });

   free(vertex);
   free(fragment);
   free(mem);
   free(png);

   windowAddGUI(wnd, "viewer", [=](Window*wnd) {
      if (ImGui::Begin("Viewer")) {
         ImGui::Image((ImTextureID)game->fbo.tex, { 100, 100 });
      }
      ImGui::End();

      return true;
   });
}

void gameUpdate(Game* game, Window* wnd) {

   render::fboBind(game->fbo);

   render::enableAlphaBlending(true);
   render::viewport({ 0,0,100,100 });
   render::clear(DkGreen);

   auto model = Matrix::identity();
   auto texmat = Matrix::identity();

   auto view = Matrix::ortho(0, 100, 0, 100, -1, 1);
   
   
   model *= Matrix::translate2f({ 25,25 });

   static float angle = 0.0f;
   angle = (int)(angle + 1) % 360;

   model *= Matrix::translate2f({ 25,25 });
   model *= Matrix::rotate2D(angle);
   model *= Matrix::translate2f({ -25,-25 });

   model *= Matrix::scale2f({ 50.0f,50.0f });
   

   render::shaderSetActive(game->shader);

   render::uSetMatrix("uTexMatrix", texmat);
   render::uSetMatrix("uModelMatrix", model);
   render::uSetMatrix("uViewMatrix", view);
   render::uSetTextureSlot("uDiffuse", 0);

   render::textureBind(game->texture, 0);

   render::meshRender(game->mesh);

   render::fboBind({});


   gameDoUI(wnd);
}

void gameDestroy(Game* game) {
   delete game;
}