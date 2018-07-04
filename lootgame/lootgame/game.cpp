#include "game.h"
#include "app.h"

#include "imgui.h"
#include "scf.h"
#include <unordered_map>
#include <vector>

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
GameData* gameGet() {
   return &g_game->data;
}

static void _gameDataInit(GameData* game, StringView assetsFolder) {
   
}


Game* gameCreate(StringView assetsFolder) {
   auto out = new Game();
   _gameDataInit(&out->data, assetsFolder);
   g_game = out;
   return out;
}

GameData* gameData(Game* game) { return &game->data; }

static void _createGraphicsObjects(Game* game){
   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

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

static Dude _createDude(Game* game) {
   Dude out;
   out.pos = { 50,50 };
   out.size = { 205,250 };
   out.texture = game->gokuTex;
   out.mesh = &game->mesh;
   return out;
}



void gameBegin(Game* game, Window* wnd) {
   
   _createGraphicsObjects(game);
   game->dude = _createDude(game);

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

void gameUpdate(Game* game, Window* wnd) {   



   _renderScene(game);
   gameDoUI(wnd);
}

void gameDestroy(Game* game) {
   delete game;
}

FBO const& gameGetOutputFBO() {
   return g_game->fbo;
}
