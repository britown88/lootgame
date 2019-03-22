#pragma once


#include "game.h"
#include "render.h"

void doRootUI();
bool gameDoUIWindow(GameInstance& instance);
void uiDoGameDebugger(GameInstance& instance);
void uiOpenTextureManager();
void uiOpenMapManager();

void beginDisabled();
void endDisabled();

void uiOpenAssetManager();
