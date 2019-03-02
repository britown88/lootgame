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


static void _renderViewerFBO(GameState& g, FBO& output, ImVec2 sz) {

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
      _renderViewerFBO(g.state, g.outputFbo, ImGui::GetContentRegionAvail());
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

static void _setEditMode(GameState& g, GameEditMode mode) {
   switch (g.ui.mode) {
   case GameEditMode_Walls:
      g.ui.editingWall = nullptr;
      break;
   }

   if (g.ui.mode == mode) {
      g.ui.mode = GameEditMode_None;
   }

   g.ui.mode = mode;
}

static void _viewerMenuBar(GameState& g) {
   if (ImGui::BeginMenuBar()) {
      bool editing = g.ui.editing;
      if (editing) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive)); }
      if (ImGui::Button(ICON_FA_PENCIL_ALT)) { _toggleEditing(g); }
      if (editing) { ImGui::PopStyleColor(); }

      ImGui::Separator();
      ImGui::Checkbox("Game UI", &g.ui.showGameUI);

      ImGui::Separator();

      if (!editing) {
         beginDisabled();
      }

      ImGui::Checkbox("Edit Grid", &g.ui.showEditGrid);
      float gridSize = g.ui.gridSize.x;
      ImGui::PushItemWidth(100.0f);
      ImGui::SliderFloat("##grid", &gridSize, 10.0f, 100.0f, "%0.0f");
      ImGui::PopItemWidth();
      g.ui.gridSize = { gridSize, gridSize };

      ImGui::Separator();

      auto m = g.ui.mode;


      if (m == GameEditMode_Walls) {ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));}
      if (ImGui::Button("Walls")) { _setEditMode(g, GameEditMode_Walls); }
      if (m == GameEditMode_Walls) {ImGui::PopStyleColor();}

      if (!editing) {
         endDisabled();
      }

      ImGui::EndMenuBar();
   }
}

static void _statusBar(GameState&g) {

   if (g.ui.editing) {
      ImGui::TextUnformatted("Editing");
      ImGui::SameLine();
   }

   auto &vp = g.camera.viewport;
   ImGui::Text("VP: x:%0.1f y:%0.1f w:%0.1f h:%0.1f", vp.x, vp.y, vp.w, vp.h);

   ImGui::SameLine();

   auto mpos = g.io.mousePos.toWorld();
   ImGui::Text("Mouse (%0.1f, %0.1f)", mpos.x, mpos.y);
}

static void _handleWallInputs(GameState& g) {
   auto& current = g.ui.editingWall;

   if (ImGui::IsItemClicked()) {
      if (!current) {
         Wall newall;
         newall.points.push_back(g.io.mousePos.toWorld());
         g.map.walls.push_back(newall);
         current = &g.map.walls.back();
      }
      else {
         current->points.push_back(g.io.mousePos.toWorld());
      }
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
      g.ui.editing = false;
   }
   if (ImGui::IsKeyPressed(SDL_SCANCODE_E)) {
      _toggleEditing(g);
   }

   if (g.ui.editing) {

      //------------- camera drag with space
      if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(keys[ImGuiKey_Space])) {
         g.ui.cameraDragStart = g.io.mousePos.toWorld();
         g.ui.cameraDragVpStart = g.camera.viewport.xy();
         g.ui.draggingCamera = true;
      }
      if (g.ui.draggingCamera) {
         auto start = g.ui.cameraDragStart;
         auto end = g.io.mousePos.toWorld();
         Float2 delta = { start.x - end.x, start.y - end.y };

         auto&vp = g.camera.viewport;
         vp.setPos(vp.xy() + delta);

         if (ImGui::IsKeyReleased(keys[ImGuiKey_Space])) {
            g.ui.draggingCamera = false ;
         }
      }

      // ----------------- camera zoom with mouse
      if (!g.ui.draggingCamera) {
         auto wheel = ImGui::GetIO().MouseWheel;
         if (wheel != 0.0f) {

            auto mPos = g.io.mousePos.toWorld();
            auto scale = 1.0f + -wheel * 0.1f;

            auto& vp = g.camera.viewport;
            if ( scale > 1.0f || vp.w > 50.0f) {
               vp.scaleFromPoint(mPos, scale);
            }

            
         }
      }

      switch (g.ui.mode) {
      case GameEditMode_Walls:
         _handleWallInputs(g);
         break;
      }
   }
}

static void _doRightClickMenu(GameState&g) {

   switch (g.ui.mode) {
   case GameEditMode_Walls:
      if (g.ui.editingWall) {
         if (ImGui::MenuItem("Finish Wall")) {
            g.ui.editingWall = nullptr;
         }
      }
      break;
   }
}


static void _renderGrid(GameState& g) {
   Float2 &gridSize = g.ui.gridSize;

   auto a = g.vpScreenArea.Min();
   auto b = g.vpScreenArea.Max();
   ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 255, 255, 128), 0.0f, 15, 2.0f);

   auto& vp = g.camera.viewport;
   auto vpMin = vp.Min();
   auto vpMax = vp.Max();

   vpMin.x = floorf(vpMin.x / gridSize.x) * gridSize.x;
   vpMin.y = floorf(vpMin.y / gridSize.y) * gridSize.y;

   vpMin.x = clamp(vpMin.x, 0, vp.Max().x);
   vpMin.y = clamp(vpMin.y, 0, vp.Max().y);
   vpMax.x = clamp(vpMax.x, 0, g.map.size.x);
   vpMax.y = clamp(vpMax.y, 0, g.map.size.y);

   auto color = IM_COL32(255, 255, 255, 64);

   while (vpMin.x < vpMax.x) {
      Coords pa = { { vpMin.x, vp.y } };
      Coords pb = { { vpMin.x, vp.y + vp.h } };
      auto a = pa.toScreen(g);
      auto b = pb.toScreen(g);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color);
      vpMin.x += gridSize.x;
   }
   while (vpMin.y < vpMax.y) {
      Coords pa = { { vp.x, vpMin.y } };
      Coords pb = { { vp.x + vp.w, vpMin.y } };
      auto a = pa.toScreen(g);
      auto b = pb.toScreen(g);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color);
      vpMin.y += gridSize.y;
   }

}


static void _renderWalls(GameState& g) {
   ImU32 lineCol = IM_COL32(255, 0, 0, 255);
   ImU32 finisherCol = IM_COL32(255, 128, 128, 255);
   ImU32 previewCol = IM_COL32(255, 128, 128, 128);

   for (auto&& wall : g.map.walls) {
      for (auto iter = wall.points.begin(); iter != wall.points.end(); ++iter) {
         if (iter + 1 != wall.points.end()) {
            auto a = Coords::fromWorld(*iter).toScreen(g);
            auto b = Coords::fromWorld(*(iter + 1)).toScreen(g);

            ImGui::GetWindowDrawList()->AddLine(a, b, lineCol);
         }
      }

      if (&wall != g.ui.editingWall && wall.points.size() > 2) {
         auto a = Coords::fromWorld(*(wall.points.begin())).toScreen(g);
         auto b = Coords::fromWorld(*(wall.points.end() - 1)).toScreen(g);
         ImGui::GetWindowDrawList()->AddLine(a, b, finisherCol);
      }
   }

   // render the editing wall
   if (g.ui.editingWall && !g.ui.editingWall->points.empty()) {
      auto back = g.ui.editingWall->points.back();
      auto a = Coords::fromWorld(back).toScreen(g);
      auto b = g.io.mousePos.toScreen(g);

      ImGui::GetWindowDrawList()->AddLine(a, b, lineCol);

      if (g.ui.editingWall->points.size() > 2) {
         auto a = Coords::fromWorld(*(g.ui.editingWall->points.end() - 1)).toScreen(g);
         auto b = Coords::fromWorld(*(g.ui.editingWall->points.begin())).toScreen(g);
         ImGui::GetWindowDrawList()->AddLine(a, b, previewCol);
      }

      if (g.ui.editingWall->points.size() > 1) {
         auto a = g.io.mousePos.toScreen(g);
         auto b = Coords::fromWorld(*(g.ui.editingWall->points.begin())).toScreen(g);
         ImGui::GetWindowDrawList()->AddLine(a, b, finisherCol);
      }
   }
}

static void _renderHelpers(GameState& g) {
   if (g.ui.editing && g.ui.showEditGrid) {
      _renderGrid(g);
   }

   _renderWalls(g);

}


static bool _showWindowedViewer(GameInstance& gi) {
   bool p_open = true;
   auto& g = gi.state;

   if (ImGui::Begin(gi.winTitle.c_str(), &p_open, ImGuiWindowFlags_MenuBar)) {
      g.ui.focused = ImGui::IsWindowFocused();

      _viewerMenuBar(g);

      auto viewsz = ImGui::GetContentRegionAvail();
      viewsz.y -= ImGui::GetTextLineHeightWithSpacing();

      _renderViewerFBO(g, gi.outputFbo, viewsz);
      ImGui::InvisibleButton("invisbtn", viewsz);
      if (ImGui::BeginPopupContextItem()) {
         _doRightClickMenu(g);
         ImGui::EndPopup();
      }

      if (g.ui.focused) {
         _viewerHandleInput(g);
      }

      auto a = g.vpScreenArea.Min();
      auto b = g.vpScreenArea.Max();      
      
      ImGui::PushClipRect(a, b, false);      
      _renderHelpers(g);
      ImGui::PopClipRect();

      _statusBar(g);

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
