#include "stdafx.h"

#include "ui.h"
#include "app.h"

#include "reflection_gen.h"


struct TextureState {
   Symbol* sym;
   bool srgbPreview = true;
};

struct TextureManagerState {
   Array<TextureState> keyList;
   bool keysDirty = true;
   Symbol* newkey;
   bool focusNewKey = false;

};

static void _doTextureManager(TextureManagerState& state) {
   if (ImGui::Begin("Textures", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      if (ImGui::Button(ICON_FA_SAVE)) {
         assets_textureMapSave();
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_FA_RECYCLE)) {
         assets_textureMapReload();
      }


      if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen)) {
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
               Texture newtex;
               newtex.id = state.newkey;
               TextureMap.map.insert({ state.newkey, newtex });
               state.keysDirty = true;
               ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
         }
         ImGui::Indent();

         if (state.keysDirty) {
            state.keyList.clear();
            for (auto&&kvp : TextureMap.map) {
               state.keyList.push_back({ kvp.first });
            }
            std::sort(state.keyList.begin(), state.keyList.end(), [](TextureState const&a, TextureState const&b) {return natstrcmp(a.sym, b.sym) < 0; });
            state.keysDirty = false;
         }

         for (auto&&k : state.keyList) {
            auto& tex = TextureMap.map[k.sym];

            if (tex.markForDelete) {
               continue;
            }

            bool open = ImGui::CollapsingHeader(k.sym, ImGuiTreeNodeFlags_AllowItemOverlap);
            if (ImGui::BeginPopupContextItem()) {
               if (ImGui::MenuItem("Delete")) {
                  tex.markForDelete = true;
               }
               ImGui::EndPopup();
            }

            if (open) {
               ImGui::PushID(k.sym);

               ImGui::SameLine();
               if (ImGui::Button(ICON_FA_RECYCLE)) {
                  render::textureRefresh(tex);
               }

               ImGui::Indent();
               if (doTypeUIEX(reflect<Texture>(), &tex)) {
                  render::textureRefresh(tex);
               }

               auto w = ImGui::GetContentRegionAvailWidth() * 0.5f;
               ImVec2 imgsz = ImVec2(w, (w / tex.sz.x) * tex.sz.y);

               if (tex.handle) {
                  ImGui::Checkbox("SRGB Preview", &k.srgbPreview);

                  ImDrawList* draw_list = ImGui::GetWindowDrawList();
                  if (k.srgbPreview) {
                     draw_list->AddCallback([](auto, auto) { render::enableSRGB();  }, nullptr);
                  }

                  ImGui::Image((ImTextureID)(intptr_t)tex.handle, imgsz);

                  if (k.srgbPreview) {
                     draw_list->AddCallback([](auto, auto) { render::disableSRGB();  }, nullptr);
                  }
               }



               ImGui::PopID();

               ImGui::Unindent();
            }

         }

         ImGui::Unindent();
      }
   }
   ImGui::End();
}

void uiOpenTextureManager() {
   TextureManagerState state;
   appAddGUI("Textures", [=]()mutable {
      _doTextureManager(state);
      return true;
   });
}