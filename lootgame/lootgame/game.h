#pragma once

#include "math.h"
#include "render.h"
#include <vector>
#include <string>

typedef struct Window Window;

struct GameData {
   struct {
      ColorRGBAf bgClearColor = { 0.45f, 0.55f, 0.60f, 1.0f };  // clear color behond all imgui windows
      bool showUI = true;                               // whether to show the ui or just a fullscreen viewer
      Rectf vpScreenArea = {0,0,1,1}; // set by imgui when the viewer moves (only in debug), necessary to track mouse position
   } imgui;

   const struct {
      const Int2 resolution = { 1920, 1080 };
   } constants;

   struct {
      Float2 mousePos;

   } io;
};

GameData* gameDataGet();

void gameCreate(StringView assetsFolder);

// called after the window and context are created
void gameBegin();

typedef union SDL_Event SDL_Event;
bool gameProcessEvent(SDL_Event* event);

typedef struct Window Window;
void gameUpdate(Window* wnd);

void gameDestroy();
void gameDoUI(Window* wnd);

FBO const& gameGetOutputFBO();