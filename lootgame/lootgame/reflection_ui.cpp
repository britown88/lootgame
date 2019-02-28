#include "reflection.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

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
   
   bool hasStep = member->ui.step > 0.0f;
   bool hasRange = member->ui.min < member->ui.max;

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


bool doTypeUIEX(TypeMetadata const* type, void* data, StructMemberMetadata const* parent, const char* label) {
   if (!type) {
      return false;
   }

   auto str_label = "";
   if (parent) {
      str_label = parent->name;
   }
   if (label) {
      str_label = label;
   }

   ImGui::PushID(data);

   bool readOnly = parent && parent->flags&StructMemberFlags_ReadOnly;

   if (readOnly) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
   }

   DEFER{ 
      if (readOnly) {
         ImGui::PopItemFlag();
         ImGui::PopStyleVar();
      }
      ImGui::PopID(); 
   };

   switch (type->variety) {
   case TypeVariety_Basic: {
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
      if (type == meta_symbol) { ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(*(StringView*)data); return false; }
   }  break;
   case TypeVariety_Struct: {
      bool shown = true;
      if (parent) {
         shown = ImGui::CollapsingHeader(str_label, ImGuiTreeNodeFlags_DefaultOpen);
      }

      if (shown) {
         if (parent) ImGui::Indent();
         for (auto&& member : type->structMembers) {
            if (member.flags&StructMemberFlags_StaticArray) {
               if (ImGui::CollapsingHeader(member.name)) {
                  ImGui::Indent();

                  for (int i = 0; i < member.staticArraySize; ++i) {
                     doTypeUIEX(member.type, (byte*)data + member.offset + (member.type->size * i), &member, std::to_string(i).c_str());
                  }

                  ImGui::Unindent();
               }
            }
            else {
               doTypeUIEX(member.type, (byte*)data + member.offset, &member);
            }
         }
      
         if (parent) ImGui::Unindent();
      }

   }  break;
   case TypeVariety_Enum: {
      if (type->enumFlags&EnumFlags_Bitfield) {

         if (ImGui::CollapsingHeader(str_label)) {
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
               }
            }
            ImGui::Unindent();
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

         if (ImGui::BeginCombo(str_label, entry ? entry->name : "")) {
            for (auto&&e : type->enumEntries) {
               if (ImGui::MenuItem(e.name)) {
                  void assignEnumValue(size_t enumSize, int64_t entryValue, void*target);
                  assignEnumValue(type->size, e.value, data);
               }
            }
            ImGui::EndCombo();
         }
      }

   }  break;
   case TypeVariety_Array:
   case TypeVariety_KVP:
      type->funcs.doUI(data, parent, label);
      break;
   }

   return false;
}
