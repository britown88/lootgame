#include "stdafx.h"


#include "app.h"
#include "lpp.h"

#include "reflection_gen.h"
#include "vex.h"
#include "win.h"

static void _parseArgs(int argc, char** argv) {
   auto begin = argv + 1;
   auto end = argv + argc;

   for (auto arg = begin; arg < end; ++arg) {
      if (!strcmp(*arg, "-reflectgen") && ++arg < end) {
         AppConfig.reflectgen = true;
         AppConfig.reflectTarget = *arg;
      }
      else if (!strcmp(*arg, "-assetgen")) {
         AppConfig.assetgen = true;
      }
   }
}

static void _loadConfig() {
   auto ini = fileReadString("config.ini");
   if (!ini.empty()) {
      auto vex = vexCreate(ini);

      auto child = vex->children;
      while (child) {
         if (child->tag == "assetdir") {
            AppConfig.assetPath = std::string(child->body.begin, child->body.end);
         }

         child = child->next;
      }

      vexDestroy(vex);
   }
}


int main(int argc, char** argv)
{
   _parseArgs(argc, argv);

   if (AppConfig.reflectgen) {
      runReflectGen();
   }
   else if (AppConfig.assetgen) {
      _loadConfig();
      //reflectionStartup();

      runAssetGen();
   }
   else {
      lppStartup();
      _loadConfig();

      //reflectionStartup();
      
      auto app = appCreate();
      appCreateWindow(app, WindowConfig{ 1280, 720, "Making Games is Fucking Hard" });
      while (appRunning(app)) {
         appStep(app);
      }
      appDestroy(app);
   }
   
   return 0;
}