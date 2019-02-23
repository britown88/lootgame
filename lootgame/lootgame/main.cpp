
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

   if (config.reflectgen) {
      runReflectGen(config);
   }
   else {
      reflectionStartup();

      typedef std::unordered_map<Symbol*, std::vector<std::string>> TestType;

      TestType test;
      for (int j = 0; j < 100; ++j) {
         std::vector<std::string> v;
         for (int i = 0; i < 100; ++i) {
            v.push_back(intern(format("Here's a number: %d", i).c_str()));
         }
         test.insert({ intern(format("%d", j).c_str()), v });
      }

      auto type = reflect<TestType>();

      auto writer = scfWriterCreate();
      serialize(writer, type, &test);

      uint64_t sz = 0;
      auto mem = scfWriteToBuffer(writer, &sz);;

      auto reader = scfView(mem);
      TestType test2;
      deserialize(&reader, type, &test2);

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