#pragma once

#include "math.h"
#include "render.h"
#include "coords.h"



typedef struct GameState GameState;

struct GraphicObjects {
   ShaderHandle shader = 0;

   Mesh mesh, meshUncentered;
   FBO
      unlitScene,
      lightLayer,
      litScene,
      UI;

   void build();
   bool reloadShaders();
};

void assetsSave();
void assetsLoad();
void assetsReloadAll();

// called in Graphics.Build()
void assetsBuildTextureHandles();

//@reflect{
struct EngineConstants {
   //@readonly
   Int2 resolution = { 1920, 1080 };
};//}

//@reflect{
struct EngineState {   
   ColorRGBAf bgClearColor = { 0.45f, 0.55f, 0.60f, 1.0f };  // clear color behind all imgui windows
   bool fullScreen = false;
   bool reloadShaders = false;
};//}

extern EngineState Engine;
extern GraphicObjects Graphics;

enum GameButton_ {
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
} ;
typedef byte GameButton;


struct IO {
   Coords mousePos = { { 0,0 } };

   Float2 leftStick_RAW;
   Float2 rightStick_RAW;

   Float2 leftStick;   // unit vector of left stick
   Float2 rightStick;  // unit vector of right stick

   float leftTrigger = 0.0f; // 0-1 analog trigger
   float rightTrigger = 0.0f;// 0-1 analog trigger

   bool buttonDown[GameButton_COUNT];
   bool buttonPressed[GameButton_COUNT];
   bool buttonReleased[GameButton_COUNT];
};

struct GameCamera {
   Rectf viewport;
   
};


//@reflect{
struct GameAssets {
   EngineConstants constants;

};//}

extern GameAssets Assets;
extern EngineConstants& Const;
#include "assets_gen.h"
  

struct GameStateUI {
   bool fullscreen = false;
   bool focused = false;
   bool editing = false;

   bool showGameUI = true;
   Float2 gridSize = { 100.0f, 100.0f };
   bool showEditGrid = true;

   Coords rightClickMousePos;
};

struct GameState {

   GameStateUI ui;
   IO io;
   GameCamera camera;// = { { 0, 0, (float)Const.vpSize.x, (float)Const.vpSize.y } }; // viewport into the world
   Rectf vpScreenArea = { 0, 0, 1, 1 }; // screen coordinates or the viewer image within the UI
   
   Time lastMouseMove;

   Time lastUpdate; // appGetTime of last gameUpdate
   bool paused = false;

   Milliseconds gameClock = 0; // ticks since game started
   Milliseconds frameClock = 0; // ticks since last frame step
   Milliseconds otherFrameClock = 0; // ticks since last 'every other frame' step

};

struct GameInstance {
   std::string winTitle;
   GameState state;
   FBO outputFbo;
};

extern GameState* DEBUG_CurrentGame;

void gameBeginFrame(GameState& g);

typedef union SDL_Event SDL_Event;
bool gameProcessEvent(GameState& g, SDL_Event* event);

void gameUpdate(GameState& g);
void gameDraw(GameState& g, FBO& output);

