#pragma once

#include "defs.h"

typedef struct GameState GameState;

typedef Float2 WorldCoords;
typedef Float2 VPCoords;
typedef Float2 ScreenCoords;

// Coords is syntax sugar for keeping track of the 3 major coordinate systems of the game
// Uses the camera in global Game struct for world coord conversions
struct Coords {
   union {
      WorldCoords world;
      struct {
         float x, y;
      };
   };

   ScreenCoords toScreen(GameState& g);
   VPCoords toViewport(GameState& g);
   WorldCoords toWorld();

   static Coords fromScreen(ScreenCoords const& c, GameState& g);
   static Coords fromViewport(VPCoords const& c, GameState& g);
   static Coords fromWorld(WorldCoords const& c);
};