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
   else {
      g.ui.mode = mode;
   }

   
}

static bool _wallIsConvex(Wall& w, Float2 mouse) {
   auto &pts = w.poly.points;
   auto polycopy = pts;
   polycopy.push_back(mouse);
   return polyConvex(polycopy.data(), polycopy.size());
}

static bool _pointInWall(Wall& w, Float2 mouse) {
   return pointInPoly(mouse, w.poly.points.data(), w.poly.points.size());
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
         newall.poly.points.push_back(g.io.mousePos.toWorld());
         g.map.walls.push_back(newall);
         current = &g.map.walls.back();
      }
      else {
         auto mouse = g.io.mousePos.toWorld();
         if (_wallIsConvex(*current, mouse)) {
            current->poly.points.push_back(mouse);

            Float2 min = { FLT_MAX,FLT_MAX }, max = { -FLT_MAX,-FLT_MAX };
            for (auto&&p : current->poly.points) {
               min.x = MIN(min.x, p.x);
               min.y = MIN(min.y, p.y);
               max.x = MAX(max.x, p.x);
               max.y = MAX(max.y, p.y);
            }
            current->bb = rectFromMinMax(min, max);
         }         
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

   if (ImGui::MenuItem("Spawn Test Dude")) {
      DEBUG_gameSpawnDude(g, g.io.mousePos);
   }
   if (ImGui::MenuItem("Spawn 100 Test Dudes")) {
      for(int i = 0; i < 10; ++i)
         for (int j = 0; j < 10; ++j) {
            auto coords = g.io.mousePos;

            auto p = coords.toWorld();
            p.x += i * 40;
            p.y += j * 40;

            DEBUG_gameSpawnDude(g, Coords::fromWorld(p));
         }
         
   }
   ImGui::Separator();

   switch (g.ui.mode) {
   case GameEditMode_None:
      if (ImGui::MenuItem("Edit Walls")) {
         if (!g.ui.editing) {
            _toggleEditing(g);
         }
         _setEditMode(g, GameEditMode_Walls);
      }
   case GameEditMode_Walls:
      if (g.ui.editingWall) {
         if (ImGui::MenuItem("Finish Wall")) {
            auto& w = g.ui.editingWall;
            if (w->poly.points.size() < 3) {
               for (auto iter = g.map.walls.begin(); iter != g.map.walls.end(); ++iter) {
                  if (&*iter == w) {
                     g.map.walls.erase(iter);
                     break;
                  }
               }
            }
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

static void _renderPhyObjs(GameState& g) {
   for (auto&& p : g.phySys.objs) {
      if (p->type == PhyObject::PhyType_Circle) {
         auto center = Coords::fromWorld(p->pos).toScreen(g);
         auto radius = Coords::fromWorld({ p->circle.size,  p->circle.size }).toScreen(g) - Coords::fromWorld({ 0,0 }).toScreen(g);
         ImGui::GetWindowDrawList()->AddCircle(center, radius.x, IM_COL32_WHITE);

         Float2 a = { center.x - 10, center.y };
         Float2 b = { center.x + 10, center.y };
         Float2 c = { center.x, center.y - 10 };
         Float2 d = { center.x, center.y + 10 };
         ImGui::GetWindowDrawList()->AddLine(a, b, IM_COL32_WHITE);
         ImGui::GetWindowDrawList()->AddLine(c, d, IM_COL32_WHITE);

      }
   }

   for (auto&& wall : g.map.walls) {
      auto pCount = wall.poly.points.size();
      ImU32 lineCol = IM_COL32(50, 200, 50, 255);
      ImU32 badPolyCol = IM_COL32(255, 0, 0, 255);

      for (auto iter = wall.poly.points.begin(); iter != wall.poly.points.end(); ++iter) {
         auto c = lineCol;

         auto next = iter + 1;
         if (next == wall.poly.points.end()) {
            next = wall.poly.points.begin();
         }

         auto a = Coords::fromWorld(*iter).toScreen(g);
         auto b = Coords::fromWorld(*next).toScreen(g);

         auto norm = v2Perp(v2Normalized(b - a));

         auto radius = Coords::fromWorld({ 10,10 }).toScreen(g) - Coords::fromWorld({ 0,0 }).toScreen(g);
         norm = { norm.x * radius.x, norm.y * radius.y };

         auto guideCol = IM_COL32(255, 255, 255, 128);

         auto oa = a;
         auto ob = b;
         oa += norm;
         ob += norm;
         ImGui::GetWindowDrawList()->AddLine(oa, ob, guideCol);

         oa = a; ob = b;
         oa -= norm;
         ob -= norm;
         ImGui::GetWindowDrawList()->AddLine(oa, ob, guideCol);

         ImGui::GetWindowDrawList()->AddCircle(a, radius.x, guideCol);
         ImGui::GetWindowDrawList()->AddCircle(b, radius.x, guideCol);
      }
   }
}

static void _renderWalls(GameState& g) {
   ImU32 lineCol = IM_COL32(50, 200, 50, 255);
   ImU32 badPolyCol = IM_COL32(255, 0, 0, 255);
   ImU32 bbCol = IM_COL32(0, 255, 255, 128);

   ImU32 hovCol = IM_COL32(255, 255, 0, 255);

   auto mouse = g.io.mousePos.toWorld();

   bool badPoly = false;
   if (g.ui.editingWall) {
      badPoly = !_wallIsConvex(*g.ui.editingWall, mouse);
   }

   for (auto&& wall : g.map.walls) {
      Array<ImVec2> screenPts;
      for (auto p : wall.poly.points) {
         screenPts.push_back(Coords::fromWorld(p).toScreen(g));
      }

      auto drawlist = ImGui::GetWindowDrawList();
      drawlist->AddConvexPolyFilled(screenPts.data(), screenPts.size(), IM_COL32(0,0,0,200));
      drawlist->AddPolyline(screenPts.data(), screenPts.size(), lineCol, true, 2.0f);
   }

   // render the editing wall
   if (g.ui.editingWall) {

      auto &eWall = g.ui.editingWall->poly.points;
      auto eCount = eWall.size();

      auto c = badPoly ? badPolyCol : lineCol;

      if (eCount > 0) {
         ImGui::GetWindowDrawList()->AddLine(
            Coords::fromWorld(eWall.back()).toScreen(g),
            g.io.mousePos.toScreen(g), c);
      }

      if (eCount > 1) {
         ImGui::GetWindowDrawList()->AddLine(
            Coords::fromWorld(eWall.front()).toScreen(g),
            g.io.mousePos.toScreen(g), c);
      }
   }
}

static void _renderShadowCalc(GameState& g) {
   const float SightRadius = 200;

   auto world0 = Coords::fromWorld({ 0,0 }).toScreen(g);
   auto ppos = Coords::fromWorld(g.maindude.phy.pos).toScreen(g);
   auto radius = (Coords::fromWorld({ SightRadius,  SightRadius }).toScreen(g) - world0).x;
   auto radiusSq = radius * radius;

   auto drawlist = ImGui::GetWindowDrawList();

   auto radiusCol = IM_COL32_WHITE;
   auto segmentCol = IM_COL32(255, 127, 39, 255);
   auto segmentCol2 = IM_COL32(255, 255, 128, 128);
   auto rayCol = IM_COL32(255, 0, 0, 128);

   
   Array<ConvexPoly> blockers;
   for (auto w : g.map.walls) {
      //if (w.poly.points.size() >= 3) {
         blockers.push_back(w.poly);
      //}
   }

   auto output = render::buildShadowsBuffer(g.maindude.phy.pos, SightRadius, blockers);

   for (int i = 0; i < output.size()/3; ++i) {
      ImVec2 triangle[] = {
         Coords::fromWorld(output[i * 3]).toScreen(g),
         Coords::fromWorld(output[i * 3 + 1]).toScreen(g),
         Coords::fromWorld(output[i * 3 + 2]).toScreen(g) };

      drawlist->AddConvexPolyFilled(triangle, 3, IM_COL32(0,128,255, 64));
   }
}

static void _renderHelpers(GameState& g) {
   if (g.ui.editing && g.ui.showEditGrid) {
      _renderGrid(g);
      _renderWalls(g);
      //_renderPhyObjs(g);
      _renderShadowCalc(g);
      
   }
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
         DEBUG_gameSpawnDude(g, Coords::fromWorld(Float2{(float)(rand()%100), (float)(rand()%100)}));
      }
      if (ImGui::Button("Spawn 100 Dudes", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         for (int i = 0; i < 100; ++i) {
            DEBUG_gameSpawnDude(g, Coords::fromWorld(Float2{ (float)(rand() % 100), (float)(rand() % 100) }));
         }
         
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
