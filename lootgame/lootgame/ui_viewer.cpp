#include "stdafx.h"

#include "defs.h"
#include "ui.h"
#include "app.h"

#include "reflection_gen.h"
#include <SDL2/SDL_keycode.h>

static ImGuiWindowFlags BorderlessFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoDocking;


static void _renderViewerFBO(GameState& g, FBO& output) {
   auto sz = ImGui::GetContentRegionAvail();

   auto rect = getProportionallyFitRect(output.sz, { (int32_t)sz.x, (int32_t)sz.y });

   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   const ImVec2 p = ImGui::GetCursorScreenPos();
   auto a = ImVec2(p.x + rect.x, p.y + rect.y);
   auto b = ImVec2(p.x + rect.x + rect.w, p.y + rect.y + rect.h);

   g.vpScreenArea = { a.x, a.y, b.x - a.x, b.y - a.y };

   draw_list->AddRectFilled(a, b, IM_COL32_BLACK);
   draw_list->AddCallback([](auto, auto) { render::enableSRGB();  }, nullptr);
   draw_list->AddImage((ImTextureID)(intptr_t)output.out[0].handle, a, b);
   draw_list->AddCallback([](auto, auto) { render::disableSRGB(); }, nullptr);
}

static void _showFullScreenViewer(GameInstance& g) {
   auto sz = ImGui::GetIO().DisplaySize;

   auto &style = ImGui::GetStyle();

   auto vp = ImGui::GetMainViewport();

   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
   ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

   ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
   ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);

   if (ImGui::Begin("fullscreenViewer", nullptr, BorderlessFlags)) {
      _renderViewerFBO(g.state, g.outputFbo);
      //g.mouseActive = true;
   }
   ImGui::End();

   ImGui::PopStyleVar(3);

   // handle hotkeys todo: this will be handled by gamestate later
   auto& io = ImGui::GetIO();
   auto& keys = io.KeyMap;
   if (ImGui::IsKeyPressed(keys[ImGuiKey_Escape])) {
      appClose();
   }
   if (io.KeyCtrl && ImGui::IsKeyPressed(keys[ImGuiKey_Enter])) {
      g.state.ui.fullscreen = false;
   }
}

static void _toggleEditing(GameState&g) {
   g.ui.editing = !g.ui.editing;
   LOG(g.ui.editing ? "Entered Edit Mode" : "Existing Edit Mode");
}

static void _viewerMenuBar(GameState& g) {
   if (ImGui::BeginMenuBar()) {
      bool editing = g.ui.editing;
      if (editing) {
         ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
      }
      if (ImGui::Button(ICON_FA_PENCIL_ALT)) { 
         _toggleEditing(g);
      }
      if (editing) {
         ImGui::PopStyleColor();
      }


      ImGui::EndMenuBar();
   }
}

static void _viewerHandleInput(GameState& g) {
   auto& io = ImGui::GetIO();
   auto& keys = io.KeyMap;

   if (ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Escape])) {
      appClose();
   }
   if (io.KeyCtrl && ImGui::IsKeyPressed(keys[ImGuiKey_Enter])) {
      g.ui.fullscreen = true;
   }
   if (ImGui::IsKeyPressed(SDL_SCANCODE_E)) {
      _toggleEditing(g);
   }

   if (g.ui.editing) {
      if (ImGui::IsKeyPressed(keys[ImGuiKey_Space])) {
         g.ui.cameraDragStart = g.io.mousePos;
         g.ui.cameraDragVpStart = g.camera.viewport.xy();
      }
      g.ui.draggingCamera = ImGui::IsKeyDown(keys[ImGuiKey_Space]);

      if (g.ui.draggingCamera) {
         auto start = g.ui.cameraDragStart.toWorld();
         auto end = g.io.mousePos.toWorld();
         Float2 delta = { start.x - end.x, start.y - end.y };

         auto&vp = g.camera.viewport;
         vp.setPos(vp.xy() + delta);
      }
   }
}

static bool _showWindowedViewer(GameInstance& gi) {
   bool p_open = true;
   auto& g = gi.state;

   if (ImGui::Begin(gi.winTitle.c_str(), &p_open, ImGuiWindowFlags_MenuBar)) {
      g.ui.focused = ImGui::IsWindowFocused();

      _viewerMenuBar(g);
      _renderViewerFBO(g, gi.outputFbo);

      if (g.ui.focused) {
         _viewerHandleInput(g);
      }
   }
   ImGui::End();

   if (!p_open) {
      //cleanup?
   }

   return p_open;
}


void uiDoGameDebugger(GameInstance& instance) {
   if (ImGui::Begin("Game Debugger", nullptr)) {
      auto&g = instance.state;

      if (ImGui::Button("Restart", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         instance.state = {};
         gameStartActionMode(instance.state);
      }

      if (ImGui::Button("Reload Shaders", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         //gameReloadShaders(g);
      }
      if (ImGui::Button("Spawn Dude", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         //DEBUG_gameSpawnDude(g);
      }

      auto&io = g.io;

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Input")) {
         ImGui::Text("Left Stick: (%f, %f)", io.leftStick.x, io.leftStick.y);
         ImGui::Text("Right Stick: (%f, %f)", io.rightStick.x, io.rightStick.y);
         ImGui::Text("Left Trigger: %f", io.leftTrigger);
         ImGui::Text("Right Trigger: %f", io.rightTrigger);

         ImGui::Text("LeftTrigger: %s", io.buttonDown[GameButton_LT] ? "Down" : "Up");
         ImGui::Text("Pressed: %s", io.buttonPressed[GameButton_LT] ? "true" : "false");
         ImGui::Text("Released: %s", io.buttonReleased[GameButton_LT] ? "true" : "false");
      }

      doTypeUI(&g.DEBUG);

   }
   ImGui::End();
}



bool gameDoUIWindow(GameInstance& inst) {
   if (inst.state.ui.fullscreen) {
      _showFullScreenViewer(inst);
      return true;
   }
   return _showWindowedViewer(inst);   
}
