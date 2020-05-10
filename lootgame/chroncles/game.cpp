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

void gameBeginNewGame(GameState& g) {
   // starting mode
   gameBeginExploreMode(g);
}

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

static void _applyInputs(GameState&g) {
   //g.io.leftStick = _scaleStick(g.io.leftStick_RAW);
   //g.io.rightStick = _scaleStick(g.io.rightStick_RAW);

   //if (dudeAlive(g.maindude)) {
   //   dudeApplyInput(g, g.maindude);
   //}
}

static void _otherFrameStep(GameState& g) {
   Milliseconds tickSize = 32;
   //LOG("32 milis step");
}

static void _updateMode(GameState&g, Milliseconds tickSize) {
   //g.mode.clock += tickSize;
   switch (g.mode) {
   case GameMode_Explore: gameUpdateExploreMode(g); break;
   }
}

static void _frameStep(GameState& g) {
   Milliseconds tickSize = 16;
   //LOG("16 milis step");

   //dudeUpdateState(g.maindude, tickSize);
   //mainDudeCheckAttackCollisions(g.maindude, g.baddudes);

   //dudeUpdateRotation(g.maindude);
   //dudeUpdateVelocity(g.maindude);

   //

   //_buildPhySystem(g);
}

static void _milliStep(GameState& g) {
   Milliseconds tickSize = 1;
   //LOG("1 mili step");

   //updatePhyPositions(g.phySys.objs);
}

static void _perRenderStep(GameState& g, Milliseconds ms) {
   Milliseconds tickSize = ms;
   //_cameraFollowPlayer(g);
   //LOG("PerFrame step");
}


static const int LastUpdateMSCap = 32;
static const int FrameLength = 16;

void gameUpdate(GameState& g) {
   auto time = appGetTime();
   auto dt = time - g.lastUpdate;
   auto ms = dt.toMilliseconds();

   _applyInputs(g);

   if (ms > LastUpdateMSCap) {
      // something bad happened and we spiked hard
      ms = LastUpdateMSCap;
      g.lastUpdate = time - timeMillis(ms);
   }

   g.lastUpdate += timeMillis(ms);

   g.otherFrameClock += ms;
   g.frameClock += ms;
   g.gameClock += ms;

   while (g.otherFrameClock > FrameLength * 2) {
      _otherFrameStep(g);
      g.otherFrameClock -= FrameLength * 2;

      for (int i = 0; i < 2; ++i) {
         if (g.frameClock > FrameLength) {
            _frameStep(g);
            g.frameClock -= FrameLength;
         }

         for (int j = 0; j < FrameLength; ++j) {
            if (g.gameClock > 0) {
               _milliStep(g);
               --g.gameClock;
            }
         }
      }
   }

   while (g.frameClock > FrameLength) {
      _frameStep(g);
      g.frameClock -= FrameLength;

      for (int j = 0; j < FrameLength; ++j) {
         if (g.gameClock > 0) {
            _milliStep(g);
            --g.gameClock;
         }
      }
   }

   while (g.gameClock > 0) {
      _milliStep(g);
      --g.gameClock;
   }

   _updateMode(g, ms);
   _perRenderStep(g, ms);
}


