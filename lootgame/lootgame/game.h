#pragma once

#include "math.h"
#include <vector>
#include <string>

typedef struct Window Window;

struct GameData {
   struct {
      ColorRGBAf bgClearColor = { 0.45f, 0.55f, 0.60f, 1.0f };  // clear color behond all imgui windows
      bool showUI = true;                               // whether to show the ui or just a fullscreen viewer
   } imgui;
};

GameData* gameGet();

typedef struct Game Game;
Game* gameCreate(StringView assetsFolder);
GameData* gameData(Game* game);

// called after the window and context are created
void gameBegin(Game* game, Window* wnd);

typedef struct Window Window;
void gameUpdate(Game* game, Window* wnd);

void gameDestroy(Game* game);
void gameDoUI(Window* wnd);