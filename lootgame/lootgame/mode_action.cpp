#include "stdafx.h"

#include "game.h"
#include "app.h"

#define DUDE_COUNT 1

static Dude _createDude(GameState& game) {
   Dude out;
   out.phy.pos = { 270,150 };
   dudeApplyTemplate(out, DudeTemplates.Dude);
   return out;
}

static Dude _createEnemy(Float2 pos) {
   Dude out;
   out.phy.pos = pos;
   dudeApplyTemplate(out, DudeTemplates.Dude);
   return out;
}


void gameStartActionMode(GameState &g) {
   g = {};

   auto m = Assets.maps.find(intern("test"));
   if (m != Assets.maps.end()) {
      g.map = &(Map&)m->second;
   }
   g.mode.type = ModeType_ACTION;
   g.lastUpdate = appGetTime();

   g.maindude = _createDude(g);

   //auto e = _createEnemy({ 1000, 1000 });
   //e.ai.target = &g.maindude;
   //g.baddudes.push_back(e);

   for (int i = 0; i < g.mode.action.waveSize; ++i) {
      auto e = _createEnemy({ (float)(rand() % (int)g.map->size.x) + 100, (float)(rand() % (int)g.map->size.y) + 100 });
      e.ai.target = &g.maindude;
      g.baddudes.push_back(e);
   }

   //appAddGUI("dudeeditor", [=]() {
   //   uiEditDude(game->maindude);
   //   return true;
   //});
}

void gameUpdateActionMode(GameState &g) {
   bool allDead = true;
   for (auto&& d : g.baddudes) {
      if (dudeAlive(d)) {
         allDead = false;
         break;
      }
   }

   if (allDead) {
      ++g.mode.action.waveSize;
      for (int i = 0; i < g.mode.action.waveSize; ++i) {
         auto e = _createEnemy({ (float)(rand() % (int)g.map->size.x) + 100, (float)(rand() % (int)g.map->size.y) + 100 });
         e.ai.target = &g.maindude;
         g.baddudes.push_back(e);
      }
   }

   if (!dudeAlive(g.maindude)) {
      g.mode.type = ModeType_YOUDIED;
      g.mode.clock = 0;
      g.maindude.mv.moveVector = { 0,0 };
   }
}

void gameUpdateYouDiedMode(GameState &g) {
   if (g.mode.clock > 3000) {
      gameStartActionMode(g);
   }
}


void DEBUG_gameSpawnDude(GameState& game, Coords location) {
   auto e = _createEnemy(location.toWorld());
   e.ai.target = &game.maindude;
   game.baddudes.push_back(e);
}