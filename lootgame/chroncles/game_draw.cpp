#include "stdafx.h"

#include "game.h"


void gameDraw(GameState& g, FBO& output) {
   auto& c = Const;

   render::shaderSetActive(Graphics.shader);
   uber::resetToDefault(true);


   render::fboBind({});
}
