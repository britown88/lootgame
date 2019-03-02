#include "stdafx.h"

#include "app.h"
#include "game.h"
#include "render.h"

#include <misc/cpp/imgui_stdlib.h>
#include <SDL2/SDL.h>

#include "scf.h"
#include "vex.h"

#include "reflection_gen.h"




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


void uiEditDude(Dude& dude) {
   auto& c = Const;
   if (ImGui::Begin("Dude Editor", 0)) {

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Lighting")) {
         ImGui::Indent();

         //ImGui::InputFloat("Floor Height", &cFloorHeight, 0.01f, 0.1f, 4);
         //ImGui::InputFloat("Dude Height", &cDudeHeight, 0.01f, 1.0f, 4);
         //ImGui::InputFloat("Light Height", &cLightHeight, 0.01f, 1.0f, 4);

         ImGui::DragFloat("Floor Height", &c.floorHeight, 0.01f, 0.0f, 2.0f);
         ImGui::DragFloat("Dude Height", &c.dudeHeight, 0.01f, 0.0f, 2.0f);
         ImGui::DragFloat("Light Height", &c.lightHeight, 0.01f, 0.0f, 2.0f);

         ImGui::DragFloat("Linear Portion", &c.lightLinearPortion, 0.1f, 0.0f, 1.0f);
         ImGui::DragFloat("Smoothness Factor", &c.lightSmoothingFactor, 0.1f, 0.0f, 50.0f);
         ImGui::DragFloat("Intensity Scalar", &c.lightIntensity, 0.1f, 1.0f, 100.0f);



         ImGui::Unindent();
      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Movement")) {
         ImGui::Indent();

         //ImGui::InputFloat("Acceleration", &cDudeAcceleration, 0.001f, 0.01f, 4);
         ImGui::InputFloat("Rotation Speed", &c.dudeRotationSpeed, 0.01f, 0.1f, 4);
         if (ImGui::InputFloat("Move Speed", &dude.phy.maxSpeed, 0.01f, 0.1f, 4)) {
            c.dudeMoveSpeed = dude.phy.maxSpeed;
         }
         ImGui::InputFloat("Dash Speed", &c.dudeDashSpeed, 0.01f, 0.1f, 4);
         ImGui::InputFloat("Dash Distance", &c.dudeDashDistance, 10.0f, 10.0f, 0);
         ImGui::InputFloat("Knockback Distance", &c.dudeKnockbackDistance, 10.0f, 10.0f, 0);
         ImGui::InputFloat("Speed Cap Accel", &c.dudeSpeedCapEasing, 0.001f, 0.01f, 4);
         ImGui::InputFloat("Backward Penalty", &c.dudeBackwardsPenalty, 0.1f, 0.01f, 4);

         float ratio = dude.mv.moveSpeedCap / 0.5f;
         ImGui::ProgressBar(ratio, ImVec2(-1, 0), "Speed Cap");

         ImGui::Unindent();
      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Timing")) {
         ImGui::Indent();

         Milliseconds shrt = 50, lng = 100;
         ImGui::InputScalar("Post-Dash CD", ImGuiDataType_U64, &c.dudePostDashCooldown, &shrt, &lng);
         ImGui::InputScalar("Stamina Base Tick Speed", ImGuiDataType_U64, &c.dudeBaseStaminaTickRecoveryTime, &shrt, &lng);

         ImGui::InputScalar("Damaged Stamina Empty CD", ImGuiDataType_U64, &c.cooldownOnDamagedStaminaEmpty, &shrt, &lng);
         ImGui::InputScalar("Damaged Stamina CD", ImGuiDataType_U64, &c.cooldownOnDamagedStamina, &shrt, &lng);
         ImGui::InputScalar("Damaged Health CD", ImGuiDataType_U64, &c.cooldownOnDamagedHealth, &shrt, &lng);
         ImGui::Unindent();

      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Attack")) {
         ImGui::Indent();
         int i = 0;
         for (auto &&swing : dude.moveset.swings) {
            ImGui::PushID(&swing);

            if (ImGui::CollapsingHeader(format("Swing %d", i++).c_str())) {
               ImGui::Indent();

               ImGui::InputFloat("Angle", &swing.swipeAngle, 10.0f, 10.0f, 1);
               Milliseconds shrt = 50, lng = 100;
               ImGui::InputFloat("Lunge Speed", &swing.lungeSpeed, 0.01f, 0.1f, 4);
               ImGui::InputFloat("Lunge Distance", &swing.lungeDist, 10.0f, 10.0f, 0);
               ImGui::InputScalar("Swing Duration", ImGuiDataType_U64, &swing.swingDur, &shrt, &lng);
               ImGui::InputScalar("Windup Duration", ImGuiDataType_U64, &swing.windupDur, &shrt, &lng);
               ImGui::InputScalar("Cooldown Duration", ImGuiDataType_U64, &swing.cooldownDur, &shrt, &lng);

               ImGui::Unindent();
            }

            ImGui::PopID();
         }

         ImGui::Unindent();

      }
   }
   ImGui::End();
}
