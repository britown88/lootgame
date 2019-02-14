#include "assets.h"

#include "win.h"
#include "game.h"
#include <stb/stb_image.h>
#include <assert.h>



static int _reloadShader(GraphicObjects &gfx) {

   auto vertex = fileReadString("assets/vertex.glsl");
   auto fragment = fileReadString("assets/fragment.glsl");

   auto s = render::shaderBuild(vertex, fragment);

   free(vertex);
   free(fragment);

   if (s == 0) {
      return 0;
   }

   if (gfx.shader != 0) {
      render::shaderDestroy(gfx.shader);
   }

   gfx.shader = s;
   return 1;
}


static Texture _textureBuildFromFile(const char* path, TextureFlag flags = TextureFlag_Defaults) {
   u64 sz = 0;
   i32 x, y, comp;
   x = y = comp = 0;
   auto mem = fileReadBinary(path, &sz);
   auto png = stbi_load_from_memory(mem, (i32)sz, &x, &y, &comp, 4);
   auto out = render::textureBuild({ x, y }, flags, (ColorRGBA*)png);

   free(mem);
   free(png);
   return out;
}

void GraphicObjects::build() {
   textures[GameTextures_Dude] = _textureBuildFromFile("assets/dude2.png");
   textures[GameTextures_DudeNormals] = _textureBuildFromFile("assets/dudenormal2.png", TextureFlag_ClampedNearest | TextureFlag_Color_RGBA8);
   textures[GameTextures_Target] = _textureBuildFromFile("assets/target.png");
   textures[GameTextures_Light] = _textureBuildFromFile("assets/light3.png");
   textures[GameTextures_Circle] = _textureBuildFromFile("assets/circle.png", TextureFlag_ClampedLinear | TextureFlag_Color_SRGBA);
   textures[GameTextures_ShittySword] = _textureBuildFromFile("assets/sword.png");
   textures[GameTextures_SwordNormals] = _textureBuildFromFile("assets/swordnormal.png", TextureFlag_ClampedNearest | TextureFlag_Color_RGBA8);
   textures[GameTextures_GemEmpty] = _textureBuildFromFile("assets/gemempty.png");
   textures[GameTextures_GemFilled] = _textureBuildFromFile("assets/gemfilled.png");
   textures[GameTextures_HeartEmpty] = _textureBuildFromFile("assets/heartempty.png");
   textures[GameTextures_HeartFilled] = _textureBuildFromFile("assets/heartfilled.png");
   textures[GameTextures_Tile] = _textureBuildFromFile("assets/toldold.png", TextureFlag_RepeatedNearest | TextureFlag_Color_SRGBA);
   textures[GameTextures_TileNormals] = _textureBuildFromFile("assets/tilenormal.png", TextureFlag_RepeatedNearest | TextureFlag_Color_RGBA8 | TextureFlag_DisablePremultiply);

   assert(_reloadShader(*this));

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

   mesh = render::meshBuild(vbo, 6);
   meshUncentered = render::meshBuild(vboUncentered, 6);

   auto& res = Const.resolution;
   unlitScene = render::fboBuild(res, {
      render::textureBuild(res, TextureFlag_FBODefaults),   // diffuse
      render::textureBuild(res, TextureFlag_FBODefaults)    // normals
      });
   lightLayer = render::fboBuild(res);
   litScene = render::fboBuild(res);
   UI = render::fboBuild(res);
}

bool GraphicObjects::reloadShaders() {
   return _reloadShader(*this);
}