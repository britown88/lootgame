#pragma once

#include "math.h"
#include "render.h"
#include <vector>
#include <string>

typedef enum {
   GameButton_LEFT = 0,
   GameButton_RIGHT,
   GameButton_UP,
   GameButton_DOWN,
   GameButton_A,
   GameButton_B,
   GameButton_X,
   GameButton_Y,
   GameButton_LB,
   GameButton_LT,
   GameButton_RB,
   GameButton_RT,

   GameButton_COUNT
} GameButton;

struct GameData {
   GameData() {}
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

      Float2 moveVector;   // unit vector of left stick
      Float2 aimVector;    // unit vector of right stick

      f32 leftTrigger = 0.0f; // 0-1 analog trigger
      f32 rightTrigger = 0.0f;// 0-1 analog trigger

      bool buttonDown[GameButton_COUNT];
      bool buttonPressed[GameButton_COUNT];
      bool buttonReleased[GameButton_COUNT];      

   } io;
};

GameData* gameDataGet();

typedef struct Game Game;

Game* gameCreate(StringView assetsFolder);

// called after the window and context are created
void gameBegin(Game*game);

typedef union SDL_Event SDL_Event;
bool gameProcessEvent(Game*game, SDL_Event* event);

void gameHandleInput(Game*game);

void gameRender(Game*game);
void gameUpdate(Game*game);

void gameDestroy(Game*game);
void gameDoUI(Game*game);

FBO const& gameGetOutputFBO(Game*game);