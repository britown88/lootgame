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
   
   out.texture = &TextureMap.map[intern("Dude")];

   out.c = White;
   out.moveset = _createMoveSet();
   out.phy.pos = { 10,10 };
   out.phy.circle.size = 10.0f;
   out.phy.maxSpeed = Const.dudeMoveSpeed;
   out.phy.invMass = 0.0f;
   out.renderSize = { (float)out.texture->sz.x, (float)out.texture->sz.y };


   out.status.stamina = out.status.staminaMax = 4;
   out.status.health = out.status.healthMax = 1;

   return out;
}

static Dude _createEnemy(Float2 pos) {
   Dude out;

   out.texture = &TextureMap.map[intern("Dude")];

   out.moveset = _createMoveSet();
   out.c = White;// {1.0f, 0.3f, 0.3f, 1.0f};
   out.phy.pos = pos;
   out.phy.circle.size = 10.0f;
   out.phy.velocity = { 0,0 };
   out.phy.maxSpeed = Const.dudeMoveSpeed;
   out.phy.invMass = (float)(rand() % 50 + 5) / 100.0f;
   out.renderSize = { (float)out.texture->sz.x, (float)out.texture->sz.y };

   out.status.stamina = out.status.staminaMax = 3;
   out.status.health = out.status.healthMax = 1;

   return out;
}


void gameStartActionMode(GameState &g) {
   g = {};

   g.mode.type = ModeType_ACTION;
   g.lastUpdate = appGetTime();

   g.maindude = _createDude(g);

   auto e = _createEnemy({ 1000, 1000 });
   e.ai.target = &g.maindude;
   g.baddudes.push_back(e);

   for (int i = 0; i < DUDE_COUNT; ++i) {
      auto e = _createEnemy({ (float)(rand() % 1820) + 100, (float)(rand() % 980) + 100 });
      e.ai.target = &g.maindude;
      g.baddudes.push_back(e);
   }

   //appAddGUI("dudeeditor", [=]() {
   //   uiEditDude(game->maindude);
   //   return true;
   //});
}


void DEBUG_gameSpawnDude(GameState& game) {
   auto e = _createEnemy({ (float)(rand() % 1820) + 100, (float)(rand() % 980) + 100 });
   e.ai.target = &game.maindude;
   game.baddudes.push_back(e);
}