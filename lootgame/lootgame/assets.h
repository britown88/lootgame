#pragma once

#include "defs.h"
#include "render.h"

enum GameTextures_ {
   GameTextures_Dude = 0,
   GameTextures_DudeNormals,
   GameTextures_Target,
   GameTextures_Light,
   GameTextures_Circle,
   GameTextures_ShittySword,
   GameTextures_SwordNormals,
   GameTextures_Tile,
   GameTextures_TileNormals,
   GameTextures_GemEmpty,
   GameTextures_GemFilled,
   GameTextures_HeartEmpty,
   GameTextures_HeartFilled,

   GameTexture_COUNT
};
typedef u32 GameTexture;

struct GraphicObjects {
   ShaderHandle shader = 0;

   Texture textures[GameTexture_COUNT];

   Mesh mesh, meshUncentered;
   FBO
      unlitScene,
      lightLayer,
      litScene,
      UI;

   void build();
   bool reloadShaders();
};

