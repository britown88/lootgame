#pragma once

#include "defs.h"
#include "math.h"

//fuck it we'll put all our app stuff here!

// the imgui example maps these differently from SDL 
// and imgui just uses numbers because omar is weird
#define MOUSE_LEFT 0
#define MOUSE_RIGHT 1
#define MOUSE_MIDDLE 2

struct _AppConfig {
   std::filesystem::path assetPath;

   bool reflectgen = false;
   const char* reflectTarget = nullptr;
};

extern _AppConfig AppConfig;

void runReflectGen();

// APP
typedef struct App App;
App* appCreate();

typedef struct {
   uint32_t w, h;
   StringView title;
}WindowConfig;

void appCreateWindow(App* app, WindowConfig const& info);
void appDestroy(App* app);

bool appRunning(App* app);
void appStep(App* app);

// these function work off a global App* (the last created one, create/switch potential later

// gui(wnd) is called every frame during the imgui update until it returns false
// do your one-off imgui::begin()s here, use lambda capture for state
// label must be unique or this call is ignored
void appAddGUI(StringView label, std::function<bool()> gui);
void appClose();
Time appGetTime();

void appBeginNewGameInstance();