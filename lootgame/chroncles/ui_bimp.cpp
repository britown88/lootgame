#include "stdafx.h"
//
//#include "ui.h"
//#include "game.h"
//#include "app.h"
//#include "win.h"
//
//#include <imgui.h>
//
//#include "IconsFontAwesome.h"
//
//#include "imgui_internal.h" // for checkerboard
//
//#define MAX_IMG_DIM 0x100000
//
//#define POPUPID_COLORPICKER "egapicker"
//
//#define STORAGE_NEW_SIZE_X 0
//#define STORAGE_NEW_SIZE_Y 1
//
//enum ToolStates_ {
//   ToolStates_NONE = 0,
//   ToolStates_PENCIL,
//   ToolStates_LINES,
//   ToolStates_RECT,
//   ToolStates_FLOODFILL,
//   ToolStates_EYEDROP,
//   ToolStates_REGION_PICK,
//   ToolStates_REGION_PICKED,
//};
//typedef byte ToolStates;
//
//
//struct BIMPState {
//
//   bool fitRectAfterFirstFrame = false;
//
//   Texture pngTex;
//   EGATexture ega;
//
//   // drawn over main view, transient elements
//   Texture editTex;
//   EGATexture editEGA;
//
//   EGATexture snippet;
//   Int2 snippetPosition = { 0, 0 };
//
//   EGAPalette palette;
//   char palName[64];
//
//   ToolStates toolState = ToolStates_NONE;
//   EGAPColor popupCLickedColor = 0; // for color picker popup
//   Float2 mousePos = { 0 }; //mouse positon within the image coords (updated per frame)
//   bool mouseInImage = false; //helper boolean for every frame
//   Int2 lastMouse = { 0 }; // mouse position of last frame
//   Int2 lastPointPlaced = { 0 }; // last point added by pencil
//   bool mouseDown = false; // only gets true when mouse clicking in a valid image
//
//   float zoomLevel = 1.0f;
//   Int2 zoomOffset = {};
//   Float2 windowSize = { 0 }; //set every frame
//
//   bool egaStretch = false;
//   ImVec4 bgColor;
//   EGAColor useColors[2] = { 0, 1 };
//   bool erase = false;
//
//   std::string winName;
//
//   std::vector<EGATexture> history;
//   size_t historyPosition = 0;
//};
//
//static void _stateTexCleanup(BIMPState &state) {
//   render::textureDestroyContent(state.pngTex);
//   render::textureDestroyContent(state.editTex);
//
//   egaTextureDestroyContent(state.ega);
//   egaTextureDestroyContent(state.editEGA);
//   egaTextureDestroyContent(state.snippet);
//}
//
//static void _cleanupHistory(BIMPState &state) {
//   for (auto hist : state.history) {
//      egaTextureDestroyContent(hist);
//   }
//   state.history.clear();
//   state.historyPosition = 0;
//}
//
//static void _stateDestroy(BIMPState &state) {
//   _cleanupHistory(state);
//   _stateTexCleanup(state);
//}
//
//static void _exitRegionPicked(BIMPState &state) {
//   if (state.toolState == ToolStates_REGION_PICKED) {
//      egaTextureDestroyContent(state.snippet);
//      state.toolState = ToolStates_REGION_PICK;
//   }
//}
//
//static void _resizeTextures(BIMPState &state, Int2 newSize) {
//
//   render::textureDestroyContent(state.pngTex);
//   state.pngTex = render::textureBuild(newSize);
//
//   egaTextureResize(state.ega, newSize);
//
//   render::textureDestroyContent(state.editTex);
//   state.editTex = render::textureBuild(newSize);
//
//   egaTextureResize(state.editEGA, newSize);
//}
//
//static void _saveSnapshot(BIMPState &state) {
//
//   if (state.historyPosition + 1 < state.history.size()) {
//      for (auto iter = state.history.begin() + state.historyPosition + 1; iter != state.history.end(); ++iter) {
//         egaTextureDestroyContent(*iter);
//      }
//      state.history.erase(state.history.begin() + state.historyPosition + 1, state.history.end());
//   }
//
//   state.history.push_back(egaTextureCreateCopy(state.ega));
//   state.historyPosition = state.history.size() - 1;
//}
//static void _undo(BIMPState &state) {
//   if (state.historyPosition > 0) {
//      _exitRegionPicked(state);
//      --state.historyPosition;
//
//      auto revision = state.history[state.historyPosition];
//      auto revSize = egaTextureGetSize(revision);
//      auto curSize = egaTextureGetSize(state.ega);
//      if (revSize.x != curSize.x || revSize.y != curSize.y) {
//         _resizeTextures(state, revSize);
//      }
//
//      egaClearAlpha(state.ega);
//      egaClearAlpha(state.editEGA);
//      egaRenderTexture(state.ega, { 0,0 }, revision);
//
//   }
//}
//static void _redo(BIMPState &state) {
//   if (state.historyPosition < state.history.size() - 1) {
//      _exitRegionPicked(state);
//      ++state.historyPosition;
//
//      auto revision = state.history[state.historyPosition];
//      auto revSize = egaTextureGetSize(revision);
//      auto curSize = egaTextureGetSize(state.ega);
//      if (revSize.x != curSize.x || revSize.y != curSize.y) {
//         _resizeTextures(state, revSize);
//      }
//
//      egaClearAlpha(state.ega);
//      egaClearAlpha(state.editEGA);
//      egaRenderTexture(state.ega, { 0,0 }, revision);
//   }
//}
//
//static void _resize(BIMPState &state, Int2 newSize) {
//   auto curSize = egaTextureGetSize(state.ega);
//   if (newSize.x != curSize.x || newSize.y != curSize.y) {
//      //_exitRegionPicked(state);
//      _resizeTextures(state, newSize);
//      _saveSnapshot(state);
//   }
//}
//
//static void _fitToWindow(BIMPState &state) {
//   auto texSize = state.pngTex.sz;
//   float pxWidth = 1.0f;
//   float pxHeight = 1.0f;
//
//   if (state.egaStretch) {
//      pxWidth = EGA_PIXEL_WIDTH;
//      pxHeight = EGA_PIXEL_HEIGHT;
//   }
//
//   Float2 t = { texSize.x * pxWidth, texSize.y * pxHeight };
//
//   auto rect = getProportionallyFitRect(t, state.windowSize);
//
//   if (rect.w > rect.h) {
//      state.zoomLevel = rect.h / (float)texSize.y / pxHeight;
//   }
//   else {
//      state.zoomLevel = rect.w / (float)texSize.x / pxWidth;
//   }
//
//
//   state.zoomOffset = { 0,0 };
//}
//
//static void _cropToSnippet(BIMPState &state) {
//   auto curSize = egaTextureGetSize(state.ega);
//   auto snipSize = egaTextureGetSize(state.snippet);
//   if (snipSize.x != curSize.x || snipSize.y != curSize.y) {
//      _resizeTextures(state, snipSize);
//   }
//
//   egaClearAlpha(state.ega);
//   egaClearAlpha(state.editEGA);
//   egaRenderTexture(state.ega, { 0,0 }, state.snippet);
//   _exitRegionPicked(state);
//   _saveSnapshot(state);
//   _fitToWindow(state);
//}
//
//static std::string _genWinTitle(BIMPState *state) {
//   return format("%s BIMP - Brandon's Image Manipulation Program###BIMP%p", ICON_FA_PAINT_BRUSH, state);
//}
//
//static void _refreshEditTextures(BIMPState &state) {
//   render::textureDestroyContent(state.editTex);
//   egaTextureDestroyContent(state.editEGA);
//
//   state.editEGA = egaTextureCreate(state.pngTex.sz);
//   state.editTex = render::textureBuild(state.pngTex.sz);
//
//   egaClearAlpha(state.editEGA);
//}
//
//static std::string _getPng() {
//   OpenFileConfig cfg;
//   cfg.filterNames = "PNG Image (*.png)";
//   cfg.filterExtensions = "*.png";
//   cfg.initialDir = cwd();
//
//   return openFile(cfg);
//}
//
//
//
//static void _loadPNG(BIMPState &state) {
//   auto png = _getPng();
//   if (!png.empty()) {
//
//      _stateTexCleanup(state);
//      state.pngTex = render::textureBuildFromFile(png.c_str());
//
//      auto palName = pathGetFilename(png.c_str());
//      strcpy(state.palName, palName.c_str());
//
//      _fitToWindow(state);
//   }
//}
//
//static void _colorButtonEGAStart(EGAColor c) {
//   auto egac = egaGetColor(c);
//   ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(egac.r, egac.g, egac.b, 255));
//   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(egac.r, egac.g, egac.b, 128));
//   ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(egac.r, egac.g, egac.b, 255));
//}
//static void _colorButtonEGAEnd() {
//   ImGui::PopStyleColor(3);
//}
//
//static bool _doColorSelectButton(BIMPState &state, uint32_t idx) {
//   auto label = format("##select%d", idx);
//   auto popLabel = format("%spopup", label.c_str());
//   _colorButtonEGAStart(state.palette.colors[state.useColors[idx]]);
//   auto out = ImGui::Button(label.c_str(), ImVec2(32.0f - 8 * idx, 32.0f - 8 * idx));
//
//   if (ImGui::BeginDragDropTarget()) {
//      if (auto payload = ImGui::AcceptDragDropPayload(UI_DRAGDROP_PALCOLOR)) {
//         auto plData = (uiDragDropPalColor*)payload->Data;
//         if (plData->paletteIndex < EGA_PALETTE_COLORS) {
//            state.useColors[idx] = plData->paletteIndex;
//         }
//      }
//      ImGui::EndDragDropTarget();
//   }
//   _colorButtonEGAEnd();
//
//   if (out) {
//      ImGui::OpenPopup(popLabel.c_str());
//   }
//
//   if (ImGui::BeginPopup(popLabel.c_str(), ImGuiWindowFlags_AlwaysAutoResize)) {
//      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
//
//      for (byte i = 0; i < 16; ++i) {
//         ImGui::PushID(i);
//         _colorButtonEGAStart(state.palette.colors[i]);
//         if (ImGui::Button("", ImVec2(20, 20))) {
//            state.useColors[idx] = i;
//            ImGui::CloseCurrentPopup();
//         }
//         _colorButtonEGAEnd();
//         ImGui::PopID();
//
//         ImGui::SameLine();
//      }
//
//      ImGui::PopStyleVar();
//      ImGui::EndPopup();
//   }
//
//   return out;
//}
//
//static void _setTool(BIMPState &state, ToolStates tool) {
//   _exitRegionPicked(state);
//   egaClearAlpha(state.editEGA);
//   state.mouseDown = false;
//   state.toolState = tool;
//}
//
//static void _doToolbar(BIMPState &state) {
//   auto &imStyle = ImGui::GetStyle();
//
//   //if (ImGui::Begin("historydebug", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
//   //   ImGui::Text("History Position: %d", state.historyPosition);
//   //   int i = 0;
//   //   for (auto h : state.history) {
//   //      ImGui::Text("Snapshot %d", i++);
//   //   }
//   //}
//   //ImGui::End();
//
//   if (ImGui::CollapsingHeader("Palette", ImGuiTreeNodeFlags_DefaultOpen)) {
//      ImGui::Indent();
//      //uiPaletteEditor(state.palette, state.palName, 64, PaletteEditorFlags_ENCODE);
//      ImGui::Unindent();
//   }
//   if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
//
//      ImGui::Indent();
//
//      bool btnNew = ImGui::Button(ICON_FA_FILE " New"); ImGui::SameLine();
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(Ctrl+N)"); }
//      bool btnLoad = ImGui::Button(ICON_FA_UPLOAD " Load"); ImGui::SameLine();
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(Ctrl+L)"); }
//      bool btnSave = ImGui::Button(ICON_FA_DOWNLOAD " Save");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(Ctrl+S)"); }
//
//      ImGui::Separator();
//
//      for (auto i = 0; i < 2; ++i) {
//         _doColorSelectButton(state, i);
//         ImGui::SameLine();
//      }
//
//      auto btnSwapColors = ImGui::Button(ICON_FA_EXCHANGE_ALT " Swap Colors");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(X)"); }
//
//      ImGui::Separator();
//
//      ImGui::BeginGroup();
//      if (state.toolState == ToolStates_PENCIL) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnPencil = ImGui::Button(ICON_FA_PENCIL_ALT " Pencil");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(P) Shift for Lines"); }
//      if (state.toolState == ToolStates_PENCIL) { ImGui::PopStyleColor(); }
//      if (state.toolState == ToolStates_FLOODFILL) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnFill = ImGui::Button(ICON_FA_PAINT_BRUSH " Flood Fill");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(F) Shift for color replace"); }
//      if (state.toolState == ToolStates_FLOODFILL) { ImGui::PopStyleColor(); }
//      if (state.erase) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnErase = ImGui::Button(ICON_FA_ERASER " Eraser");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(E)"); }
//      if (state.erase) { ImGui::PopStyleColor(); }
//      if (state.toolState == ToolStates_EYEDROP) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnColorPicker = ImGui::Button(ICON_FA_EYE_DROPPER " Pick Color");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(C) or RightClick Pixel"); }
//      if (state.toolState == ToolStates_EYEDROP) { ImGui::PopStyleColor(); }
//      bool btnUndo = ImGui::Button(ICON_FA_UNDO " Undo");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Ctrl+Z"); }
//      ImGui::EndGroup();
//
//      ImGui::SameLine();
//
//      ImGui::BeginGroup();
//      if (state.toolState == ToolStates_LINES) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnLines = ImGui::Button(ICON_FA_STAR " Draw Lines");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(L)"); }
//      if (state.toolState == ToolStates_LINES) { ImGui::PopStyleColor(); }
//      if (state.toolState == ToolStates_RECT) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnRect = ImGui::Button(ICON_FA_SQUARE " Draw Rect");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(R) Shift for Outline"); }
//      if (state.toolState == ToolStates_RECT) { ImGui::PopStyleColor(); }
//      bool regionState = state.toolState >= ToolStates_REGION_PICK; //TODO: This is bad
//      if (regionState) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
//      bool btnRegion = ImGui::Button(ICON_FA_EXPAND " Region Select");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("(Shift+R)"); }
//      if (regionState) { ImGui::PopStyleColor(); }
//
//      bool regionPicked = state.toolState == ToolStates_REGION_PICKED;
//      if (!regionPicked) { ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f); }
//      bool btnCrop = ImGui::Button(ICON_FA_CROP " Crop");
//      auto cropttipEnabled = "(Shift+C)";
//      auto cropttipDisabled = "Select region to crop";
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip(regionPicked ? cropttipEnabled : cropttipDisabled); }
//      if (!regionPicked) { ImGui::PopStyleVar(); }
//
//      bool btnRedo = ImGui::Button(ICON_FA_REDO " Redo");
//      if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Ctrl+Y"); }
//      ImGui::EndGroup();
//
//      ImGui::Separator();
//
//      int32_t sizeBuff[2] = { 0 };
//      if (state.pngTex) {
//         auto texSize = textureGetSize(state.pngTex);
//         memcpy(sizeBuff, &texSize, sizeof(i32) * 2);
//      }
//      ImGui::AlignTextToFramePadding();
//      ImGui::Text("Size: %d x %d", sizeBuff[0], sizeBuff[1]);
//      if (state.ega) {
//         ImGui::SameLine();
//         if (ImGui::Button("Resize")) {
//            ImGui::OpenPopup("Resize");
//         }
//         if (ImGui::BeginPopupModal("Resize", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
//            auto imStore = ImGui::GetStateStorage();
//            auto newX = imStore->GetIntRef(STORAGE_NEW_SIZE_X, 32);
//            auto newY = imStore->GetIntRef(STORAGE_NEW_SIZE_Y, 32);
//
//            i32 sizeBuff[2] = { *newX, *newY };
//            if (state.ega && ImGui::IsWindowAppearing()) {
//               auto egaSize = egaTextureGetSize(state.ega);
//               memcpy(sizeBuff, &egaSize, sizeof(i32) * 2);
//            }
//
//            ImGui::Dummy(ImVec2(200, 0));
//            if (ImGui::DragInt2("Size", sizeBuff, 1, 1, MAX_IMG_DIM)) {
//               *newX = sizeBuff[0];
//               *newY = sizeBuff[1];
//            }
//
//            if (ImGui::Button("Resize") || ImGui::IsKeyPressed(SDL_SCANCODE_RETURN)) {
//               _resize(state, *(Int2*)sizeBuff);
//               ImGui::CloseCurrentPopup();
//            }
//            ImGui::EndPopup();
//         }
//      }
//
//
//      ImGui::Unindent();
//
//      auto &io = ImGui::GetIO();
//      if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_DELETE)) {
//            if (regionPicked) {
//               _exitRegionPicked(state);
//               egaClearAlpha(state.editEGA);
//               _saveSnapshot(state);
//            }
//         }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_X)) { btnSwapColors = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_P)) { btnPencil = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_F)) { btnFill = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_E)) { btnErase = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_C) && !io.KeyShift) { btnColorPicker = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_L) && !io.KeyCtrl) { btnLines = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_R) && !io.KeyShift) { btnRect = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_R) && io.KeyShift) { btnRegion = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_C) && io.KeyShift) { btnCrop = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_N) && io.KeyCtrl) { btnNew = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_L) && io.KeyCtrl) { btnLoad = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_S) && io.KeyCtrl) { btnSave = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_Z) && io.KeyCtrl) { btnUndo = true; }
//         if (ImGui::IsKeyPressed(SDL_SCANCODE_Y) && io.KeyCtrl) { btnRedo = true; }
//
//      }
//
//      if (state.ega) {
//         if (btnPencil) { _setTool(state, ToolStates_PENCIL); }
//         if (btnFill) { _setTool(state, ToolStates_FLOODFILL); }
//         if (btnErase) { state.erase = !state.erase; }
//         if (btnColorPicker) { _setTool(state, ToolStates_EYEDROP); }
//         if (btnLines) { _setTool(state, ToolStates_LINES); }
//         if (btnRect) { _setTool(state, ToolStates_RECT); }
//         if (btnRegion) { _setTool(state, ToolStates_REGION_PICK); }
//         if (btnCrop && regionPicked) { _cropToSnippet(state); }
//         if (btnUndo) { _undo(state); }
//         if (btnRedo) { _redo(state); }
//      }
//
//      if (btnSwapColors) { std::swap(state.useColors[0], state.useColors[1]); }
//      if (btnNew) {
//         ImGui::OpenPopup("New Image Size");
//      }
//
//      if (ImGui::BeginPopupModal("New Image Size", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
//         auto imStore = ImGui::GetStateStorage();
//         auto newX = imStore->GetIntRef(STORAGE_NEW_SIZE_X, 32);
//         auto newY = imStore->GetIntRef(STORAGE_NEW_SIZE_Y, 32);
//
//         i32 sizeBuff[2] = { *newX, *newY };
//         if (state.ega && ImGui::IsWindowAppearing()) {
//            auto egaSize = egaTextureGetSize(state.ega);
//            memcpy(sizeBuff, &egaSize, sizeof(i32) * 2);
//         }
//
//         ImGui::Dummy(ImVec2(200, 0));
//         if (ImGui::DragInt2("Size", sizeBuff, 1, 1, MAX_IMG_DIM)) {
//            *newX = sizeBuff[0];
//            *newY = sizeBuff[1];
//         }
//
//         if (ImGui::Button("Create!") || ImGui::IsKeyPressed(SDL_SCANCODE_RETURN)) {
//            _stateTexCleanup(state);
//            state.pngTex = textureCreateCustom(*newX, *newY, { RepeatType_CLAMP, FilterType_NEAREST });
//            state.ega = egaTextureCreate(*newX, *newY);
//            _refreshEditTextures(state);
//            egaClearAlpha(state.ega);
//            _fitToWindow(state);
//            _cleanupHistory(state);
//            _saveSnapshot(state);
//            ImGui::CloseCurrentPopup();
//         }
//
//         ImGui::EndPopup();
//      }
//
//   }
//   if (ImGui::CollapsingHeader("Encoding", ImGuiTreeNodeFlags_DefaultOpen)) {
//
//      ImGui::Indent();
//
//      ImGui::BeginGroup();
//      bool btnOpen = ImGui::Button(ICON_FA_FOLDER_OPEN " Load PNG");
//
//      if (!state.pngTex) { ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f); }
//
//      bool btnClose = ImGui::Button(ICON_FA_TRASH_ALT " Close Texture");
//
//      ImGui::EndGroup();
//      ImGui::SameLine();
//      ImGui::BeginGroup();
//
//      auto encodeBtnSize = ImVec2(
//         0,
//         ImGui::GetFrameHeight() * 2 + imStyle.ItemSpacing.y);
//
//      bool encode = ImGui::Button(ICON_FA_IMAGE " Encode!", encodeBtnSize);
//      ImGui::EndGroup();
//
//      if (!state.pngTex) { ImGui::PopStyleVar(); }
//
//      ImGui::Unindent();
//
//      if (btnOpen) {
//         _loadPNG(state);
//      }
//
//      if (btnClose) {
//         _stateTexCleanup(state);
//      }
//
//      if (encode && state.pngTex) {
//         if (state.ega) {
//            egaTextureDestroy(state.ega);
//         }
//
//         EGAPalette resultPal = { 0 };
//
//         if (auto decoded = egaTextureCreateFromTextureEncode(state.pngTex, &state.palette, &resultPal)) {
//            _exitRegionPicked(state);
//            state.ega = decoded;
//            state.palette = resultPal;
//            _refreshEditTextures(state);
//
//            egaTextureDecode(state.ega, state.pngTex, &state.palette);
//
//            _cleanupHistory(state);
//            _saveSnapshot(state);
//         }
//         else {
//            ImGui::OpenPopup("Encode Failed!");
//         }
//      }
//      uiModalPopup("Encode Failed!", "Failed to finish encoding... does your palette have at least one color in it?");
//   }
//   if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen)) {
//
//      ImGui::Indent();
//
//      ImGui::Checkbox("EGA Stretch", &state.egaStretch);
//
//      ImGui::ColorEdit4("Background Color", (float*)&state.bgColor,
//         ImGuiColorEditFlags_NoInputs |
//         ImGuiColorEditFlags_AlphaPreview |
//         ImGuiColorEditFlags_HSV |
//         ImGuiColorEditFlags_AlphaBar |
//         ImGuiColorEditFlags_PickerHueBar);
//
//      if (ImGui::Button("Fit to window")) {
//         if (state.pngTex) {
//            _fitToWindow(state);
//         }
//      }
//
//      ImGui::Unindent();
//   }
//}
//
//static Recti _makeRect(Int2 const &a, Int2 const &b) {
//   return { MIN(a.x, b.x), MIN(a.y, b.y), labs(b.x - a.x) + 1,  labs(b.y - a.y) + 1 };
//}
//
//static void _floodFill(EGATexture &tex, Int2 mousePoint, EGAPColor c) {
//   auto texSize = egaTextureGetSize(tex);
//   auto pixelCount = texSize.x * texSize.y;
//   byte *visited = new byte[pixelCount];
//   memset(visited, 0, pixelCount);
//
//   auto oc = egaTextureGetColorAt(tex, mousePoint.x, mousePoint.y);
//   std::vector<Int2> neighbors;
//   neighbors.push_back(mousePoint);
//
//   while (!neighbors.empty()) {
//      auto spot = neighbors.back();
//      neighbors.pop_back();
//      auto idx = spot.y * texSize.x + spot.x;
//
//      visited[idx] = true;
//
//      if (egaTextureGetColorAt(tex, spot.x, spot.y) == oc) {
//         egaRenderPoint(tex, spot, c);
//
//         Int2 n = { spot.x, spot.y - 1 };
//         if (n.x >= 0 && n.x < texSize.x && n.y >= 0 && n.y < texSize.y) {
//            auto idx = n.y * texSize.x + n.x;
//            if (!visited[idx]) {
//               neighbors.push_back(n);
//            }
//         }
//
//         n = { spot.x, spot.y + 1 };
//         if (n.x >= 0 && n.x < texSize.x && n.y >= 0 && n.y < texSize.y) {
//            auto idx = n.y * texSize.x + n.x;
//            if (!visited[idx]) {
//               neighbors.push_back(n);
//            }
//         }
//
//         n = { spot.x + 1, spot.y };
//         if (n.x >= 0 && n.x < texSize.x && n.y >= 0 && n.y < texSize.y) {
//            auto idx = n.y * texSize.x + n.x;
//            if (!visited[idx]) {
//               neighbors.push_back(n);
//            }
//         }
//
//         n = { spot.x - 1, spot.y };
//         if (n.x >= 0 && n.x < texSize.x && n.y >= 0 && n.y < texSize.y) {
//            auto idx = n.y * texSize.x + n.x;
//            if (!visited[idx]) {
//               neighbors.push_back(n);
//            }
//         }
//      }
//   }
//
//   visited[texSize.x*mousePoint.y + mousePoint.y] = true;
//
//   delete[] visited;
//}
//
//static void _commitEditPlane(BIMPState &state) {
//   egaRenderTexture(state.ega, { 0,0 }, state.editEGA);
//   egaClearAlpha(state.editEGA);
//   _saveSnapshot(state);
//}
//
//static void _doToolMousePressed(BIMPState &state, Int2 mouse) {
//   auto &io = ImGui::GetIO();
//
//   auto color = state.erase ? EGA_ALPHA : state.useColors[0];
//
//   switch (state.toolState) {
//   case ToolStates_REGION_PICK:
//      state.lastPointPlaced = mouse;
//      break;
//   case ToolStates_REGION_PICKED: {
//      auto snipSize = egaTextureGetSize(state.snippet);
//      Recti region = {
//         state.snippetPosition.x, state.snippetPosition.y,
//         snipSize.x, snipSize.y
//      };
//
//      if (mouse.x >= region.x && mouse.y >= region.y &&
//         mouse.x < region.x + region.w && mouse.y < region.y + region.h) {
//         state.lastPointPlaced = { mouse.x - region.x, mouse.y - region.y };
//
//         if (io.KeyShift) {
//            egaRenderTexture(state.ega, state.snippetPosition, state.snippet);
//            _saveSnapshot(state);
//         }
//
//      }
//      else {
//         _commitEditPlane(state);
//         state.lastPointPlaced = mouse;
//         _exitRegionPicked(state);
//
//      }
//
//      break; }
//
//   case ToolStates_PENCIL: {
//      egaClearAlpha(state.editEGA);
//      if (state.erase) { egaRenderTexture(state.editEGA, { 0,0 }, state.ega); }
//      if (io.KeyShift) {
//         egaRenderLine(state.editEGA, state.lastPointPlaced, mouse, color);
//         state.lastPointPlaced = mouse;
//         if (state.erase) {
//            egaClearAlpha(state.ega);
//         }
//         _commitEditPlane(state);
//         state.mouseDown = false;
//      }
//      break; }
//
//   case ToolStates_RECT: {
//      egaClearAlpha(state.editEGA);
//      if (state.erase) { egaRenderTexture(state.editEGA, { 0,0 }, state.ega); }
//      state.lastPointPlaced = mouse;
//
//      if (io.KeyShift) { egaRenderLineRect(state.editEGA, _makeRect(mouse, mouse), color); }
//      else { egaRenderRect(state.editEGA, _makeRect(mouse, mouse), color); }
//      break; }
//
//   case ToolStates_FLOODFILL: {
//      egaClearAlpha(state.editEGA);
//
//      if (io.KeyShift) {
//         egaColorReplace(state.ega, egaTextureGetColorAt(state.ega, mouse.x, mouse.y), color);
//      }
//      else {
//         _floodFill(state.ega, mouse, color);
//      }
//
//      state.mouseDown = false;
//      _saveSnapshot(state);
//      break; }
//
//   case ToolStates_LINES: {
//      egaClearAlpha(state.editEGA);
//      if (state.erase) { egaRenderTexture(state.editEGA, { 0,0 }, state.ega); }
//      state.lastPointPlaced = mouse;
//      egaRenderLine(state.editEGA, mouse, mouse, color);
//      break; }
//
//   case ToolStates_EYEDROP: {
//      auto c = egaTextureGetColorAt(state.ega, mouse.x, mouse.y);
//      if (c < EGA_PALETTE_COLORS) {
//         state.useColors[0] = c;
//         state.mouseDown = false;
//         state.toolState = ToolStates_PENCIL;
//      }
//      break; }
//   }
//
//}
//
//static bool _cutSnippet(BIMPState &state, Int2 mouse) {
//   auto snippetRegion = _makeRect(state.lastPointPlaced, mouse);
//
//   if (snippetRegion.w == snippetRegion.h == 1) {
//      return false;
//   }
//
//   if (state.snippet) {
//      egaTextureDestroy(state.snippet);
//   }
//
//   //_saveSnapshot(state);
//
//   state.snippetPosition = { snippetRegion.x, snippetRegion.y };
//   state.snippet = egaTextureCreate(snippetRegion.w, snippetRegion.h);
//   egaClearAlpha(state.snippet);
//
//   egaRenderTexturePartial(state.snippet, { 0,0 }, state.ega, snippetRegion);
//   egaRenderRect(state.ega, snippetRegion, EGA_ALPHA);
//   egaClearAlpha(state.editEGA);
//   egaRenderTexture(state.editEGA, state.snippetPosition, state.snippet);
//
//   return true;
//}
//
//static void _doToolMouseReleased(BIMPState &state, Int2 mouse) {
//   switch (state.toolState) {
//   case ToolStates_REGION_PICK:
//      if (_cutSnippet(state, mouse)) {
//         state.toolState = ToolStates_REGION_PICKED;
//      }
//      break;
//   case ToolStates_LINES:
//   case ToolStates_RECT:
//   case ToolStates_PENCIL: {
//      if (state.erase) {
//         egaClearAlpha(state.ega);
//      }
//      _commitEditPlane(state);
//      break; }
//   }
//}
//
//static void _doToolMouseDown(BIMPState &state, Int2 mouse) {
//   auto &io = ImGui::GetIO();
//   auto color = state.erase ? EGA_ALPHA : state.useColors[0];
//
//   switch (state.toolState) {
//   case ToolStates_REGION_PICKED: {
//      egaClearAlpha(state.editEGA);
//      state.snippetPosition = { mouse.x - state.lastPointPlaced.x, mouse.y - state.lastPointPlaced.y };
//      egaRenderTexture(state.editEGA, state.snippetPosition, state.snippet);
//      break; }
//   case ToolStates_PENCIL: {
//      egaRenderLine(state.editEGA, state.lastMouse, mouse, color);
//      state.lastPointPlaced = mouse;
//      break; }
//   case ToolStates_RECT: {
//      egaClearAlpha(state.editEGA);
//      if (state.erase) { egaRenderTexture(state.editEGA, { 0,0 }, state.ega); }
//
//      if (io.KeyShift) { egaRenderLineRect(state.editEGA, _makeRect(state.lastPointPlaced, mouse), color); }
//      else { egaRenderRect(state.editEGA, _makeRect(state.lastPointPlaced, mouse), color); }
//
//      break; }
//   case ToolStates_LINES: {
//      egaClearAlpha(state.editEGA);
//      if (state.erase) { egaRenderTexture(state.editEGA, { 0,0 }, state.ega); }
//
//      egaRenderLine(state.editEGA, state.lastPointPlaced, mouse, color);
//      break; }
//   }
//
//}
//
//struct SnippetPayload {
//   EGATexture snippet;
//   EGAPalette palette;
//   Int2 dragPos = { 0,0 };
//};
//
//
//// this function immediately follows the invisibile dummy button for the viewer
//// all handling of mouse/keyboard for interactions with the viewer should go here!
//static void _handleInput(BIMPState &state, Float2 pxSize, Float2 cursorPos) {
//   auto &io = ImGui::GetIO();
//
//   if (state.ega) {
//      if (ImGui::BeginDragDropTarget()) {
//         if (auto payload = ImGui::AcceptDragDropPayload(UI_DRAGDROP_SNIPPET, ImGuiDragDropFlags_AcceptBeforeDelivery)) {
//            auto snip = (SnippetPayload*)payload->Data;
//
//            if (snip->snippet != state.snippet) {
//               if (!payload->Delivery) {
//                  _exitRegionPicked(state);
//                  state.toolState = ToolStates_NONE;
//
//                  egaClearAlpha(state.editEGA);
//                  egaRenderTexture(state.editEGA,
//                     { (i32)state.mousePos.x - snip->dragPos.x,
//                     (i32)state.mousePos.y - snip->dragPos.y }, snip->snippet);
//               }
//               else {
//                  _exitRegionPicked(state);
//                  state.toolState = ToolStates_REGION_PICKED;
//                  state.snippetPosition =
//                  { (i32)state.mousePos.x - snip->dragPos.x,
//                     (i32)state.mousePos.y - snip->dragPos.y };
//
//                  state.snippet = egaTextureCreateCopy(snip->snippet);
//               }
//
//
//               //_saveSnapshot(state);
//            }
//         }
//         ImGui::EndDragDropTarget();
//      }
//   }
//
//   if (state.toolState == ToolStates_REGION_PICKED) {
//      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
//         SnippetPayload pload = { state.snippet, &state.palette, state.lastPointPlaced };
//         ImGui::SetDragDropPayload(UI_DRAGDROP_SNIPPET, &pload, sizeof(pload), ImGuiCond_Once);
//         ImGui::EndDragDropSource();
//      }
//   }
//
//   // clicked actions
//   if (ImGui::IsItemActive()) {
//      // ctrl+dragging move image
//      if (io.KeyCtrl && ImGui::IsMouseDragging()) {
//         state.zoomOffset.x += (i32)ImGui::GetIO().MouseDelta.x;
//         state.zoomOffset.y += (i32)ImGui::GetIO().MouseDelta.y;
//      }
//   }
//
//   // hover actions
//   if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped)) {
//      // ctrl+wheel zooming
//      if (io.KeyCtrl && fabs(io.MouseWheel) > 0.0f) {
//         auto zoom = state.zoomLevel;
//         state.zoomLevel = MIN(100, MAX(0.1f, state.zoomLevel + io.MouseWheel * 0.05f * state.zoomLevel));
//         state.zoomOffset.x = -(i32)(state.mousePos.x * state.zoomLevel * pxSize.x - (io.MousePos.x - cursorPos.x));
//         state.zoomOffset.y = -(i32)(state.mousePos.y * state.zoomLevel * pxSize.y - (io.MousePos.y - cursorPos.y));
//      }
//
//      if (state.mouseInImage) {
//         if (ImGui::IsMouseClicked(MOUSE_RIGHT)) {
//            if (state.ega) {
//               if (state.toolState == ToolStates_REGION_PICKED) {
//                  _commitEditPlane(state);
//                  _exitRegionPicked(state);
//               }
//               else {
//                  auto c = egaTextureGetColorAt(state.ega, (u32)state.mousePos.x, (u32)state.mousePos.y);
//                  if (c < EGA_COLOR_UNDEFINED) {
//
//                     if (io.KeyCtrl) {
//                        state.popupCLickedColor = c;
//                        ImGui::OpenPopup(POPUPID_COLORPICKER);
//                     }
//                     else {
//                        state.useColors[0] = c;
//                     }
//                  }
//               }
//            }
//         }
//      }
//   }
//
//   // now we handle actual tool functions, which dont happen if we're holding ctrl
//
//   if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
//      return;
//   }
//
//   if (!state.ega || io.KeyCtrl) {
//      state.mouseDown = false;
//      return;
//   }
//
//   Int2 m = { (i32)state.mousePos.x, (i32)state.mousePos.y };
//
//   if (state.mouseInImage && ImGui::IsMouseClicked(MOUSE_LEFT)) {
//      state.mouseDown = true;
//      _doToolMousePressed(state, m);
//      state.lastMouse = m;
//   }
//
//   if (ImGui::IsMouseDown(MOUSE_LEFT) && state.mouseDown) {
//      _doToolMouseDown(state, m);
//      state.lastMouse = m;
//   }
//
//   if (ImGui::IsMouseReleased(MOUSE_LEFT) && state.mouseDown) {
//      _doToolMouseReleased(state, m);
//      state.mouseDown = false;
//   }
//
//
//}
//
//static ImVec2 _imageToScreen(Float2 imgCoords, BIMPState &state, ImVec2 const &p) {
//   float pxWidth = 1.0f;
//   float pxHeight = 1.0f;
//
//   if (state.egaStretch) {
//      pxWidth = EGA_PIXEL_WIDTH;
//      pxHeight = EGA_PIXEL_HEIGHT;
//   }
//
//   return {
//      imgCoords.x * pxWidth * state.zoomLevel + p.x + state.zoomOffset.x,
//      imgCoords.y * pxHeight * state.zoomLevel + p.y + state.zoomOffset.y };
//}
//
//static void _makeGuideRect(BIMPState &state, ImVec2 const &p, Float2 a_in, Float2 b_in, ImVec2 &a_out, ImVec2 &b_out) {
//   Float2 min = { MIN(a_in.x, b_in.x), MIN(a_in.y,  b_in.y) };
//   Float2 max = { MAX(a_in.x,  b_in.x) + 1, MAX(a_in.y,  b_in.y) + 1 };
//   a_out = _imageToScreen(min, state, p);
//   b_out = _imageToScreen(max, state, p);
//}
//
//static void _doCustomToolRender(BIMPState &state, ImDrawList *draw_list, ImVec2 const &p) {
//   if (!state.ega) {
//      return;
//   }
//
//   auto &io = ImGui::GetIO();
//   auto mouseScreenPos = io.MousePos;
//   ImU32 guideCol = IM_COL32(32, 32, 32, 164);
//
//   // the region picked area we want to always show
//   if (state.toolState == ToolStates_REGION_PICKED) {
//      auto snippetSize = egaTextureGetSize(state.snippet);
//      ImVec2 a = _imageToScreen({ (f32)state.snippetPosition.x, (f32)state.snippetPosition.y }, state, p);
//      ImVec2 b = _imageToScreen({ (f32)state.snippetPosition.x + snippetSize.x, (f32)state.snippetPosition.y + snippetSize.y }, state, p);
//
//      draw_list->AddRectFilled(a, b, IM_COL32(255, 255, 255, 32));
//      draw_list->AddRect(a, b, guideCol, 1.0f, ImDrawCornerFlags_All, 1.5f);
//   }
//
//   if (!state.mouseInImage) {
//      return;
//   }
//
//   // the rest of thenguides only show if mouse in image
//   auto a = _imageToScreen(Float2{ floorf(state.mousePos.x), floorf(state.mousePos.y) }, state, p);
//   auto b = _imageToScreen(Float2{ floorf(state.mousePos.x + 1), floorf(state.mousePos.y + 1) }, state, p);
//   draw_list->AddRect(a, b, guideCol, 1.0f, ImDrawCornerFlags_All, 1.5f);
//
//   switch (state.toolState) {
//   case ToolStates_PENCIL: {
//      if (io.KeyShift) {
//         auto lasta = _imageToScreen(Float2{ (f32)state.lastPointPlaced.x, (f32)state.lastPointPlaced.y }, state, p);
//         auto lastb = _imageToScreen(Float2{ (f32)state.lastPointPlaced.x + 1, (f32)state.lastPointPlaced.y + 1 }, state, p);
//         draw_list->AddRect(lasta, lastb, guideCol, 1.0f, ImDrawCornerFlags_All, 1.5f);
//
//         auto centerA = _imageToScreen(Float2{ floorf(state.mousePos.x) + 0.5f, floorf(state.mousePos.y) + 0.5f }, state, p);
//         auto centerB = _imageToScreen(Float2{ state.lastPointPlaced.x + 0.5f, state.lastPointPlaced.y + 0.5f }, state, p);
//         draw_list->AddLine(centerA, centerB, guideCol, 1.5f);
//      }
//
//      break; }
//   case ToolStates_REGION_PICK:
//   case ToolStates_RECT: {
//      if (state.mouseDown) {
//         ImVec2 a, b;
//         _makeGuideRect(state, p,
//            { (f32)state.lastPointPlaced.x, (f32)state.lastPointPlaced.y },
//            { floorf(state.mousePos.x) , floorf(state.mousePos.y) }, a, b);
//
//         draw_list->AddRect(a, b, guideCol, 1.0f, ImDrawCornerFlags_All, 1.5f);
//      }
//      break; }
//   case ToolStates_LINES: {
//      if (state.mouseDown) {
//         auto lasta = _imageToScreen(Float2{ (f32)state.lastPointPlaced.x, (f32)state.lastPointPlaced.y }, state, p);
//         auto lastb = _imageToScreen(Float2{ (f32)state.lastPointPlaced.x + 1, (f32)state.lastPointPlaced.y + 1 }, state, p);
//         draw_list->AddRect(lasta, lastb, guideCol, 1.0f, ImDrawCornerFlags_All, 1.5f);
//
//         auto centerA = _imageToScreen(Float2{ floorf(state.mousePos.x) + 0.5f, floorf(state.mousePos.y) + 0.5f }, state, p);
//         auto centerB = _imageToScreen(Float2{ state.lastPointPlaced.x + 0.5f, state.lastPointPlaced.y + 0.5f }, state, p);
//         draw_list->AddLine(centerA, centerB, guideCol, 1.5f);
//      }
//      break; }
//   }
//
//}
//
//static void _showStats(BIMPState &state, float viewHeight) {
//   auto &imStyle = ImGui::GetStyle();
//
//   static float statsCol = 32.0f;
//
//   //ImGui::GetTextLineHeight() * 2
//   auto bottomLeft = ImVec2(imStyle.WindowPadding.x, imStyle.WindowPadding.y + viewHeight);
//
//   ImGui::SetCursorPos(ImVec2(bottomLeft.x, bottomLeft.y - ImGui::GetTextLineHeight()));
//   ImGui::Text(ICON_FA_SEARCH); ImGui::SameLine(statsCol);
//   ImGui::Text("Scale: %.1f%%", state.zoomLevel * 100.0f); //ImGui::SameLine();
//
//   int lineCount = 0;
//
//   if (state.mouseInImage) {
//      ImGui::SetCursorPos(ImVec2(bottomLeft.x, bottomLeft.y - ImGui::GetTextLineHeight() - ImGui::GetTextLineHeightWithSpacing() * ++lineCount));
//      ImGui::Text(ICON_FA_MOUSE_POINTER); ImGui::SameLine(statsCol);
//      ImGui::Text("Mouse: (%.1f, %.1f)", state.mousePos.x, state.mousePos.y);
//   }
//
//   if (state.mouseDown && (state.toolState == ToolStates_RECT || state.toolState == ToolStates_REGION_PICK)) {
//      ImGui::SetCursorPos(ImVec2(bottomLeft.x, bottomLeft.y - ImGui::GetTextLineHeight() - ImGui::GetTextLineHeightWithSpacing() * ++lineCount));
//
//      auto txt = format("Region: %d x %d",
//         labs(state.lastMouse.x - state.lastPointPlaced.x) + 1,
//         labs(state.lastMouse.y - state.lastPointPlaced.y) + 1);
//
//      ImGui::Text(ICON_FA_EXPAND); ImGui::SameLine(statsCol);
//      ImGui::Text(txt.c_str());
//   }
//
//
//
//}
//
//bool _doUI(BIMPState &state) {
//   auto game = gameGet();
//   auto &imStyle = ImGui::GetStyle();
//   bool p_open = true;
//
//   ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_Appearing);
//   if (ImGui::Begin(state.winName.c_str(), &p_open, 0)) {
//      auto availSize = ImGui::GetContentRegionAvail();
//      if (ImGui::BeginChild("Toolbar", ImVec2(uiPaletteEditorWidth() + imStyle.IndentSpacing + imStyle.WindowPadding.x * 2, availSize.y))) {
//         _doToolbar(wnd, state);
//      }
//      ImGui::EndChild();
//
//      ImGui::SameLine();
//      availSize = ImGui::GetContentRegionAvail();
//      if (ImGui::BeginChild("DrawArea", availSize, true, ImGuiWindowFlags_NoScrollWithMouse)) {
//         auto viewSize = ImGui::GetContentRegionAvail();
//         state.windowSize = { viewSize.x, viewSize.y };
//
//         if (state.pngTex) {
//            auto texSize = textureGetSize(state.pngTex);
//            float pxWidth = 1.0f;
//            float pxHeight = 1.0f;
//
//            if (state.egaStretch) {
//               pxWidth = EGA_PIXEL_WIDTH;
//               pxHeight = EGA_PIXEL_HEIGHT;
//            }
//
//            Float2 drawSize = { texSize.x * pxWidth, texSize.y * pxHeight };
//
//            // this is a bit of a silly check.. imgui doesnt let us exceed custom draw calls past a certain point so 10k+
//            // draw calls from this alpha grid is bad juju so imma try and limit it here
//            auto pxSize = drawSize.x * drawSize.y;
//            float checkerGridSize = 10;
//            if (pxSize / (checkerGridSize * checkerGridSize) > 0xFFF) {
//               checkerGridSize = sqrtf(pxSize / 0xFFF);
//            }
//
//            ImDrawList* draw_list = ImGui::GetWindowDrawList();
//            const ImVec2 p = ImGui::GetCursorScreenPos();
//
//            // drawlists use screen coords
//            auto a = ImVec2(state.zoomOffset.x + p.x, state.zoomOffset.y + p.y);
//            auto b = ImVec2(state.zoomOffset.x + p.x + drawSize.x*state.zoomLevel, state.zoomOffset.y + p.y + drawSize.y*state.zoomLevel);
//
//            // get mouse position within the iamge
//            auto &io = ImGui::GetIO();
//            state.mousePos.x = (io.MousePos.x - p.x - state.zoomOffset.x) / state.zoomLevel / pxWidth;
//            state.mousePos.y = (io.MousePos.y - p.y - state.zoomOffset.y) / state.zoomLevel / pxHeight;
//            state.mouseInImage =
//               state.mousePos.x > 0.0f && state.mousePos.x < texSize.x &&
//               state.mousePos.y > 0.0f && state.mousePos.y < texSize.y &&
//               (io.MousePos.x >= p.x && io.MousePos.x < p.x + viewSize.x) &&
//               (io.MousePos.y >= p.y && io.MousePos.y < p.y + viewSize.y);
//
//            //in our own clip rect we draw an alpha grid and a border            
//            draw_list->PushClipRect(a, b, true);
//            ImGui::RenderColorRectWithAlphaCheckerboard(a, b, ImGui::GetColorU32(state.bgColor), checkerGridSize * state.zoomLevel, ImVec2());
//            draw_list->AddRect(a, b, IM_COL32(0, 0, 0, 255));
//            draw_list->PopClipRect();
//
//            //make a view-sized dummy button for capturing input
//            ImGui::InvisibleButton("##dummy", viewSize);
//            _handleInput(state, { pxWidth, pxHeight }, { p.x, p.y });
//
//            //input affects the image so only decode after handling input
//            if (state.ega) { egaTextureDecode(state.ega, state.pngTex, &state.palette); }
//            if (state.editEGA) { egaTextureDecode(state.editEGA, state.editTex, &state.palette); }
//
//            // Draw the actual image
//            // in erase mode, while dragging an operation, only render the edit plane
//            bool eraseDrawMode = state.erase && state.ega && state.mouseDown;
//
//            if (!eraseDrawMode) {
//               draw_list->AddImage((ImTextureID)textureGetHandle(state.pngTex), a, b);
//            }
//            if (state.editTex) {
//               ImU32 editColor = IM_COL32(255, 255, 255, eraseDrawMode ? 255 : 230);
//               draw_list->AddImage((ImTextureID)textureGetHandle(state.editTex), a, b, ImVec2(0, 0), ImVec2(1, 1), editColor);
//            }
//
//            // some tools can use some custom rendering
//            _doCustomToolRender(state, draw_list, p);
//
//            // show stats in the lower left
//            _showStats(state, viewSize.y);
//
//            // the popup for colorpicker from pixel
//            if (state.ega) {
//               uiPaletteColorPicker(POPUPID_COLORPICKER, &state.palette.colors[state.popupCLickedColor]);
//            }
//         }
//      }
//      ImGui::EndChild();
//
//      if (state.fitRectAfterFirstFrame) {
//         state.fitRectAfterFirstFrame = false;
//         _fitToWindow(state);
//      }
//   }
//   ImGui::End();
//
//   return p_open;
//}
//
//void uiBimpStart() {
//   auto game = gameGet();
//   BIMPState *state = new BIMPState();
//
//   strcpy(state->palName, "default");
//   if (auto pal = assetsPaletteRetrieve(game->assets, state->palName)) {
//      state->palette = *pal;
//   }
//
//   state->winName = _genWinTitle(state);
//
//   windowAddGUI(wnd, state->winName.c_str(), [=](Window*wnd) mutable {
//      bool ret = _doUI(wnd, *state);
//      if (!ret) {
//         _stateDestroy(*state);
//         delete state;
//      }
//      return ret;
//   });
//}
//
//void uiBimpHandleDrop() {
//   if (ImGui::BeginDragDropTarget()) {
//      if (auto payload = ImGui::AcceptDragDropPayload(UI_DRAGDROP_SNIPPET)) {
//         auto snip = (SnippetPayload*)payload->Data;
//         auto m = ImGui::GetMousePos();
//         uiBimpStartEX(wnd, snip->snippet, snip->palette, { m.x, m.y });
//      }
//      ImGui::EndDragDropTarget();
//   }
//}
//
//void uiBimpStartEX(EGATexture const &texture, EGAPalette &palette, Float2 cursorPos) {
//   auto game = gameGet();
//   BIMPState *state = new BIMPState();
//
//   strcpy(state->palName, "default");
//   state->palette = *palette;
//
//   state->winName = _genWinTitle(state);
//
//   auto inSize = egaTextureGetSize(texture);
//   state->pngTex = textureCreateCustom(inSize.x, inSize.y, { RepeatType_CLAMP, FilterType_NEAREST });
//   state->ega = egaTextureCreateCopy(texture);
//   _refreshEditTextures(*state);
//   state->fitRectAfterFirstFrame = true;
//
//   _saveSnapshot(*state);
//
//   windowAddGUI(wnd, state->winName.c_str(), [=](Window*wnd) mutable {
//      ImGui::SetNextWindowPos(ImVec2(cursorPos.x, cursorPos.y), ImGuiCond_Appearing);
//      bool ret = _doUI(wnd, *state);
//      if (!ret) {
//         _stateDestroy(*state);
//         delete state;
//      }
//      return ret;
//   });
//
//}
//
//
