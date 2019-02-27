
#include "app.h"
#include "lpp.h"

#include "reflection_gen.h"
#include "vex.h"

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



static void _testTemplates() {
   const char* templt = R"(
//GENERATED CODE!  DO NOT TOUCH!
//include this somewhere... anywhere you're normally doing "header implementations."


#include "reflection_gen.h"

@struct_metadata_defs{TypeMetadata* meta_@struct_name = new TypeMetadata;
}

void reflectionStartup_internal()
{
   @struct_metadata_init{
      {
         auto& structName = meta_@struct_name;
         structName.name = symbolInternString("@struct_name");
         structName.size = sizeof(@struct_name);
         structName.metaclass = ReflectionMetaClass_STRUCT;

         structName.ops.create = [](void* data) {new (data) @struct_name;};
         structName.ops.destroy = [](void* data) {((@struct_name*)data)->~@struct_name{}(); };

         @struct_metadata_member_init{
            {
               StructMemberMetadata member;
               member.name = symbolInternString("@member_name");
               member.offset = offsetof(@struct_name, @member_name);
               member.type = @member_type;
               structName.members.push_back(member);
            }
         }
      }
   }

   @initialize_file_reflection{
      //@file --------------------------------------
      void @file{}_reflectionStartup();
      @file{}_reflectionStartup();

   }
}

)";
   auto t = vexTemplateCreate(templt);

   for (int i = 0; i < 10; ++i) {
      auto structName = format("MyStruct_%d", i);

      vexTemplateBeginScope(t, "struct_metadata_defs");
      vexTemplateAddSubstitution(t, "struct_name", structName.c_str());
      vexTemplateEndScope(t);

      vexTemplateBeginScope(t, "struct_metadata_init");
      vexTemplateAddSubstitution(t, "struct_name", structName.c_str());

      for (int m = 0; m < 5; ++m) {
         auto memberName = format("MyMember_%d", i);
         vexTemplateBeginScope(t, "struct_metadata_member_init");
         vexTemplateAddSubstitution(t, "member_name", structName.c_str());
         vexTemplateEndScope(t);
      }

      vexTemplateEndScope(t);

      vexTemplateBeginScope(t, "initialize_file_reflection");
      auto fileName = format("MyFile_%d", i);
      vexTemplateAddSubstitution(t, "file", fileName.c_str());
      vexTemplateEndScope(t);
   }

   auto out = vexTemplateRender(t);
   vexTemplateDestroy(t);
}

#include "render.h"

//@reflect{
struct ReflectTest3 {
   int staticArr[100] = { 0 };
};//}

#include "main_reflection_gen.inl"

static void _testReflect() {
   typedef ReflectTest3 TestType;

   TestType test;
   for (int i = 0; i < 100; ++i) {
      test.staticArr[i] = i;
   }

   auto type = reflect<TestType>();

   auto writer = scfWriterCreate();
   serialize(writer, type, &test);

   uint64_t sz = 0;
   auto mem = scfWriteToBuffer(writer, &sz);;

   auto reader = scfView(mem);
   TestType test2;
   deserialize(reader, type, &test2);


   auto testtype = reflect<std::unordered_map<Symbol*, std::vector<std::string>>>();

}

int main(int argc, char** argv)
{
   AppConfig config;
   _parseArgs(argc, argv, config);

   if (config.reflectgen) {
      runReflectGen(config);
   }
   else {
      lppStartup();
      reflectionStartup();

      //_testTemplates();
      _testReflect();


      
      auto app = appCreate(config);
      appCreateWindow(app, WindowConfig{ 1280, 720, "Making Games is Fucking Hard" });
      while (appRunning(app)) {
         appStep(app);
      }
      appDestroy(app);
   }
   
   return 0;
}