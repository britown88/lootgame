#include "stdafx.h"

#include "app.h"
#include "game.h"
#include "render.h"
#include "ui.h"

#include <misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <SDL.h>

#include "scf.h"
#include "vex.h"

#include "reflection_gen.h"

void beginDisabled() {
   ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
   ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}
void endDisabled() {
   ImGui::PopItemFlag();
   ImGui::PopStyleVar();
}


struct SCFTestState {
   SCFWriter *writer = nullptr;
   char stringBuff[256] = { 0 };
   int i = 0;
   float f = 0.0;
};

struct SCFTestResultState {
   void* data;
   uint64_t size;
};

struct VexTestState {
   std::string testInput;
};

static void _renderNode(VexNode * node) {
   std::string tag(node->tag.begin, node->tag.end);
   std::string body(node->body.begin, node->body.end);
   std::string span(node->span.begin, node->span.end);

   if (ImGui::TreeNodeEx(span.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "Tag: [%s]", tag.c_str())) {
      if (ImGui::TreeNodeEx("body", ImGuiTreeNodeFlags_DefaultOpen, "Body")) {
         ImGui::TextWrapped(body.c_str());
         ImGui::TreePop();
      }
      if (ImGui::TreeNode("Span")) {
         ImGui::TextWrapped(span.c_str());
         ImGui::TreePop();
      }

      if (ImGui::TreeNodeEx("children", ImGuiTreeNodeFlags_DefaultOpen, "Children")) {
         auto iter = node->children;
         while (iter) {
            _renderNode(iter);
            iter = iter->next;
         }

         ImGui::TreePop();
      }

      ImGui::TreePop();
   }
}

static bool _doVexTest(VexTestState &state) {
   bool p_open = true;

   if (ImGui::Begin("Vex Testing", &p_open, 0)) {
      ImGui::Columns(2);

      ImGui::InputTextMultiline("", &state.testInput, ImGui::GetContentRegionAvail());
      ImGui::NextColumn();

      if (ImGui::BeginChild("render")) {
         auto vex = vexCreate(state.testInput);
         ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Always);
         _renderNode(vex);
         vexDestroy(vex);
      }
      ImGui::EndChild();

      ImGui::Columns(1);
   }
   ImGui::End();

   return p_open;
}

static void _uiDoSCFReader(SCFReader &view) {
   while (!scfReaderNull(view) && !scfReaderAtEnd(view)) {
      if (auto i = scfReadInt(view)) {
         ImGui::PushID(i);
         ImGui::Text("Int");
         ImGui::NextColumn();
         ImGui::Text("%d", *i);
         ImGui::NextColumn();
         ImGui::PopID();
      }
      else if (auto f = scfReadFloat(view)) {
         ImGui::PushID(f);
         ImGui::Text("Float");
         ImGui::NextColumn();
         ImGui::Text("%f", *f);
         ImGui::NextColumn();
         ImGui::PopID();
      }
      else if (auto str = scfReadString(view)) {
         ImGui::PushID(str);
         ImGui::Text("String");
         ImGui::NextColumn();
         ImGui::Text("%s", str);
         ImGui::NextColumn();
         ImGui::PopID();
      }
      else {
         SCFReader subreader = scfReadList(view);
         if (!scfReaderNull(subreader)) {
            ImGui::PushID(subreader.pos);
            if (ImGui::TreeNode("List")) {
               ImGui::NextColumn(); ImGui::NextColumn();
               _uiDoSCFReader(subreader);
               ImGui::TreePop();
            }
            else {
               ImGui::NextColumn(); ImGui::NextColumn();
            }
            ImGui::PopID();
            
         }
         else {
            scfReaderSkip(view);
         }
      }
   }
}

static bool _doSCFTest(SCFTestState& state) {
   bool p_open = true;

   if (ImGui::Begin("SCF Testing", &p_open, 0)) {
      if (!state.writer) {
         if (ImGui::Button("Create a writer")) {
            state.writer = scfWriterCreate();
         }
      }

      else{
         ImGui::Columns(2);
         
         ImGui::InputInt("Int", &state.i);
         ImGui::NextColumn();
         if (ImGui::Button("Add##int")) {
            scfWriteInt(state.writer, state.i);
         }

         ImGui::NextColumn();
         ImGui::InputFloat("Float", &state.f);
         ImGui::NextColumn();
         if (ImGui::Button("Add##float")) {
            scfWriteFloat(state.writer, state.f);
         }

         ImGui::NextColumn();
         ImGui::InputText("String", state.stringBuff, 256);
         ImGui::NextColumn();
         if (ImGui::Button("Add##string")) {
            scfWriteString(state.writer, state.stringBuff);
         }

         ImGui::NextColumn(); ImGui::NextColumn();
         if (ImGui::Button("Start a new sublist")) {
            scfWriteListBegin(state.writer);
         }
         ImGui::NextColumn(); ImGui::NextColumn();
         if (ImGui::Button("End current sublist")) {
            scfWriteListEnd(state.writer);
         }

         ImGui::NextColumn();

         ImGui::Columns(1);

         DEBUG_imShowWriterStats(state.writer);

         if (ImGui::Button("Reset")) {
            scfWriterDestroy(state.writer);
            state.writer = nullptr;
         }

         if (ImGui::Button("Write and show results")) {
            SCFTestResultState result;
            result.data = scfWriteToBuffer(state.writer, &result.size);

            scfWriterDestroy(state.writer);
            state.writer = nullptr;

            auto lbl = format("SCF Result##%p", result.data);

            appAddGUI(lbl.c_str(), [=]() mutable {
               bool p_open = true;

               ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Appearing);
               if (ImGui::Begin(lbl.c_str(), &p_open, 0)) {
                  auto view = scfView(result.data);

                  ImGui::Columns(2);
                  _uiDoSCFReader(view);
                  ImGui::Columns(1);
                  
               }
               ImGui::End();

               if (!p_open) {
                  delete[] result.data;
               }

               return p_open;
            });
         }

         
      }


   }
   ImGui::End();

   if (!p_open && state.writer) {
      scfWriterDestroy(state.writer);
      state.writer = nullptr;
   }

   return p_open;
}

static void _mainMenu() {
   if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Assets")) {
         if (ImGui::MenuItem("Save")) {
            assetsSave();
         }

         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Debug")) {

         if (ImGui::MenuItem("New Game Instance")) {
            appBeginNewGameInstance();
         }

         ImGui::ColorEdit4("Clear Color", (float*)&Engine.bgClearColor);

         if (ImGui::MenuItem("Dialog Stats")) {
            appAddGUI("DialogStats", [=]() {

               bool p_open = true;
               if (ImGui::Begin("Dialog Stats", &p_open)) {
                  //ImGui::Text("Active Dialogs: %d", DEBUG_windowGetDialogCount(wnd));

                  if (ImGui::Button("Open a test dialog")) {
                     static int testest = 0;
                     auto label = format("Dialog Test##%d", testest++);

                     appAddGUI(label.c_str(), [=]() {
                        bool p_open = true;
                        if (ImGui::Begin(label.c_str(), &p_open)) {
                           ImGui::Text("Hi!");
                        }
                        ImGui::End();
                        return p_open;
                     });
                  }
               }
               ImGui::End();
               return p_open;
            });
         }
         if (ImGui::MenuItem("ImGui Demo")) {
            appAddGUI("imguidemo", [=]() mutable {
               bool p_open = true;
               ImGui::ShowDemoWindow(&p_open);
               return p_open;
            });
         }

         if (ImGui::MenuItem("SCF Testing")) {
            SCFTestState state;
            appAddGUI("SCF Testing", [=]() mutable {
               return _doSCFTest(state);
            });
         }

         if (ImGui::MenuItem("Vex Testing")) {
            VexTestState state;
            appAddGUI("Vex Testing", [=]() mutable {
               return _doVexTest(state);
            });
         }


         ImGui::EndMenu();
      }



      auto stats = format("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(stats.c_str()).x);
      ImGui::Text(stats.c_str());

      ImGui::EndMenuBar();
   }
}

static ImGuiWindowFlags BorderlessFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoDocking;


void doRootUI() {
   auto sz = ImGui::GetIO().DisplaySize;
   auto &style = ImGui::GetStyle();

   auto vp = ImGui::GetMainViewport();

   ImGui::SetNextWindowViewport(vp->ID);
   ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
   ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

   if (ImGui::Begin("root", nullptr, BorderlessFlags | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
      _mainMenu();
      ImGui::BeginChild("content");
      ImGui::DockSpace(ImGui::GetID("dockspace"));
      ImGui::EndChild();
   }
   ImGui::End();
}


uiModalResult uiModalPopup(StringView title, StringView msg, uiModalType type, StringView icon) {
   uiModalResult result = uiModalResults_CLOSED;

   if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      result = uiModalResults_OPEN;

      if (icon) {
         ImGui::Text(icon);
         ImGui::SameLine();
      }

      ImGui::Text(msg);

      switch (type) {
      case uiModalTypes_YESNO: {
         bool yes = ImGui::Button("Yes");
         ImGui::SameLine();
         bool no = ImGui::Button("No");

         if (ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
            no = true;
         }

         if (yes) { result = uiModalResults_YES; }
         else if (no) { result = uiModalResults_NO; }
         break; }

      case uiModalTypes_YESNOCANCEL: {
         bool yes = ImGui::Button("Yes");
         ImGui::SameLine();
         bool no = ImGui::Button("No");
         ImGui::SameLine();
         bool cancel = ImGui::Button("Cancel");

         if (ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
            cancel = true;
         }

         if (yes) { result = uiModalResults_YES; }
         else if (no) { result = uiModalResults_NO; }
         else if (cancel) { result = uiModalResults_CANCEL; }
         break; }

      case uiModalTypes_OK: {
         bool ok = ImGui::Button("OK");

         if (ImGui::IsKeyPressed(SDL_SCANCODE_RETURN) ||
            ImGui::IsKeyPressed(SDL_SCANCODE_SPACE)) {
            ok = true;
         }

         if (ok) { result = uiModalResults_OK; }
         break; }

      case uiModalTypes_OKCANCEL: {
         bool ok = ImGui::Button("OK");
         ImGui::SameLine();
         bool cancel = ImGui::Button("Cancel");

         if (ImGui::IsKeyPressed(SDL_SCANCODE_RETURN) ||
            ImGui::IsKeyPressed(SDL_SCANCODE_SPACE)) {
            ok = true;
         }
         if (ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
            cancel = true;
         }

         if (ok) { result = uiModalResults_OK; }
         else if (cancel) { result = uiModalResults_CANCEL; }
         break; }
      }

      if (result != uiModalResults_OPEN) {
         ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
   }

   return result;
}

static bool _paletteColorButton(ImVec4 color, ImVec2 sz = ImVec2(0,0), bool highlight = false) {
   if (highlight) {
      ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(200, 200, 200, 255));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
   }

   auto out = ImGui::ColorButton("color", color, ImGuiColorEditFlags_NoTooltip, sz);

   if (highlight) {
      ImGui::PopStyleColor();
      ImGui::PopStyleVar(2);
   }

   return out;
}

static uint8_t _palEditorOpenIndex = EGA_PALETTE_COLORS;
static uint8_t _palEditorOpenStartColor = 0;

bool uiPaletteEditor(const char* str_id, EGAPalette& pal, PaletteEditorFlags flags) {

   auto w = ImGui::CalcItemWidth();
   auto btnW = w / EGA_PALETTE_COLORS;
   
   bool value_changed = false;

   for (int i = 0; i < 16; ++i) {
      ImGui::PushID(i);
      auto rgb = egaGetColor(pal.colors[i]);
      auto imcol = ImVec4(rgb.r / 255.0f, rgb.g / 255.0f, rgb.b / 255.0f, 1.0f);

      auto cposa = ImGui::GetCursorScreenPos();
      auto cposb = cposa;
      cposb.x += btnW;
      cposb.y += ImGui::GetFrameHeight();

      bool highlight = ImGui::IsMouseHoveringRect(cposa, cposb);

      _paletteColorButton(imcol, ImVec2(btnW, 0), highlight);
      bool dragdrop = false;

      if (ImGui::BeginDragDropSource()) {
         ImGui::SetDragDropPayload(UI_DRAGDROP_PALCOLOR, &pal.colors[i], 1);

         ImGui::Text("Index #%d", i);
         ImGui::Separator();
         ImGui::ColorButton("##preview", imcol, 0, ImVec2(ImGui::GetFrameHeight() * 2, ImGui::GetFrameHeight() * 2));
         ImGui::SameLine();
         ImGui::Text("Color %d", pal.colors[i]);

         ImGui::EndDragDropSource();
         dragdrop = true;
      }

      if (ImGui::BeginDragDropTarget()) {
         if (auto pload = ImGui::AcceptDragDropPayload(UI_DRAGDROP_PALCOLOR)) {
            pal.colors[i] = *(EGAColor*)pload->Data;
         }
         if (auto ddpal = typeMetadataImGuiPayloadAccept<EGAPalette>()) {
            pal = *ddpal;
         }
         ImGui::EndDragDropTarget();
         dragdrop = true;
      }

      if (!dragdrop) {
         if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Index #%d", i);
            ImGui::Separator();
            ImGui::ColorButton("##preview", imcol, 0, ImVec2(ImGui::GetFrameHeight() * 2, ImGui::GetFrameHeight() * 2));
            ImGui::SameLine();
            ImGui::Text("Color %d", pal.colors[i]);
            ImGui::EndTooltip();
         }
         if (ImGui::BeginPopupContextItem("picker", 0)) {
            if (ImGui::IsWindowAppearing()) {
               _palEditorOpenStartColor = pal.colors[i];
            }
            if (!ImGui::IsWindowHovered()) {
               value_changed = _palEditorOpenStartColor != pal.colors[i];
               pal.colors[i] = _palEditorOpenStartColor;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            // do a 8x8 grid of all colors
            for (int y = 0; y < 8; ++y) {
               ImGui::PushID(y);
               for (int x = 0; x < 8; ++x) {
                  ImGui::PushID(x);
                  auto colidx = y * 8 + x;
                  auto rgb = egaGetColor(colidx);
                  auto imcol = ImVec4(rgb.r / 255.0f, rgb.g / 255.0f, rgb.b / 255.0f, 1.0f);

                  auto cposa = ImGui::GetCursorScreenPos();
                  auto cposb = cposa;
                  cposb.x += ImGui::GetFrameHeight();
                  cposb.y += ImGui::GetFrameHeight();
                  bool highlight = ImGui::IsMouseHoveringRect(cposa, cposb);

                  if (_paletteColorButton(imcol, ImVec2(0, 0), highlight)) {
                     ImGui::CloseCurrentPopup();
                  }
                  if (ImGui::IsItemHovered()) {
                     value_changed = pal.colors[i] != colidx;
                     pal.colors[i] = colidx;
                  }
                  ImGui::SameLine(0, 0);
                  ImGui::PopID();
               }
               ImGui::NewLine();
               ImGui::PopID();
            }

            ImGui::SameLine();
            ImGui::PopStyleVar();
            ImGui::NewLine();
            ImGui::Text("Color %d", pal.colors[i]);

            ImGui::EndPopup();
         }

      }

      
      ImGui::SameLine(0, 0);
      ImGui::PopID();
   }
   ImGui::NewLine();
   return value_changed;
}