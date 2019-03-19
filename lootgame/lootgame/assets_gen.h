// This code was generated by robots, do not modify

#pragma once

#include "game.h"

enum Textures_{
   Textures_Circle,
   Textures_Dude,
   Textures_DudeNormals,
   Textures_GemCracked,
   Textures_GemEmpty,
   Textures_GemFilled,
   Textures_HeartEmpty,
   Textures_HeartFilled,
   Textures_Light,
   Textures_ShittySword,
   Textures_SwordNormals,
   Textures_Target,
   Textures_Tile,
   Textures_TileNormals,
   Textures_TinyHeartEmpty,
   Textures_TinyHeartFull,
   Textures_TinyStamCracked,
   Textures_TinyStamEmpty,
   Textures_TinyStamFull,
   
   Textures_COUNT
};

struct AssetTextures{
   Texture* array[Textures_COUNT] = { 0 };

   Texture* Circle = nullptr;
   Texture* Dude = nullptr;
   Texture* DudeNormals = nullptr;
   Texture* GemCracked = nullptr;
   Texture* GemEmpty = nullptr;
   Texture* GemFilled = nullptr;
   Texture* HeartEmpty = nullptr;
   Texture* HeartFilled = nullptr;
   Texture* Light = nullptr;
   Texture* ShittySword = nullptr;
   Texture* SwordNormals = nullptr;
   Texture* Target = nullptr;
   Texture* Tile = nullptr;
   Texture* TileNormals = nullptr;
   Texture* TinyHeartEmpty = nullptr;
   Texture* TinyHeartFull = nullptr;
   Texture* TinyStamCracked = nullptr;
   Texture* TinyStamEmpty = nullptr;
   Texture* TinyStamFull = nullptr;
   
};
extern AssetTextures Textures;

enum Maps_{
   Maps_secondmap,
   Maps_test,
   
   Maps_COUNT
};

struct AssetMaps{
   Map* array[Maps_COUNT] = { 0 };

   Map* secondmap = nullptr;
   Map* test = nullptr;
   
};
extern AssetMaps Maps;

void assetsStartup();
