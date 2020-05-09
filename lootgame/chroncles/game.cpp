#include "stdafx.h"

#include "game.h"
#include "app.h"
#include "win.h"
#include "render.h"


#include "assets_gen.inl"

#include <GL/glew.h>
#include <SDL.h>
#include <stb/stb_image.h>

#define AXIS_DEADZONE 0.25f

// our aim stick normalizes so we need a harsher deadzone to prevent bounce-back
#define AXIS_AIM_DEADZONE 0.5f

GameAssets Assets;
EngineConstants& Const = Assets.constants;

EngineState Engine;
GraphicObjects Graphics;

void gameBeginFrame(GameState& g) {
   auto& mPos = ImGui::GetIO().MousePos;
   auto& io = g.io;
   io.mousePos = Coords::fromScreen({ mPos.x, mPos.y }, g);

   // update buttons
   for (GameButton b = 0; b < GameButton_COUNT; ++b) {
      io.buttonPressed[b] = io.buttonReleased[b] = false;
   }
}

bool gameProcessEvent(GameState& g, SDL_Event* event) {
   auto &io = g.io;

   switch (event->type) {
   case SDL_KEYUP:
   case SDL_KEYDOWN: {
      //bool pressed = event->type == SDL_KEYDOWN;
      //GameButton btn;

      //switch (event->key.keysym.scancode) {
      //case SDL_SCANCODE_W:
      //   btn = GameButton_UP;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.y = -1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_DOWN]) {
      //      io.leftStick.y = 0.0f;
      //   }

      //   break;
      //case SDL_SCANCODE_A:
      //   btn = GameButton_LEFT;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.x = -1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_RIGHT]) {
      //      io.leftStick.x = 0.0f;
      //   }
      //   break;
      //case SDL_SCANCODE_S:
      //   btn = GameButton_DOWN;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.y = 1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_UP]) {
      //      io.leftStick.y = 0.0f;
      //   }
      //   break;
      //case SDL_SCANCODE_D:
      //   btn = GameButton_RIGHT;

      //   if (pressed) {
      //      if (!event->key.repeat) io.leftStick.x = 1.0f;
      //   }
      //   else if (!io.buttonDown[GameButton_LEFT]) {
      //      io.leftStick.x = 0.0f;
      //   }
      //   break;
      //default: return false;
      //}

      //if (!event->key.repeat) {
      //   io.leftStick = v2Normalized(io.leftStick);
      //}

      //if (!io.buttonDown[btn] && pressed) {
      //   io.buttonPressed[btn] = true;
      //}
      //if (io.buttonDown[btn] && !pressed) {
      //   io.buttonReleased[btn] = true;
      //}
      //io.buttonDown[btn] = pressed;

      //return true;
   }  break;

   case SDL_MOUSEMOTION:
      g.lastMouseMove = appGetTime();
      break;

   case SDL_MOUSEBUTTONDOWN:
   case SDL_MOUSEBUTTONUP: {
      //if (g.mouseActive) {
      //   bool pressed = event->type == SDL_MOUSEBUTTONDOWN;
      //   if (!io.buttonDown[GameButton_RT] && pressed) {
      //      io.buttonPressed[GameButton_RT] = true;
      //   }

      //   if (io.buttonDown[GameButton_RT] && !pressed) {
      //      io.buttonReleased[GameButton_RT] = true;
      //   }

      //   io.buttonDown[GameButton_RT] = pressed;
      //   return true;
      //}
   }

   case SDL_CONTROLLERBUTTONUP:
   case SDL_CONTROLLERBUTTONDOWN:
      switch (event->cbutton.button) {
      case SDL_CONTROLLER_BUTTON_A: break;
      case SDL_CONTROLLER_BUTTON_B: break;
      case SDL_CONTROLLER_BUTTON_X: break;
      case SDL_CONTROLLER_BUTTON_Y: break;
      case SDL_CONTROLLER_BUTTON_BACK: break;
      case SDL_CONTROLLER_BUTTON_START: break;
      case SDL_CONTROLLER_BUTTON_LEFTSTICK: break;
      case SDL_CONTROLLER_BUTTON_RIGHTSTICK: break;
      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: break;
      case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: break;
      case SDL_CONTROLLER_BUTTON_DPAD_UP: break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: break;
      default: return false;
      }
      return true;

   case SDL_CONTROLLERAXISMOTION:
   {
      auto value = event->caxis.value / (float)(INT16_MAX + 1);
      bool pressed = true;
      GameButton btn;
      if (fabs(value) < AXIS_DEADZONE) {
         //value = 0.0f;
         pressed = false;
      }

      switch (event->caxis.axis) {
      case SDL_CONTROLLER_AXIS_LEFTX: io.leftStick_RAW.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_LEFTY: io.leftStick_RAW.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_RIGHTX: io.rightStick_RAW.x = value; btn = value < 0.0f ? GameButton_LEFT : GameButton_RIGHT; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: io.rightStick_RAW.y = value; btn = value < 0.0f ? GameButton_UP : GameButton_DOWN; break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT: io.leftTrigger = value; btn = GameButton_LT; break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: io.rightTrigger = value; btn = GameButton_RT; break;
      }

      if (!io.buttonDown[btn] && pressed) {
         io.buttonPressed[btn] = true;
      }

      if (io.buttonDown[btn] && !pressed) {
         io.buttonReleased[btn] = true;
      }

      io.buttonDown[btn] = pressed;

   } return true;
   }

   return false;
}


void gameUpdate(GameState& g) {
   auto time = appGetTime();
   auto dt = time - g.lastUpdate;
   auto ms = dt.toMilliseconds();
}


