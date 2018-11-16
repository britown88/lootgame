#include "app.h"
#include "game.h"

#include <imgui.h>
#include <SDL2/SDL.h>
#include "IconsFontAwesome.h"

#include "scf.h"

static ImGuiWindowFlags BorderlessFlags =
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoDocking;

struct SCFTestState {
   SCFWriter *writer = nullptr;
   char stringBuff[256] = { 0 };
   int i = 0;
   float f = 0.0;
};

struct SCFTestResultState {
   void* data;
   u32 size;
};

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

static void _mainMenu( Game* g) {
   auto game = gameDataGet();
   if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Debug")) {

         ImGui::ColorEdit4("Clear Color", (float*)&game->imgui.bgClearColor);

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


         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Tools")) {

         ImGui::EndMenu();
      }

      auto stats = format("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(stats.c_str()).x);
      ImGui::Text(stats.c_str());

      ImGui::EndMenuBar();
   }
}

#include <functional>
#include <GL/glew.h>

static void _renderViewerFBO(Game* game) {

   auto fbo = gameGetOutputFBO(game);


   auto sz = ImGui::GetContentRegionAvail();

   auto rect = getProportionallyFitRect(fbo.sz, { (i32)sz.x, (i32)sz.y });

   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   const ImVec2 p = ImGui::GetCursorScreenPos();
   auto a = ImVec2(p.x + rect.x, p.y + rect.y);
   auto b = ImVec2(p.x + rect.x + rect.w, p.y + rect.y + rect.h);

   gameDataGet()->imgui.vpScreenArea = { a.x, a.y, b.x - a.x, b.y - a.y };

   draw_list->AddRectFilled(a, b, IM_COL32_BLACK);
   draw_list->AddCallback([](auto, auto) { glEnable(GL_FRAMEBUFFER_SRGB); }, nullptr);
   draw_list->AddImage( (ImTextureID)(iPtr)fbo.out[0].handle, a, b );
   draw_list->AddCallback([](auto, auto) { glDisable(GL_FRAMEBUFFER_SRGB); }, nullptr);
}

static void _showFullScreenViewer(Game* g) {
   auto game = gameDataGet();
   auto sz = ImGui::GetIO().DisplaySize;

   auto &style = ImGui::GetStyle();

   auto vp = ImGui::GetMainViewport();

   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
   ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

   ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
   ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);

   if (ImGui::Begin("GameWindow", nullptr, BorderlessFlags)) {
      _renderViewerFBO(g);
      game->imgui.mouseHovering = true;
   }
   ImGui::End();

   ImGui::PopStyleVar(3);
}

static void _showWindowedViewer(Game* g) {
   auto game = gameDataGet();

   //auto sz = ImGui::GetIO().DisplaySize;
   //ImGui::SetNextWindowSize(ImVec2(sz.x / 2.0f, sz.y / 2.0f), ImGuiCond_Appearing);

   if (ImGui::Begin("Viewer", nullptr, 0)) {

      if (ImGui::IsWindowFocused()) {
         if (ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
            appClose();
         }
      }

      _renderViewerFBO(g);

      game->imgui.mouseHovering = ImGui::IsWindowHovered();


   }
   ImGui::End();

}

static void _doUIDebugger(Game* g) {
   ImGui::SetNextWindowSize(ImVec2(400, 1000), ImGuiCond_FirstUseEver);
   if (ImGui::Begin("Game Debugger", nullptr)) {

      
      if (ImGui::Button("Reload Shaders", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         gameReloadShaders(g);
      }
      if (ImGui::Button("Spawn Dude", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
         DEBUG_gameSpawnDude(g);
      }

      
      auto&io = gameDataGet()->io;

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Input")) {
         ImGui::Text("Left Stick: (%f, %f)", io.leftStick.x, io.leftStick.y);
         ImGui::Text("Right Stick: (%f, %f)", io.rightStick.x, io.rightStick.y);
         ImGui::Text("Left Trigger: %f", io.leftTrigger);
         ImGui::Text("Right Trigger: %f", io.rightTrigger);

         ImGui::Text("LeftTrigger: %s", io.buttonDown[GameButton_LT] ? "Down" : "Up");
         ImGui::Text("Pressed: %s", io.buttonPressed[GameButton_LT] ? "true" : "false");
         ImGui::Text("Released: %s", io.buttonReleased[GameButton_LT] ? "true" : "false");
      }

      ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Appearing);
      if (ImGui::CollapsingHeader("Fiddling")) {
         auto& c = ConstantsGet();
         auto& g = *gameDataGet();

         ImGui::Checkbox("Show Movement UI", &g.imgui.showMovementDebugging);
         ImGui::Checkbox("Show Collision Checks", &g.imgui.showCollisionDebugging);


         ImGui::SliderFloat("Ambient Light", &g.imgui.ambientLight, 0.0f, 1.0f);
      }

      


   }
   ImGui::End();
}

void gameDoUI(Game* g) {
   auto game = gameDataGet();
   if (ImGui::IsKeyPressed(SDL_SCANCODE_F1)) {
      game->imgui.showUI = !game->imgui.showUI;
   }
   

   if (game->imgui.showUI) {
      auto game = gameDataGet();
      auto sz = ImGui::GetIO().DisplaySize;
      auto &style = ImGui::GetStyle();


      auto vp = ImGui::GetMainViewport();

      ImGui::SetNextWindowViewport(vp->ID);
      ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);      
      ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

      if (ImGui::Begin("Root", nullptr, BorderlessFlags | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus)) { 
         _mainMenu(g);
         ImGui::BeginChild("content");
         ImGui::DockSpace(ImGui::GetID("dockspace"));
         ImGui::EndChild();
      }
      ImGui::End();

      _doUIDebugger(g);
      _showWindowedViewer(g);
   }
   else {
      _showFullScreenViewer(g);

      if (ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
         appClose();
      }
   }
   
}


