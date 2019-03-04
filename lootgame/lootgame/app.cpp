#include "stdafx.h"

#include "app.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GL/glew.h> 

#include <stb/stb_image.h>

#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "math.h"


#include "fa_merged.cpp"

#include "render.h"
#include "ui.h"
#include "game.h"
#include "lpp.h"
#include "scf.h"

#include "reflection_gen.h"

_AppConfig AppConfig;
static bool _gameInstanceStep(GameInstance& g);

struct LogEntry {
   std::string file, message;
   LogLevel level;
};
struct Logger {
   Array<LogEntry> entries;
   bool pushBottom = false;
};

static Logger g_logger;

void _log(const char* file, LogLevel level, const char*msg) {

   std::filesystem::path p = file;


   g_logger.entries.push_back({ p.filename().string(), msg, level });
   g_logger.pushBottom = true;
}

static void _doLogger() {
   if (ImGui::Begin("Logger")) {
      static ImVec4 cols[3];
      static bool init = false;
      if (!init) {
         cols[LogLevel_Normal] = ImVec4(0.00f, 0.67f, 0.08f, 1.00f);
         cols[LogLevel_Warning] = ImVec4(0.69f, 0.74f, 0.00f, 1.00f);
         cols[LogLevel_Error] = ImVec4(0.88f, 0.25f, 0.13f, 1.00f);
      }

      auto max = ImVec2(ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeightWithSpacing());
      auto cpos = ImGui::GetCursorPosY();

      ImGui::BeginGroup();
      for (auto&& entry : g_logger.entries) {
         ImGui::TextColored(cols[entry.level], entry.file.c_str());
      }
      ImGui::EndGroup();
      ImGui::SameLine(0, 50);
      ImGui::BeginGroup();
      for (auto&& entry : g_logger.entries) {
         ImGui::TextColored(cols[entry.level], entry.message.c_str());
      }
      if (g_logger.pushBottom) {
         ImGui::SetScrollHereY(1.0f);
         g_logger.pushBottom = false;
      }
      ImGui::EndGroup();
   }
   ImGui::End();
}

static void _openLogger() {
   appAddGUI("Logger", [=] {
      _doLogger();
      return true;
   });
}

struct App {
   Array<GameInstance*> instances;
   GameInstance* lastFocused = nullptr;

   bool running = false;

   ImFontAtlas* fontAtlas;

   SDL_Window* sdlWnd = nullptr;
   SDL_GLContext sdlCtx = nullptr;
   ImGuiContext* imguiContext = nullptr;
   uint32_t fullscreen_flag = 0;

   bool shouldClose = false;

   struct Dialog {
      std::function<bool()> dlg;
      bool bringToFront = true;
   };

   std::unordered_map<std::string, Dialog> dlgs;

   uint64_t timerStart, timerFreq;
};

static App* g_app = nullptr;

App* appCreate() {
   auto out = new App();
   g_app = out;   
   return out;
}

void appDestroy(App* app) {

   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplSDL2_Shutdown();
   ImGui::DestroyContext();

   SDL_GL_DeleteContext(app->sdlCtx);
   SDL_DestroyWindow(app->sdlWnd);
   SDL_Quit();

   delete app->fontAtlas;
   delete app;
}

static void _initFontAtlas(App* app) {
   app->fontAtlas = new ImFontAtlas();
   app->fontAtlas->AddFontDefault();

   ImFontConfig config;
   config.MergeMode = true;
   static const ImWchar range_merged[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
   app->fontAtlas->AddFontFromMemoryCompressedTTF(
      fa_merged_compressed_data,
      fa_merged_compressed_size,
      13.0f, &config, range_merged);
}

static void _windowCreate(App* app, WindowConfig const& info) {
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
   {
      printf("Error: %s\n", SDL_GetError());
      return;
   }

   auto mappings = SDL_GameControllerAddMappingsFromFile("assets/gamecontrollerdb.txt");

   auto winFlags =
      SDL_WINDOW_OPENGL |
      SDL_WINDOW_RESIZABLE |
      SDL_WINDOW_MAXIMIZED;

   // Setup window
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
   SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   SDL_DisplayMode current;
   SDL_GetCurrentDisplayMode(0, &current);
   SDL_Window* window = SDL_CreateWindow(info.title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, info.w, info.h, winFlags);
   SDL_GLContext gl_context = SDL_GL_CreateContext(window);
   SDL_GL_SetSwapInterval(1); // Enable vsync
   glewInit();

   _initFontAtlas(app);

   // Setup Dear ImGui binding
   IMGUI_CHECKVERSION();
   auto imCtx = ImGui::CreateContext(app->fontAtlas);
   ImGuiIO& io = ImGui::GetIO(); (void)io;

   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
   io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

   //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

   ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
   ImGui_ImplOpenGL3_Init();

   app->timerStart = SDL_GetPerformanceCounter();
   app->timerFreq = SDL_GetPerformanceFrequency();

   GLuint vao;
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);

   // Setup style
   ImGui::StyleColorsDark();
   //ImGui::StyleColorsClassic();

   app->sdlCtx = gl_context;
   app->sdlWnd = window;
   app->imguiContext = imCtx;

}

#include "reflection_gen.h"

//@reflect{
enum MyEnum {
   MyEnum_A = 0,
   MyEnum_B,
   MyEnum_C,
   MyEnum_D,
   MyEnum_E,
   MyEnum_F,
};//}

//@reflect{ 
//@bitfield
enum MyBitfield_ : int {
   MyBitfield_A = (1 << 0),
   MyBitfield_B = (1 << 1),
   MyBitfield_C = (1 << 2),
};//}
typedef int MyBitfield;

//@reflect{
struct MyStruct {
   MyEnum enumTest = MyEnum_E;
   Array<int> ints;
   std::string staticStrings[10];

   //@type_override:MyBitfield_
   MyBitfield bitfield;

   //@ui(min=0 max=100)
   int intRange = 50;

   //@ui(min=0 max=1 step=0.001)
   float dragTest = 0.0f;

   //@readonly
   std::string readonly = "Read Only";

   //@type_override:std::unordered_map<Symbol*, MyBitfield_>
   std::unordered_map<Symbol*, MyBitfield> map;

   ColorRGBAf color;

   //@file
   std::string path;

   Blob dataTest;
   Int2 int2;
   Float2 float2;
};//}

#include  "app_reflection_gen.inl"

static void _loadReflectionTest() {
   appAddGUI("ReflectedUiTest", [] {
      bool p_open = true;
      if (ImGui::Begin("Reflected UI Test", &p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
         static MyStruct s;
         static bool init = false;
         if (!init) {
            for (int i = 0; i < 100; ++i) s.ints.push_back(i);

            s.map.insert({ intern("Sym1"), MyBitfield_A });
            s.map.insert({ intern("Sym2"), MyBitfield_A | MyBitfield_C });
            s.map.insert({ intern("Sym3"), MyBitfield_B });
            s.map.insert({ intern("Sym4"), 0 });
            s.map.insert({ intern("Sym5"), MyBitfield_C });


            init = true;
         }

         doTypeUI(&s);
      }
      ImGui::End();
      return p_open;
   });
}

static int viewerCount = 0;

void appBeginNewGameInstance() {
   auto inst = new GameInstance();
   inst->outputFbo = render::fboBuild(Const.resolution);
   inst->winTitle = format("Viewer %d", viewerCount++ + 1);
   g_app->instances.push_back(inst);
   g_app->lastFocused = inst;

   LOG("Starting game instance in Viewer %d", viewerCount);

   gameStartActionMode(inst->state);
   ImGui::SetWindowFocus(inst->winTitle.c_str());

   appAddGUI(inst->winTitle.c_str(), [=]()mutable {
      return _gameInstanceStep(*inst);
   });
}

void appCreateWindow(App* app, WindowConfig const& info) {
   _windowCreate(app, info);
   Graphics.build();   
      
   uiOpenTextureManager();
   _openLogger();
   appBeginNewGameInstance();

   app->running = true;
}



bool appRunning(App* app) { return app->running; }
void appPollEvents(App* app) {

   GameInstance* g = nullptr;
   for (auto&&i : app->instances) {
      if (i->state.ui.focused) {
         g = i;
         break;
      }
   }

   SDL_Event event;
   auto &io = ImGui::GetIO();
   while (SDL_PollEvent(&event))
   {
      ImGui_ImplSDL2_ProcessEvent(&event);

      bool gameHandled = false;

      // only allow mouserelated events to pass to game if imgui's not using the mouse
      if (!io.WantCaptureMouse) {
         switch (event.type) {
         case SDL_MOUSEBUTTONUP:
         case SDL_MOUSEBUTTONDOWN:
         case SDL_MOUSEMOTION:
         case SDL_MOUSEWHEEL:
            if (g) {
               gameHandled = gameProcessEvent(g->state, &event) || gameHandled;
            }
            
            break;
         }
      }

      // only allow keyelated events to pass to game if imgui's not using the keyboad
      if (!io.WantCaptureKeyboard) {
         switch (event.type) {
         case SDL_KEYDOWN:
         case SDL_KEYUP:
         case SDL_TEXTEDITING:
         case SDL_TEXTINPUT:
            if (g) {
               gameHandled = gameProcessEvent(g->state, &event) || gameHandled;
            }
            break;
         }
      }

      switch (event.type)
      {
      case SDL_QUIT: {
         app->running = false;
         break; }
      case SDL_WINDOWEVENT:
         switch (event.window.event) {
         case SDL_WINDOWEVENT_CLOSE:
            if (event.window.windowID == SDL_GetWindowID(app->sdlWnd)) {
               app->running = false;
            }
            break;

         }
         break;
      }

      // finally pass everything else through to the game to handle as it will
      if (!gameHandled && g) {
         gameProcessEvent(g->state, &event);
      }
   }
}




static void _beginUIFrame(App* app) {
   ImGui_ImplOpenGL3_NewFrame();
   ImGui_ImplSDL2_NewFrame(app->sdlWnd);
   ImGui::NewFrame();
}

static void _renderFrame(App* app) {

   SDL_GL_MakeCurrent(app->sdlWnd, app->sdlCtx);

   auto ccolor = Engine.bgClearColor;
   auto& io = ImGui::GetIO();

   ImGui::Render();
   
   render::viewport({ 0, 0, (int)io.DisplaySize.x , (int)io.DisplaySize.y });
   render::clear(ccolor);
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

   // Update and Render additional Platform Windows
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
   {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
   }

   SDL_GL_MakeCurrent(app->sdlWnd, app->sdlCtx);
   SDL_GL_SwapWindow(app->sdlWnd);
}

static void _updateDialogs(App* app) {
   Array<std::string> deleted;
   for (auto &dlg : app->dlgs) {

      if (dlg.second.bringToFront) {
         ImGui::SetNextWindowFocus();
         dlg.second.bringToFront = false;
      }

      if (!dlg.second.dlg()) {
         deleted.push_back(dlg.first);
      }
   }
   for (auto&d : deleted) {
      app->dlgs.erase(d);
   }
}

bool _gameInstanceStep(GameInstance& g) {
   gameUpdate(g.state);
   gameDraw(g.state,g.outputFbo);
   return gameDoUIWindow(g);
}

static void _appSetFullscreen(bool fullscreen) {
   uint32_t flag = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
   if (flag != g_app->fullscreen_flag) {
      g_app->fullscreen_flag = flag;
      SDL_SetWindowFullscreen(g_app->sdlWnd, flag);
   }
   
}

static void _doGameDebugger(App* app) {
   // find the focused instance and draw the debugger for it
   for (auto&& i : app->instances) {
      if (i->state.ui.focused) { app->lastFocused = i; break; }
   }

   if (app->lastFocused) {
      uiDoGameDebugger(*app->lastFocused);
   }
}

static void _updateFrame(App* app) {

   // first we look for a fullscreen instance and only render that
   bool fscreenupdate = false;
   for (auto&& i : app->instances) {
      if (i->state.ui.fullscreen) {
         ImGui::SetWindowFocus(i->winTitle.c_str());
         _gameInstanceStep(*i);
         app->lastFocused = i;
         fscreenupdate = true;         
         break;
      }
   }   

   // update sdlwnd with fullscreen state
   _appSetFullscreen(fscreenupdate);
   
   if(!fscreenupdate){
      // we're in ui mode so render everything
      doRootUI();
      _doGameDebugger(app);
      _updateDialogs(app);
   }

   static bool firstFrameFocus = true;
   if (firstFrameFocus) {
      ImGui::SetWindowFocus(app->lastFocused->winTitle.c_str());
      firstFrameFocus = false;
   }


}

//#include <thread>
//#include <chrono>

void appStep(App* app) {
   lppSync();

   for (auto&& i : app->instances) {
      gameBeginFrame(i->state);
   }
   
   appPollEvents(app);
   _beginUIFrame(app);
   _updateFrame(app);
   _renderFrame(app);

   if (app->shouldClose) {
      app->running = false;
   }
   //using namespace std::chrono_literals;
   //std::this_thread::sleep_for(50ms);

}


void appAddGUI(StringView label, std::function<bool()> gui) {

   auto dlg = g_app->dlgs.find(label);
   if (dlg != g_app->dlgs.end()) {
      dlg->second.bringToFront = true;
      return;
   }

   g_app->dlgs.insert({ label,{ gui } });
}

void appClose() {
   g_app->shouldClose = true;
}

Time appGetTime() {
   auto dt = SDL_GetPerformanceCounter() - g_app->timerStart;
   return { (dt * 1000000) / g_app->timerFreq };
}