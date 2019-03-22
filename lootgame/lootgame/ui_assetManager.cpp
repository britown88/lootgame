#include "stdafx.h"

#include "ui.h"
#include "app.h"

#include "reflection_gen.h"

struct AssetManagerState {
   ImGuiTextFilter searchFilter;

   bool asset_open = false;
   void *selectedAsset = nullptr;
   TypeMetadata const* selectedType = nullptr;
};

template<typename T>
static bool _doMapMemberEditor(AssetManagerState& state, std::unordered_map<Symbol*, T>& map, T* member) {
   bool p_open = true;

   auto memberType = reflect<T>();
   auto lbl = format("%s %s##%d", memberType->name, member->id, (uintptr_t)member);

   if (ImGui::Begin(lbl.c_str(), &p_open)) {
      doTypeUI(member);
   }
   ImGui::End();
   return p_open;
}

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
static void _doMapTreeview(AssetManagerState& state, std::unordered_map<Symbol*, T>& map) {

   auto mapType = reflectFromRef(map);
   auto keys = mapType->funcs.listKVPKeys(&map);

   for (auto& key : keys) {
      auto sym = *(Symbol**)key;
      if (state.searchFilter.PassFilter(sym)) {
         if (ImGui::Selectable(sym)) {
            T* item = nullptr;
            if (mapType->funcs.retrieveKVP(&map, key, (void**)&item)) {

               state.selectedType = reflect<T>();
               state.selectedAsset = item;
               state.asset_open = true;
               //appAddGUI(std::to_string((uintptr_t)item).c_str(), [=]() mutable {
               //   return _doMapMemberEditor(state, map, item);
               //});
            }
         }
      }
   }
}


static void _doAssetTreeview(AssetManagerState& state) {
   if (ImGui::TreeNode("Textures")) {      
      _doMapTreeview(state, Assets.textures);
      ImGui::TreePop();
   }

   if (ImGui::TreeNode("Sprites")) {
      _doMapTreeview(state, Assets.sprites);
      ImGui::TreePop();
   }

   if (ImGui::TreeNode("Maps")) {
      _doMapTreeview(state, Assets.maps);
      ImGui::TreePop();
   }
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