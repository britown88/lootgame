#pragma once

#include "defs.h"

typedef struct GameState GameState;

typedef Float2 WorldCoords;
typedef Float2 VPCoords;
typedef Float2 DlgCoords;
typedef Float2 ScreenCoords;

// Coords is syntax sugar for keeping track of the 3 major coordinate systems of the game
// Uses the camera in global Game struct for world coord conversions
// @reflect{
struct Coords {
   WorldCoords world;

   ScreenCoords toScreen(GameState& g);
   VPCoords toViewport(GameState& g);
   DlgCoords toDialog(GameState& g);
   WorldCoords toWorld();

   static Coords fromScreen(ScreenCoords const& c, GameState& g);
   static Coords fromViewport(VPCoords const& c, GameState& g);
   static Coords fromDialog(DlgCoords const& c, GameState& g);
   static Coords fromWorld(WorldCoords const& c);

   // handy for simple distance conversions to the two main coord spaces
   static float screenToWorld(float f, GameState& g);
   static float worldToScreen(float f, GameState& g);
}; //}