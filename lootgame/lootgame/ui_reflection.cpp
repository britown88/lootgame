#include "stdafx.h"

#include "reflection.h"


#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "custom_ui_renders.h"

#include "win.h"
#include "app.h"
#include "ui.h"

bool customUIRender_ColorRGBAf(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label) {
   auto c = linearToSrgbf(*(ColorRGBAf*)data);
   if (ImGui::ColorEdit4(label ? label : parent->name, (float*)&c)) {
      *(ColorRGBAf*)data = sRgbToLinear(c);
      return true;
   }

   return false;
}
bool customUIRender_Int2(TypeMetadata const* type, void* data, StructMemberMetadata const* member, const char* label) {
   auto str_label = "";
   if (member) { str_label = member->name; }
   if (label) { str_label = label; }

   bool hasStep = member && member->ui.step > 0.0f;
   bool hasRange = member && member->ui.min < member->ui.max;

   bool changed = false;

   if (hasStep && hasRange) {
      changed = ImGui::DragInt2(str_label, (int*)data, member->ui.step, (int)member->ui.min, (int)member->ui.max);
   }
   else if (hasRange) {
      changed = ImGui::SliderInt2(str_label, (int*)data, (int)member->ui.min, (int)member->ui.max);
   }
   else {
      changed = ImGui::InputInt2(str_label, (int*)data);
   }

   return changed;
}
bool customUIRender_Float2(TypeMetadata const* type, void* data, StructMemberMetadata const* member, const char* label) {
   auto str_label = "";
   if (member) { str_label = member->name; }
   if (label) { str_label = label; }

   bool hasStep = member && member->ui.step > 0.0f;
   bool hasRange = member && member->ui.min < member->ui.max;

   bool changed = false;

   if (hasStep && hasRange) {
      changed = ImGui::DragFloat2(str_label, (float*)data, member->ui.step, member->ui.min, member->ui.max);
   }
   else if (hasRange) {
      changed = ImGui::SliderFloat2(str_label, (float*)data, member->ui.min, member->ui.max);
   }
   else {
      changed = ImGui::InputFloat2(str_label, (float*)data);
   }

   return changed;
}


template<typename T>
static bool _doIntegerType(void* data, StructMemberMetadata const* member, const char* label) {
   int i = (int)*(T*)data;

   auto str_label = "";
   if (member) {
      str_label = member->name;
   }
   if (label) {
      str_label = label;
   }
   
   bool hasStep = member && member->ui.step > 0.0f;
   bool hasRange = member && member->ui.min < member->ui.max;

   bool changed = false;

   if (hasStep && hasRange) {
      changed = ImGui::DragInt(str_label, &i, member->ui.step, (int)member->ui.min, (int)member->ui.max);
   }
   else if (hasRange) {
      changed = ImGui::SliderInt(str_label, &i, (int)member->ui.min, (int)member->ui.max);
      
   }
   else  {
      int step = 1;
      if (hasStep) {
         step = (int)member->ui.step;
      }
      changed = ImGui::InputInt(str_label, &i, step, step*10);
   }

   if (changed) {
      *(T*)data = (T)i;
   }

   return changed;
}

template<typename T>
static bool _doFloatType(void* data, StructMemberMetadata const* member, const char* label) {
   float f = (float)*(T*)data;

   auto str_label = "";
   if (member) {
      str_label = member->name;
   }
   if (label) {
      str_label = label;
   }

   bool hasStep = member->ui.step > 0.0f;
   bool hasRange = member->ui.min < member->ui.max;

   bool changed = false;

   if (hasStep && hasRange) {
      changed = ImGui::DragFloat(str_label, &f, member->ui.step, member->ui.min, member->ui.max);
   }
   else if (hasRange) {
      changed = ImGui::SliderFloat(str_label, &f, member->ui.min, member->ui.max);

   }
   else {
      float step = 1;
      if (hasStep) {
         step = member->ui.step;
      }
      changed = ImGui::InputFloat(str_label, &f, step, step * 10, 3);
   }

   if (changed) {
      *(T*)data = (T)f;
   }

   return changed;
}

bool doTypeUIReference(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label) {

   auto dataPtr = (byte*)*(void**)data;

   Symbol *preview = "";
   if (dataPtr) {
      if (auto keyMember = typeMetadataGetMemberById(type, parent->referenceKeyMember)) {
         preview = *(Symbol**)(dataPtr + keyMember->offset);
      }
   }

   if (ImGui::BeginCombo(label, preview)) {
      auto keys = parent->referenceOwnerType->funcs.listKVPKeys(parent->referenceOwner);
      std::sort(keys.begin(), keys.end(), [](void*a, void*b) {return natstrcmp(*(Symbol**)a, *(Symbol**)b) < 0; });

      for (auto&&key : keys) {
         Symbol* keysym = *(Symbol**)key;
         if(ImGui::Selectable(keysym, keysym == preview)) {
            parent->referenceOwnerType->funcs.retrieveKVP(parent->referenceOwner, key, (void**)data);
         }
      }

      ImGui::EndCombo();
   }

   if (ImGui::BeginDragDropTarget()) {
      if (auto obj = typeMetadataImGuiPayloadAcceptEX(type)) {
         *(void**)data = obj;
      }
      ImGui::EndDragDropTarget();
   }

   return false;
}

bool doTypeUIEX(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label) {
   if (!type) {
      return false;
   }

   StringView str_label = nullptr;
   if (parent) {
      str_label = parent->name;
   }
   if (label) {
      str_label = label;
   }

   ImGui::PushID(data);

   bool readOnly = parent && parent->flags&StructMemberFlags_ReadOnly;

   if (readOnly) {
      beginDisabled();
   }

   DEFER{ 
      if (readOnly) {
         endDisabled();
      }
      ImGui::PopID(); 
   };

   if (parent && parent->customUI) {
      return parent->customUI(type, data, parent, label);
   }

   if (parent && parent->reference) {
      return doTypeUIReference(type, data, parent, str_label);
   }

   if (parent && parent->flags&StructMemberFlags_File) {
      if (type == meta_string) {
         if (ImGui::Button(ICON_FA_FOLDER)) {
            OpenFileConfig cfg;
            cfg.filterNames = "All Files (*.*)";
            cfg.filterExtensions = "*.*";

            auto assetDir = AppConfig.assetPath.string();
            cfg.initialDir = assetDir.c_str();

            auto file = openFile(cfg);
            if (!file.empty()) {
               auto fpath = std::filesystem::path(file).string();
               fpath = fpath.replace(fpath.begin(), fpath.begin() + assetDir.size() + 1, "");

               *(std::string*)data = fpath.c_str();
               return true;
            }
         }
         ImGui::SameLine();
      }
   }

   switch (type->variety) {
   case TypeVariety_Basic: {
      if (type == meta_blob) { ImGui::Text("%s: %d bytes", str_label, ((Blob*)data)->sz); return false; }
      if (type == meta_bool)  return ImGui::Checkbox(str_label, (bool*)data);
      if (type == meta_byte)  return _doIntegerType<byte>(data, parent, label);
      if (type == meta_sbyte) return _doIntegerType<char>(data, parent, label);
      if (type == meta_i16)   return _doIntegerType<int16_t>(data, parent, label);
      if (type == meta_i32)   return _doIntegerType<int32_t>(data, parent, label);
      if (type == meta_i64)   return _doIntegerType<int64_t>(data, parent, label);
      if (type == meta_u16)   return _doIntegerType<uint16_t>(data, parent, label);
      if (type == meta_u32)   return _doIntegerType<uint32_t>(data, parent, label);
      if (type == meta_u64)   return _doIntegerType<uint64_t>(data, parent, label);
      if (type == meta_f32)   return _doFloatType<float>(data, parent, label);
      if (type == meta_f64)   return _doFloatType<double>(data, parent, label);
      if (type == meta_string) { return ImGui::InputText(str_label, (std::string*)data); }
      if (type == meta_symbol) { 
         auto& sym = *(StringView*)data;
         if (!sym) { sym = ""; }
         std::string str = sym;
         if (ImGui::InputText(str_label, &str)) {
            sym = intern(str.c_str());
            return true;
         }
         return false; 
      }
   }  break;
   case TypeVariety_Struct: {
      bool shown = true;
      if (str_label) {
         shown = ImGui::CollapsingHeader(str_label, ImGuiTreeNodeFlags_DefaultOpen);
      }

      if (shown) {
         bool changed = false;
         if (str_label) ImGui::Indent();
         for (auto&& member : type->structMembers) {
            if (member.flags&StructMemberFlags_StaticArray) {
               if (ImGui::CollapsingHeader(member.name)) {
                  ImGui::Indent();

                  for (int i = 0; i < member.staticArraySize; ++i) {
                     if (doTypeUIEX(member.type, (byte*)data + member.offset + (member.type->size * i), &member, std::to_string(i).c_str())) {
                        changed = true;
                     }
                  }

                  ImGui::Unindent();
               }
            }
            else {
               if (doTypeUIEX(member.type, (byte*)data + member.offset, &member)) {
                  changed = true;
               }
            }
         }
      
         if (str_label) ImGui::Unindent();
         return changed;
      }

   }  break;
   case TypeVariety_Enum: {
      if (type->enumFlags&EnumFlags_Bitfield) {

         if (ImGui::CollapsingHeader(str_label)) {
            bool changed = false;
            ImGui::Indent();
            for (auto&&entry : type->enumEntries) {
               bool compareBitfieldValue(size_t enumSize, int64_t entryValue, void*data);
               bool selected = compareBitfieldValue(type->size, entry.value, data);

               if (ImGui::Selectable(entry.name, selected)) {
                  void addBitfieldValue(size_t enumSize, int64_t entryValue, void*data);
                  void removeBitfieldValue(size_t enumSize, int64_t entryValue, void*data);

                  if (selected) {
                     removeBitfieldValue(type->size, entry.value, data);
                  }
                  else {
                     addBitfieldValue(type->size, entry.value, data);
                  }
                  changed = true;
               }
            }
            ImGui::Unindent();

            return changed;
         }
      }
      else {
         EnumEntryMetadata const* entry = nullptr;
         for (auto&&e : type->enumEntries) {
            bool compareEnumValue(size_t enumSize, int64_t entryValue, void*data);
            if (compareEnumValue(type->size, e.value, data)) {
               entry = &e;
               break;
            }
         }

         bool changed = false;
         if (ImGui::BeginCombo(str_label, entry ? entry->name : "")) {
            for (auto&&e : type->enumEntries) {
               if (ImGui::MenuItem(e.name)) {
                  void assignEnumValue(size_t enumSize, int64_t entryValue, void*target);
                  assignEnumValue(type->size, e.value, data);
                  changed = true;
               }
            }
            ImGui::EndCombo();
         }

         return changed;
      }

   }  break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      return type->funcs.doUI(data, parent, label);
   }

   return false;
}
