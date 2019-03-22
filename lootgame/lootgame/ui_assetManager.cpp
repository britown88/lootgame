#include "stdafx.h"

#include "ui.h"
#include "app.h"

#include "reflection_gen.h"

struct AssetManagerState {
   ImGuiTextFilter searchFilter;

   bool asset_open = false;
   void *selectedAsset = nullptr;
   TypeMetadata const* selectedType = nullptr;

   bool focusNewKey = false;
   Symbol* newKey = nullptr;
};

static void _doAssetEditor(AssetManagerState& state) {
   if (!state.asset_open) {
      return;
   }
   bool p_open = true;

   auto id = *(Symbol**)getReflectedMember(state.selectedAsset, state.selectedType, intern("id"));
   auto lbl = format("%s %s###AssetEditor", state.selectedType->name, id, (uintptr_t)state.selectedAsset);

   if (ImGui::Begin(lbl.c_str(), &p_open)) {
      doTypeUIEX(state.selectedType, state.selectedAsset);
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
   auto keys = mapType->funcs.listKVPKeys(&map);

   
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

   for (auto& key : keys) {
      auto sym = *(Symbol**)key;
      if (state.searchFilter.PassFilter(sym)) {

         bool clicked = ImGui::Selectable(sym);
         
         if (ImGui::BeginDragDropSource()) {
            T* item = nullptr;
            if (mapType->funcs.retrieveKVP(&map, key, (void**)&item)) {
               typeMetadataImGuiPayloadSet(item);
            }  

            ImGui::Text("%s: %s", label, sym);
            ImGui::EndDragDropSource();
         }

         if (clicked) {
            T* item = nullptr;
            if (mapType->funcs.retrieveKVP(&map, key, (void**)&item)) {

               state.selectedType = reflect<T>();
               state.selectedAsset = item;
               state.asset_open = true;
            }
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