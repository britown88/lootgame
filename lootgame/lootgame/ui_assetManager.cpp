#include "stdafx.h"

#include "ui.h"
#include "app.h"

#include "reflection_gen.h"

struct AssetManagerState {
   ImGuiTextFilter searchFilter;

   bool asset_open = false;
   void *selectedAsset = nullptr;
   TypeMetadata const* selectedType = nullptr;
   UIRenderFunc customSelectedRenderer = nullptr;

   bool focusNewKey = false;
   Symbol* newKey = nullptr;
   std::string renameLbl;
   void* renameAsset = nullptr;
};

static void _doAssetEditor(AssetManagerState& state) {
   if (!state.asset_open) {
      return;
   }
   bool p_open = true;

   auto id = *(Symbol**)getReflectedMember(state.selectedAsset, state.selectedType, intern("id"));
   auto lbl = format("%s %s###AssetEditor", state.selectedType->name, id, (uintptr_t)state.selectedAsset);

   if (ImGui::Begin(lbl.c_str(), &p_open)) {
      if (state.customSelectedRenderer) {
         state.customSelectedRenderer(state.selectedType, state.selectedAsset, nullptr, nullptr);
      }
      else {
         doTypeUIEX(state.selectedType, state.selectedAsset);
      }
   }
   ImGui::End();

   if (!p_open) {
      state.asset_open = false;
      state.selectedAsset = nullptr;
      state.selectedType = nullptr;
   }
}


template<typename T>
static void _doMapTreeview(const char* label, AssetManagerState& state, std::unordered_map<Symbol*, T>& map) {

   if (!ImGui::TreeNode(label)) {
      return;
   }

   auto mapType = reflectFromRef(map);
   auto values = mapType->funcs.listKVPValues(&map);

   
   if (ImGui::Button("Add New")) {
      ImGui::OpenPopup("newkey");
      state.newKey = nullptr;
      state.focusNewKey = true;
   }
   if (ImGui::BeginPopupModal("newkey")) {
      if (state.focusNewKey) {
         ImGui::SetKeyboardFocusHere();
         state.focusNewKey = false;
      }

      doTypeUIEX(reflect<Symbol*>(), &state.newKey, nullptr, "key");
      if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Enter])) {
         T newobj;
         newobj.id = state.newKey;
         map.insert({ state.newKey, newobj });
         ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
   }

   for (auto& value : values) {
      auto t = (T*)value;

      if (t->markForDelete) {
         if (value == state.selectedAsset) {
            state.asset_open = false;
            state.selectedAsset = nullptr;
         }
         continue;
      }

      if (state.searchFilter.PassFilter(t->id)) {

         bool clicked = ImGui::Selectable(t->id);
         bool renamed = false;

         if (ImGui::BeginPopupContextItem(t->id)) {
            if (ImGui::Selectable("Delete")) {
               t->markForDelete = true;
            }
            else if (ImGui::Selectable("Rename")) {
               renamed = true;               
            }
            
            ImGui::EndPopup();
         }

         if (renamed) {
            state.renameLbl = format("Rename##%d", (uintptr_t)value);
            ImGui::OpenPopup(state.renameLbl.c_str());
            state.renameAsset = value;
            state.newKey = t->id;
            state.focusNewKey = true;
         }

         if (state.renameAsset == value && ImGui::BeginPopupModal(state.renameLbl.c_str())) {
            if (state.focusNewKey) {
               ImGui::SetKeyboardFocusHere();
               state.focusNewKey = false;
            }

            doTypeUIEX(reflect<Symbol*>(), &state.newKey, nullptr, "key");
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Enter])) {
               if (state.newKey != t->id) {
                  t->id = state.newKey;
                  T newobj = *t;
                  t->markForDelete = true;
                  map.insert({ state.newKey, newobj });
               }
               ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
         }
         
         if (ImGui::BeginDragDropSource()) {
            typeMetadataImGuiPayloadSet(t);  

            ImGui::Text("%s: %s", label, t->id);
            ImGui::EndDragDropSource();
         }

         if (clicked) {
            state.selectedType = reflect<T>();
            state.customSelectedRenderer = customUIRenderer<T>();
            state.selectedAsset = t;
            state.asset_open = true;
         }
      }
   }

   ImGui::TreePop();
}


static void _doAssetTreeview(AssetManagerState& state) {

   _doMapTreeview("Textures", state, Assets.textures);
   _doMapTreeview("Sprites", state, Assets.sprites);
   _doMapTreeview("Maps", state, Assets.maps);


   


}


static void _doAssetManager(AssetManagerState& state) {

   if (ImGui::Begin("Assets")) {
      if (ImGui::Button(ICON_FA_SAVE)) {
         assetsSave();
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_FA_RECYCLE)) {
         assetsReloadAll();
      }

      state.searchFilter.Draw(ICON_FA_SEARCH);

      if (ImGui::BeginChild("tree", ImVec2(0, 0), true)) {
         _doAssetTreeview(state);
      }
      ImGui::EndChild();
   }

   ImGui::End();


}

void uiOpenAssetManager() {
   AssetManagerState state;
   appAddGUI("Assets", [=]()mutable {
      _doAssetManager(state);
      _doAssetEditor(state);
      return true;
   });
}