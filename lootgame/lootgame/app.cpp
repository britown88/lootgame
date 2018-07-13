#include "app.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GL/glew.h> 
#include <imgui.h>
#include <stb/stb_image.h>

#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "math.h"

#include <unordered_map>
#include <string>
#include <functional>

#include "IconsFontAwesome.h"
#include "fa_merged.cpp"

#include "render.h"
#include "game.h"


struct App {
   bool running = false;
   Game* game = nullptr;

   ImFontAtlas* fontAtlas;

   SDL_Window* sdlWnd = nullptr;
   SDL_GLContext sdlCtx = nullptr;
   ImGuiContext* imguiContext = nullptr;

   Int2 size = { 0 }, clientSize = { 0 };
   float scale = 1.0f;
   bool shouldClose = false;

   struct Dialog {
      std::function<bool()> dlg;
      bool bringToFront = true;
   };

   std::unordered_map<std::string, Dialog> dlgs;

   u64 timerStart, timerFreq;
};

static App* g_app = nullptr;

App* appCreate(AppConfig const& config) {
   auto out = new App();
   out->game = gameCreate(config.assetFolder);
   g_app = out;
   return out;
}

void appDestroy(App* app) {
   gameDestroy(app->game);

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
   //io.Fonts->AddFontDefault();

   // use this for the windows UI font, but we want package this font so
   //app->fontAtlas->AddFontFromFileTTF("C:/windows/fonts/segoeui.ttf", 16);

   ImFontConfig config;
   config.MergeMode = true;
   //config.PixelSnapH = true;
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
   //SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
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

   // Setup Dear ImGui binding
   IMGUI_CHECKVERSION();
   auto imCtx = ImGui::CreateContext();
   ImGuiIO& io = ImGui::GetIO(); (void)io;
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

void appCreateWindow(App* app, WindowConfig const& info) {
   _windowCreate(app, info);
   gameBegin(app->game);

   app->running = true;
}

bool appRunning(App* app) { return app->running; }
void appPollEvents(App* app) {
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
            gameHandled = gameHandled || gameProcessEvent(app->game, &event);
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
            gameHandled = gameHandled || gameProcessEvent(app->game, &event);
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
         case SDL_WINDOWEVENT_EXPOSED:
         case SDL_WINDOWEVENT_RESIZED:
            SDL_GetWindowSize(app->sdlWnd, &app->size.x, &app->size.y);
            SDL_GL_GetDrawableSize(app->sdlWnd, &app->clientSize.x, &app->clientSize.y);
            app->scale = app->clientSize.x / (float)app->size.x;
            break;
         }
         break;
      }

      // finally pass everything else through to the game to handle as it will
      if (!gameHandled) {
         gameProcessEvent(app->game, &event);
      }
   }
}



static void _pollEvents(App* app) {
   gameHandleInput(app->game);

   appPollEvents(app);
   
}

static void _beginFrame(App* app) {
   ImGui_ImplOpenGL3_NewFrame();
   ImGui_ImplSDL2_NewFrame(app->sdlWnd);
   ImGui::NewFrame();
}

static void _renderFrame(App* app) {
   SDL_GL_MakeCurrent(app->sdlWnd, app->sdlCtx);

   gameRender(app->game);

   auto data = gameDataGet();
   auto ccolor = data->imgui.bgClearColor;

   auto& io = ImGui::GetIO();
   ImGui::Render();

   render::viewport({ 0, 0, (int)io.DisplaySize.x , (int)io.DisplaySize.y });
   render::clear(ccolor);

   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
   SDL_GL_SwapWindow(app->sdlWnd);
}

static void _updateDialogs(App* app) {
   std::vector<std::string> deleted;
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

static void _updateFrame(App* app) {
   gameUpdate(app->game);
   _updateDialogs(app);
}

#include <thread>
#include <chrono>

void appStep(App* app) {
   _pollEvents(app);
   _beginFrame(app);

   _updateFrame(app);
   _renderFrame(app);

   if (app->shouldClose) {
      app->running = false;
   }
   using namespace std::chrono_literals;
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