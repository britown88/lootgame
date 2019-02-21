
#include "app.h"
#include "lpp.h"

#include "reflection.h"

static void _parseArgs(int argc, char** argv, AppConfig &config) {
   auto begin = argv + 1;
   auto end = argv + argc;

   for (auto arg = begin; arg < end; ++arg) {
      if (!strcmp(*arg, "-assets") && ++arg < end) {
         config.assetFolder = *arg;
      }
      else if (!strcmp(*arg, "-reflectgen") && ++arg < end) {
         config.reflectgen = true;
         config.reflectTarget = *arg;
      }
   }
}

int main(int argc, char** argv)
{
   lppStartup();

   AppConfig config;
   _parseArgs(argc, argv, config);
/*
   if (config.reflectgen) {
      runReflectGen(config);
   }
   else */{
      reflectionStartup();

      auto testtype = reflect<std::unordered_map<Symbol*, std::vector<std::string>>>();

      auto app = appCreate(config);
      appCreateWindow(app, WindowConfig{ 1280, 720, "Making Games is Fucking Hard" });
      while (appRunning(app)) {
         appStep(app);
      }
      appDestroy(app);
   }
   
   return 0;
}