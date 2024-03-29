#include "stdafx.h"

#include "defs.h"
#include "ui.h"
#include "app.h"

#include "reflection_gen.h"
#include <SDL_keycode.h>

static ImGuiWindowFlags BorderlessFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoDocking;


static void _renderViewerFBO(GameState& g, FBO& output, ImVec2 sz) {

   auto fboSz = output.sz;
   fboSz.x *= EGA_PIXEL_WIDTH;
   fboSz.y *= EGA_PIXEL_HEIGHT;

   auto rect = getProportionallyFitRect(fboSz, { (int32_t)sz.x, (int32_t)sz.y });

   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   const ImVec2 p = ImGui::GetCursorScreenPos();
   auto a = ImVec2(p.x + rect.x, p.y + rect.y);
   auto b = ImVec2(p.x + rect.x + rect.w, p.y + rect.y + rect.h);

   g.vpScreenArea = { a.x, a.y, b.x - a.x, b.y - a.y };

   draw_list->AddRectFilled(a, b, IM_COL32_BLACK);

   // this assumes all fbos are linear, and forces to convert them on the final render to the imgui window
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
   ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);

   ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
   ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);

   if (ImGui::Begin("fullscreenViewer", nullptr, BorderlessFlags)) {
      _renderViewerFBO(g.state, g.outputFbo, ImGui::GetContentRegionAvail());
      //g.mouseActive = true;
   }
   ImGui::End();

   ImGui::PopStyleColor();
   ImGui::PopStyleVar(3);
}

static void _viewerMenuBar(GameState& g) {
   if (ImGui::BeginMenuBar()) {

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

static void _debugger(GameState&g) {
   ImGui::Text("Palette");
   ImGui::Separator();
   ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
   uiPaletteEditor("##palette", g.palette);
}

static bool _showWindowedViewer(GameInstance& gi) {
   bool p_open = true;
   auto& g = gi.state;

   if (ImGui::Begin(gi.winTitle.c_str(), &p_open, ImGuiWindowFlags_MenuBar)) {
      g.ui.focused = ImGui::IsWindowFocused();

      _viewerMenuBar(g);

      
      ImGui::Columns(2);
      if (ImGui::IsWindowAppearing()) {
         ImGui::SetColumnWidth(0, 250.0f);
      }
      ImGui::BeginChild("debugger");
      _debugger(g);
      ImGui::EndChild();            

      ImGui::NextColumn();

      auto viewsz = ImGui::GetContentRegionAvail();
      viewsz.y -= ImGui::GetTextLineHeightWithSpacing();

      _renderViewerFBO(g, gi.outputFbo, viewsz);
      // wacky cursor shenanigans
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (g.vpScreenArea.y - ImGui::GetCursorScreenPos().y));
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (g.vpScreenArea.x - ImGui::GetCursorScreenPos().x));

      if (g.vpScreenArea.w > 0.0f && g.vpScreenArea.h > 0.0f) {
         ImGui::InvisibleButton("invisbtn", g.vpScreenArea.sz());
      }

      auto cPos = ImGui::GetCursorPos();

      if (ImGui::IsMouseReleased(MOUSE_RIGHT) && 
         ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
         g.ui.rightClickMousePos = g.io.mousePos;
      }
      if (ImGui::BeginPopupContextItem()) {
         
         //_doRightClickMenu(g);
         ImGui::EndPopup();
      }
      auto a = g.vpScreenArea.Min();
      auto b = g.vpScreenArea.Max();
      ImGui::PushClipRect(a, b, false);

      ImGui::PopClipRect();

      ImGui::SetCursorPos(cPos);
      _statusBar(g);
      ImGui::Columns();
   }
   ImGui::End();

   if (!p_open) {
      //cleanup?
   }

   return p_open;
}



bool gameDoUIWindow(GameInstance& inst) {
   bool out = false;
   if (inst.state.ui.fullscreen) {
      _showFullScreenViewer(inst);
      out =  true;
   }
   else {
      out = _showWindowedViewer(inst);
   }

   // handle hotkeys todo: this will be handled by gamestate later
   auto& io = ImGui::GetIO();
   auto& keys = io.KeyMap;
   if (ImGui::IsKeyPressed(keys[ImGuiKey_Escape])) {
      appClose();
   }
   if (io.KeyCtrl && ImGui::IsKeyPressed(keys[ImGuiKey_Enter])) {
      inst.state.ui.fullscreen = !inst.state.ui.fullscreen;
   }
   return out;
}

