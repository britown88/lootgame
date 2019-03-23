#include "stdafx.h"

#include "ui.h"
#include "app.h"

#include "reflection_gen.h"

enum EditorMode {
   EditorMode_Closed,
   EditorMode_Const,
   EditorMode_Asset,
};

struct AssetManagerState {
   ImGuiTextFilter searchFilter;

   EditorMode mode;

   void *selectedAsset = nullptr;
   TypeMetadata const* selectedType = nullptr;
   UIRenderFunc customSelectedRenderer = nullptr;

   bool focusNewKey = false;
   Symbol* newKey = nullptr;
   std::string renameLbl;
   void* renameAsset = nullptr;
};

static void _doConstEditor(AssetManagerState& state) {
   if (state.mode != EditorMode_Const) {
      return;
   }
   bool p_open = true;


   if (ImGui::Begin("Constants###AssetEditor", &p_open)) {
      doTypeUI(&Assets.constants);
   }
   ImGui::End();

   if (!p_open) {
      state.mode = EditorMode_Closed;
   }
}

static void _doAssetEditor(AssetManagerState& state) {
   if (state.mode != EditorMode_Asset) {
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
      state.mode = EditorMode_Closed;
   }
}


template<typename T>
static void _doMapTreeview(const char* label, AssetManagerState& state, std::unordered_map<Symbol*, T>& map) {

   if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
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
         if (state.mode == EditorMode_Asset && value == state.selectedAsset) {
            state.mode = EditorMode_Closed;
            state.selectedAsset = nullptr;
         }
         continue;
      }

      if (state.searchFilter.PassFilter(t->id)) {

         bool selected = value == state.selectedAsset;

         bool clicked = ImGui::Selectable(t->id, selected && state.mode == EditorMode_Asset);
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
            state.mode = EditorMode_Asset;
         }
      }
   }

   ImGui::TreePop();
}


static void _doAssetTreeview(AssetManagerState& state) {

   auto constFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
   if (state.mode == EditorMode_Const) {
      constFlags |= ImGuiTreeNodeFlags_Selected;
   }
   ImGui::TreeNodeEx("Constants", constFlags);
   if (ImGui::IsItemClicked()) {
      state.mode = EditorMode_Const;
   }

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
      _doConstEditor(state);
      return true;
   });
}