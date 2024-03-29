#include "stdafx.h"

#include "game.h"
#include "coords.h"

ScreenCoords Coords::toScreen(GameState& g) {
   auto& vpScreen = g.vpScreenArea;
   auto& vpCamera = g.camera.viewport;

   return {
      vpScreen.x + ((world.x - vpCamera.x) / vpCamera.w) * vpScreen.w,
      vpScreen.y + ((world.y - vpCamera.y) / vpCamera.h) * vpScreen.h
   };
}
DlgCoords Coords::toDialog(GameState& g) {
   auto& vpScreen = g.vpScreenArea;
   auto& vpCamera = g.camera.viewport;

   return {
      ((world.x - vpCamera.x) / vpCamera.w) * vpScreen.w,
      ((world.y - vpCamera.y) / vpCamera.h) * vpScreen.h
   };
}
VPCoords Coords::toViewport(GameState& g) {
   auto& vpCamera = g.camera.viewport;
   return {
      world.x - vpCamera.x,
      world.y - vpCamera.y
   };
}
WorldCoords Coords::toWorld() {
   return world;
}

Coords Coords::fromScreen(ScreenCoords const& c, GameState& g) {
   auto& vpScreen = g.vpScreenArea;
   auto& vpCamera = g.camera.viewport;

   return {
      vpCamera.x + (c.x - vpScreen.x) / vpScreen.w * vpCamera.w,
      vpCamera.y + (c.y - vpScreen.y) / vpScreen.h * vpCamera.h
   };
}
Coords Coords::fromDialog(DlgCoords const& c, GameState& g) {
   auto& vpScreen = g.vpScreenArea;
   auto& vpCamera = g.camera.viewport;

   return {
      vpCamera.x + (c.x) / vpScreen.w * vpCamera.w,
      vpCamera.y + (c.y) / vpScreen.h * vpCamera.h
   };
}
Coords Coords::fromViewport(VPCoords const& c, GameState& g) {
   auto& vpCamera = g.camera.viewport;
   return {
      vpCamera.x + c.x,
      vpCamera.y + c.y
   };
}
Coords Coords::fromWorld(WorldCoords const& c) {
   return { c };
}

// these funcitons ar every wasteful but i dont care
float Coords::screenToWorld(float f, GameState& g) {
   return (Coords::fromScreen({ f, f }, g).toWorld() - Coords::fromScreen({ 0, 0 }, g).toWorld()).x;
}
float Coords::worldToScreen(float f, GameState& g) {
   return (Coords::fromWorld({ f, f }).toScreen(g) - Coords::fromWorld({ 0, 0 }).toScreen(g)).x;
}

