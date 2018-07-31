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
} GameButton_;
typedef byte GameButton;



typedef Float2 WorldCoords;
typedef Float2 VPCoords;
typedef Float2 ScreenCoords;

// Coords is syntax sugar for keeping track of the 3 major coordinate systems of the game
// Uses the camera in global Game struct for world coord conversions
struct Coords {
   union {
      WorldCoords world;
      struct {
         float x, y;
      };
   };

   ScreenCoords toScreen();
   VPCoords toViewport();
   WorldCoords toWorld();

   static Coords fromScreen(ScreenCoords const& c);
   static Coords fromViewport(VPCoords const& c);
   static Coords fromWorld(WorldCoords const& c);
};

struct Constants {
   Int2 resolution = { 1920, 1080 };

};

Constants &ConstantsGet();

struct GameData {

   GameData() {}
   struct {
      ColorRGBAf bgClearColor = { 0.45f, 0.55f, 0.60f, 1.0f };  // clear color behond all imgui windows
      bool showUI = true;                               // whether to show the ui or just a fullscreen viewer
      bool mouseHovering = false;
      Rectf vpScreenArea = {0,0,1,1}; // set by imgui when the viewer moves (only in debug), necessary to track mouse position
   
      bool showMovementDebugging = false;
      bool showCollisionDebugging = false;
      f32 ambientLight = 0.0f;
   } imgui;

   struct {
      Coords mousePos = { 0,0 };

      Float2 leftStick;   // unit vector of left stick
      Float2 rightStick;  // unit vector of right stick

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

void gameReloadShaders(Game* game);

void DEBUG_gameSpawnDude(Game* game);