#include "stdafx.h"

#include "game.h"
#include "app.h"

#define DUDE_COUNT 1


static MoveSet _createMoveSet() {
   MoveSet out;
   out.swings.resize(3);
   Rectf hitbox = { -2.0f, -4.5f, 28, 9 };

   out.swings[0].swipeAngle = 120.0f;
   out.swings[0].lungeSpeed = 0.3f;
   out.swings[0].lungeDist = 0.0f;
   out.swings[0].windupDur = 150;
   out.swings[0].swingDur = 250;
   out.swings[0].cooldownDur = 300;
   out.swings[0].hitbox = hitbox;

   out.swings[1] = out.swings[0];
   out.swings[1].windupDur = 50;
   out.swings[1].swingDur = 150;
   out.swings[1].cooldownDur = 500;

   out.swings[2] = out.swings[0];
   out.swings[2].windupDur = 50;
   out.swings[2].swingDur = 100;
   out.swings[2].cooldownDur = 200;

   //out.swings[1].swipeAngle = 180.0f;
   //out.swings[1].lungeSpeed = 0.25f;
   //out.swings[1].windupDur = 100;
   //out.swings[1].swingDur = 250;
   //out.swings[1].cooldownDur = 250;
   //out.swings[1].hitbox = hitbox;

   //out.swings[2].swipeAngle = 0.0f;
   //out.swings[2].lungeSpeed = 0.5f;
   //out.swings[2].windupDur = 100;
   //out.swings[2].swingDur = 250;
   //out.swings[2].cooldownDur = 100;
   //out.swings[2].hitbox = hitbox;

   return out;
}

static Dude _createDude(GameState& game) {
   Dude out;
   
   out.texture = Textures.Dude;

   out.c = White;
   out.moveset = _createMoveSet();
   out.phy.type = PhyObject::PhyType_Circle;
   out.phy.pos = { 270,150 };
   out.phy.circle.size = 7.0f;
   out.phy.maxSpeed = Const.dudeMoveSpeed;
   out.phy.invMass =  0.01f;
   out.renderSize = { (float)out.texture->sz.x, (float)out.texture->sz.y };

   out.status.stamina.resize(4);
   out.status.health = out.status.healthMax = 2;

   return out;
}

static Dude _createEnemy(Float2 pos) {
   Dude out;

   out.texture = Textures.Dude;

   out.moveset = _createMoveSet();
   out.c = White;// {1.0f, 0.3f, 0.3f, 1.0f};
   out.phy.type = PhyObject::PhyType_Circle;
   out.phy.pos = pos;
   out.phy.circle.size = 7.0f;
   out.phy.velocity = { 0,0 };
   out.phy.maxSpeed = Const.dudeMoveSpeed;
   out.phy.invMass = 1.0f;// (float)(rand() % 50 + 5) / 100.0f;
   out.renderSize = { (float)out.texture->sz.x, (float)out.texture->sz.y };

   out.status.stamina.resize(3);
   out.status.health = out.status.healthMax = 1;

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