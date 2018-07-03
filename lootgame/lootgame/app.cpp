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

struct Window {
   SDL_Window* sdlWnd = nullptr;
   SDL_GLContext sdlCtx = nullptr;
   ImGuiContext* imguiContext = nullptr;

   Int2 size = { 0 }, clientSize = { 0 };
   float scale = 1.0f;
   bool shouldClose = false;

   struct Dialog {
      std::function<bool(Window*)> dlg;
      bool bringToFront = true;
   };

   std::unordered_map<std::string, Dialog> dlgs;
};

struct App {
   bool running = false;
   Window* wnd = nullptr;
   ImFontAtlas* fontAtlas;
};

App* appCreate(AppConfig const& config) {
   auto out = new App();
   return out;
}

void appDestroy(App* app) {

   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplSDL2_Shutdown();
   ImGui::DestroyContext();

   SDL_GL_DeleteContext(app->wnd->sdlCtx);
   SDL_DestroyWindow(app->wnd->sdlWnd);
   SDL_Quit();

   delete app->wnd;
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

static Window* _windowCreate(App* app, WindowConfig const& info) {
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
   {
      printf("Error: %s\n", SDL_GetError());
      return nullptr;
   }

   // Setup window
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
   SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   SDL_DisplayMode current;
   SDL_GetCurrentDisplayMode(0, &current);
   SDL_Window* window = SDL_CreateWindow("ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

   // Setup style
   ImGui::StyleColorsDark();
   //ImGui::StyleColorsClassic();

   Window* out = new Window();
   out->sdlCtx = gl_context;
   out->sdlWnd = window;
   out->imguiContext = imCtx;
   windowRefreshSize(out);

   return out;
}

void appCreateWindow(App* app, WindowConfig const& info) {
   app->wnd = _windowCreate(app, info);
   app->running = true;
}

bool appRunning(App* app) { return app->running; }
void appPollEvents(App* app) {
   SDL_Event event;
   auto &io = ImGui::GetIO();
   while (SDL_PollEvent(&event))
   {
      ImGui_ImplSDL2_ProcessEvent(&event);

      // handle non-imgui-handled mouse things here
      if (!io.WantCaptureMouse) {
         switch (event.type) {
         case SDL_MOUSEBUTTONUP: {
            break; }
         case SDL_MOUSEBUTTONDOWN: {
            break; }
         case SDL_MOUSEMOTION: {
            break; }
         case SDL_MOUSEWHEEL: {
            break; }
         }
      }

      if (!io.WantCaptureKeyboard) {
         switch (event.type) {
         case SDL_KEYDOWN: {
            break; }
         case SDL_KEYUP: {
            // switch (event.key.keysym.scancode) {
            // }
            break; }
         case SDL_TEXTEDITING: {
            break; }
         case SDL_TEXTINPUT: {
            break; }
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
            windowRefreshSize(app->wnd);
            break;
         }
         break;
      }
   }
}



static void _pollEvents(App* app) {
   appPollEvents(app);
}

static void _beginFrame(App* app) {
   ImGui_ImplOpenGL3_NewFrame();
   ImGui_ImplSDL2_NewFrame(app->wnd->sdlWnd);
   ImGui::NewFrame();
}

static void _updateGame(App* app) {
   if (app->wnd->shouldClose) {
      app->running = false;
   }
}

static void _renderFrame(App* app) {

   auto& io = ImGui::GetIO();
   ImGui::Render();
   SDL_GL_MakeCurrent(app->wnd->sdlWnd, app->wnd->sdlCtx);

   render::viewport({ 0, 0, (int)io.DisplaySize.x , (int)io.DisplaySize.y });
   render::clear(DkGray);

   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
   SDL_GL_SwapWindow(app->wnd->sdlWnd);
}

static void _updateDialogs(App* app) {
   std::vector<std::string> deleted;
   for (auto &dlg : app->wnd->dlgs) {

      if (dlg.second.bringToFront) {
         ImGui::SetNextWindowFocus();
         dlg.second.bringToFront = false;
      }

      if (!dlg.second.dlg(app->wnd)) {
         deleted.push_back(dlg.first);
      }
   }
   for (auto&d : deleted) {
      app->wnd->dlgs.erase(d);
   }
}

void appStep(App* app) {
   _pollEvents(app);
   _beginFrame(app);
   _updateGame(app);
   _updateDialogs(app);
   _renderFrame(app);
}


// Window
Int2 windowSize(Window* wnd) { return wnd->size; }
Int2 windowClientArea(Window* wnd) { return wnd->clientSize; }
float windowScale(Window* wnd) { return wnd->scale; }
void windowClose(Window*wnd) { wnd->shouldClose = true; }

void windowRefreshSize(Window* wnd) {
   SDL_GetWindowSize(wnd->sdlWnd, &wnd->size.x, &wnd->size.y);
   SDL_GL_GetDrawableSize(wnd->sdlWnd, &wnd->clientSize.x, &wnd->clientSize.y);
   wnd->scale = wnd->clientSize.x / (float)wnd->size.x;
}

void windowAddGUI(Window* wnd, StringView label, std::function<bool(Window*)> gui) {

   auto dlg = wnd->dlgs.find(label);
   if (dlg != wnd->dlgs.end()) {
      dlg->second.bringToFront = true;
      return;
   }

   wnd->dlgs.insert({ label,{ gui } });
}