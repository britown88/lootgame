#include "game.h"
#include "app.h"

#include "imgui.h"
#include "scf.h"
#include <unordered_map>


struct Game {
   GameData data;
};

static GameData* g_gameData = nullptr;
GameData* gameGet() {
   return g_gameData;
}

static void _gameDataInit(GameData* game, StringView assetsFolder) {

}


Game* gameCreate(StringView assetsFolder) {
   auto out = new Game();
   _gameDataInit(&out->data, assetsFolder);
   g_gameData = &out->data;
   return out;
}

GameData* gameData(Game* game) { return &game->data; }

void gameUpdate(Game* game, Window* wnd) {
   gameDoUI(wnd);
}

void gameDestroy(Game* game) {
   delete game;
}