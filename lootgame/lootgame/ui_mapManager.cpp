#include "stdafx.h"

#include "ui.h"
#include "app.h"

#include "reflection_gen.h"


struct MapState {
   Symbol* sym;
};

struct MapManagerState {
   Array<MapState> keyList;
   bool keysDirty = true;
   Symbol* newkey;
   bool focusNewKey = false;
};

static void _doMapManager(MapManagerState& state) {
   if (ImGui::Begin("Maps", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      if (ImGui::Button(ICON_FA_SAVE)) {
         assetsSave();
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_FA_RECYCLE)) {
         //assetsReloadMaps();
      }


      if (ImGui::CollapsingHeader("Maps", ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen)) {
         ImGui::SameLine();

         if (ImGui::Button(ICON_FA_PLUS)) {
            ImGui::OpenPopup("newkey");
            state.newkey = nullptr;
            state.focusNewKey = true;
         }

         if (ImGui::BeginPopupModal("newkey")) {
            if (state.focusNewKey) {
               ImGui::SetKeyboardFocusHere();
               state.focusNewKey = false;
            }

            doTypeUIEX(reflect<Symbol*>(), &state.newkey, nullptr, "key");
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Enter])) {
               Map newtex;
               newtex.id = state.newkey;
               Assets.maps.insert({ state.newkey, newtex });
               state.keysDirty = true;
               ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
         }
         ImGui::Indent();

         if (state.keysDirty) {
            state.keyList.clear();
            for (auto&&kvp : Assets.maps) {
               state.keyList.push_back({ kvp.first });
            }
            std::sort(state.keyList.begin(), state.keyList.end(), [](MapState const&a, MapState const&b) {return natstrcmp(a.sym, b.sym) < 0; });
            state.keysDirty = false;
         }

         for (auto&&k : state.keyList) {
            auto& tex = Assets.maps[k.sym];

            if (tex.markForDelete) {
               continue;
            }

            bool open = ImGui::CollapsingHeader(k.sym, ImGuiTreeNodeFlags_AllowItemOverlap);
            if (ImGui::BeginDragDropSource()) {
               MetadataPayload pload;
               pload.metadata = reflect<Map>();
               pload.data = &tex;
               ImGui::SetDragDropPayload(TypePayload, &pload, sizeof(MetadataPayload), ImGuiCond_Once);
               ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem()) {
               if (ImGui::MenuItem("Delete")) {
                  tex.markForDelete = true;
               }
               ImGui::EndPopup();
            }

            if (open) {
               ImGui::Indent();
               ImGui::PushID(k.sym);
               doTypeUIEX(reflect<Map>(), &tex);
               ImGui::PopID();

               ImGui::Unindent();
            }
         }

         ImGui::Unindent();
      }
   }
   ImGui::End();
}

void uiOpenMapManager() {
   MapManagerState state;
   appAddGUI("Maps", [=]()mutable {
      _doMapManager(state);
      return true;
   });
}