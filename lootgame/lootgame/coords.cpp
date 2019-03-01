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

