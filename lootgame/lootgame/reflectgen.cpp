#include "app.h"
#include "vex.h"
#include "win.h"

#include "fsys.h"

#include "parser.h"


#include <vector>

struct ParsedEnumMember {
   std::string name;
   std::string constructor;
};

struct ParsedEnum {
   std::string name;
   std::vector<ParsedEnumMember> members;
};

struct ParsedType {
   std::string name;
   int pointers = 0;
};

struct ParsedStructMember {
   ParsedType type;
   std::string name;
   bool staticArray = false;
   std::string arraySize;

   std::string constructor;
};

struct ParsedStruct {
   std::string name;
   std::string anonMemberName;
   std::vector<ParsedStructMember> members;
   std::vector<ParsedStruct> children;
   std::vector<ParsedEnum> enums;
};

/*
singleLineComment =  '/', '/', { anything until }, '\n'
multilineComment =   '/', '*', { anything until }, '*', '/'
comment =            singleLineComment | multilineComment
whitespaceChar =     ' ' | '\t' | '\n' | '\r' )
whitespace =         whitespaceChar, { whitespaceChar }
skippable =          whitespace | comment, { whitespace | comment }
*/

static bool _acceptSingleLineComment(StringParser& p) {
   if (p.accept("//")) {
      while (!p.atEnd() && !p.accept('\n')) { p.skip(); }
      return true;
   }
   return false;
}
static bool _acceptMultiLineComment(StringParser& p) {
   if (p.accept("/*")) {
      while (!p.atEnd() && !p.accept("*/")) { p.skip(); }
      return true;
   }
   return false;
}
static bool _acceptComment(StringParser& p) {
   return _acceptSingleLineComment(p) || _acceptMultiLineComment(p);
}
static bool _acceptWhitespaceChar(StringParser& p) {
   return p.acceptAnyOf(" \t\n\r");
}
static bool _acceptWhitespace(StringParser& p) {
   if (_acceptWhitespaceChar(p)) {
      while (!p.atEnd() && _acceptWhitespaceChar(p));
      return true;
   }
   return false;
}
static bool _acceptSkippable(StringParser& p) {
   if (_acceptWhitespace(p) || _acceptComment(p)) {
      while (_acceptWhitespace(p) || _acceptComment(p));
      return true;
   }
   return false;
}

static bool _acceptIdStartChar(StringParser& p) {
   return p.acceptInRanges({ {'a', 'z'}, {'A', 'Z'}, {'_'} });
}
static bool _acceptIdChar(StringParser& p) {
   return _acceptIdStartChar(p) || p.acceptInRanges({ { '0', '9' } });
}
static bool _acceptIdentifier(StringParser& p) {
   if (_acceptIdStartChar(p)) {
      while (!p.atEnd() && _acceptIdChar(p));
      while (!p.atEnd() && p.accept("::") && _acceptIdentifier(p));
      return true;
   }
   return false;
}

static bool _acceptEscapeSequence(StringParser& p) {
   return p.accept('\\') && p.acceptAnyOf("abfnrtv\\'\"?");
}
static bool _acceptStringLiteral(StringParser& p) {
   if (p.accept('\"')) {
      while (!p.atEnd()) {
         if (!_acceptEscapeSequence(p)) {
            if (p.accept('\"'))
               return true;
            p.skip();
         }
      }
   }
   return false;
}

static void _skipToExpressionEnd(StringParser& p) {
   while (!p.atEnd()) {
      if (!_acceptStringLiteral(p)) {
         if (p.accept(';')) {
            return;
         }
         p.skip();
      }
   }
}

static bool _acceptPrivacy(StringParser& p) {
   if (p.accept("public") || p.accept("private") || p.accept("protected")) {
      _acceptSkippable(p);
      p.accept(':');
      return true;
   }
   return false;
}


static bool _acceptTemplateArguments(StringParser& p) {
   if (p.accept('<')) {
      int stack = 1;
      while (!p.atEnd()) {
         if (p.accept('<')) {
            ++stack;
         }
         else if (p.accept('>')) {
            if (--stack == 0) {
               return true;
            }
         }
         else {
            p.skip();
         }
      }
   }
   return false;
}


// skips member lines that begin wtih static, virtual, typedef, friend, or template
static bool _acceptSkippableExpression(StringParser& p) {
   if (p.accept("static") || p.accept("virtual") || p.accept("typedef") || p.accept("friend")) {
      _skipToExpressionEnd(p);
      return true;
   }
   else if (p.accept("template")) {
      _acceptSkippable(p);
      _acceptTemplateArguments(p);
      return true;
   }
   return false;
}


static bool _acceptType(StringParser& p, ParsedType& output) {   
   auto start = p.pos;
   if (p.accept("const")) {
      _acceptSkippable(p);
   }

   const char* typeStart = p.pos;
   if (_acceptIdentifier(p)) {
      const char* typeEnd = p.pos;
      _acceptSkippable(p);

      if (_acceptTemplateArguments(p)) {
         typeEnd = p.pos;
      }
      
      while (!p.atEnd()){
         if (p.accept('*')) {
            ++output.pointers;
            _acceptSkippable(p);
         }
         else if (p.accept('&') || p.accept("const")) {
            _acceptSkippable(p);
         }
         else {
            break;
         }
      }
      output.name.assign(typeStart, typeEnd);

      for (int i = 0; i < output.pointers; ++i) {
         output.name.append(1, '*');
      }

      return true;
   }

   // rewind here because we may have eaten the const
   p.pos = start;
   return false;
}

static bool _acceptArrayDefinition(StringParser& p, std::string& size) {
   if (p.accept('[')) {
      _acceptSkippable(p);

      auto szStart = p.pos;
      const char* szEnd = nullptr;

      while (!p.atEnd()) {
         auto current = p.pos;
         if (_acceptSkippable(p)) {
            szEnd = current;
            continue;
         }
         else if (p.accept(']')) {
            if (!szEnd) { szEnd = p.pos - 1; }
            size.assign(szStart, szEnd);
            return true;
         }
         else {
            p.skip();
         }
      }
   }
   return false;
}

static bool _acceptConstructor(StringParser& p, std::string& output, char endChar = ';') {
   auto start = p.pos;
   if (p.accept('=')) {
      _acceptSkippable(p);

      auto outStart = p.pos;
      const char* szEnd = nullptr;

      while (!p.atEnd()) {
         if (!_acceptStringLiteral(p)) {
            if (p.accept(endChar)) {
               output.assign(outStart, p.pos - 1);
               return true;
            }
            p.skip();
         }
      }
   }
   p.pos = start;
   return false;
}

// rewinds to start if failed
static bool _acceptMember(StringParser& p, ParsedStructMember &output) {
   auto start = p.pos;
   if (_acceptType(p, output.type)) {

      _acceptSkippable(p);

      const char* nameStart = p.pos;
      if (_acceptIdentifier(p)) {
         auto nameEnd = p.pos;

         _acceptSkippable(p);
         if (p.accept('(')) {
            // function, not member
            p.pos = start;
            return false;
         }

         output.name.assign(nameStart, nameEnd);

         if (_acceptArrayDefinition(p, output.arraySize)) {
            output.staticArray = true;
         }

         _acceptSkippable(p);
         
         if (!_acceptConstructor(p, output.constructor)) {
            _skipToExpressionEnd(p);
         }

         return true;
      }
   }

   p.pos = start;
   return false;
}

static bool _acceptEnum(StringParser& p, ParsedEnum& output);

static bool _acceptStruct(StringParser& p, ParsedStruct &output) {
   auto start = p.pos;
   if (p.accept("const")) {
      _acceptSkippable(p);
   }

   if (p.accept("struct")) {
      _acceptSkippable(p);

      auto idStart = p.pos;
      _acceptIdentifier(p);
      output.name.assign(idStart, p.pos);

      _acceptSkippable(p);
      if (p.accept('{')) {
         _acceptSkippable(p);

         while (!p.atEnd()) {
            if (!_acceptSkippableExpression(p) && !_acceptPrivacy(p)) {
               ParsedStructMember newMember;
               ParsedStruct child;
               ParsedEnum newEnum;

               if (_acceptEnum(p, newEnum)) {
                  output.enums.push_back(newEnum);
               }
               if (_acceptStruct(p, child)) {
                  output.children.push_back(child);
               }
               else if (_acceptMember(p, newMember)) {
                  output.members.push_back(newMember);
               }
               else {
                  _acceptSkippable(p);
                  if (p.accept('}')) {
                     _acceptSkippable(p);
                     auto anonStart = p.pos;
                     _acceptIdentifier(p);
                     output.anonMemberName.assign(anonStart, p.pos);
                     _skipToExpressionEnd(p);
                     return true;
                  }
                  else {
                     _skipToExpressionEnd(p);
                  }
               }
            }

            _acceptSkippable(p);
         }
      }

   }

   p.pos = start;
   return false;
}
static bool _acceptEnumMember(StringParser& p, ParsedEnumMember& output, bool&finalMember) {
   auto idStart = p.pos;
   if (_acceptIdentifier(p)) {
      output.name.assign(idStart, p.pos);
      _acceptSkippable(p);
      if (_acceptConstructor(p, output.constructor, ',') || p.accept(',')) {
         return true;
      }
      else if (_acceptConstructor(p, output.constructor, '}') || p.accept('}')) {
         finalMember = true;
         return true;
      } 
   }
   return false;
}


bool _acceptEnum(StringParser& p, ParsedEnum& output) {
   auto start = p.pos;
   if (p.accept("typedef")) {
      _acceptSkippable(p);
   }

   if (p.accept("enum")) {
      _acceptSkippable(p);
      auto idStart = p.pos;
      if (_acceptIdentifier(p)) {
         output.name.assign(idStart, p.pos);
      }
      _acceptSkippable(p);
      if (p.accept('{')) {
         _acceptSkippable(p);
         while (!p.atEnd()) {
            ParsedEnumMember newMember;
            bool finalmember = false;
            if (_acceptEnumMember(p, newMember, finalmember)) {
               output.members.push_back(newMember);
               if (finalmember) {
                  _acceptSkippable(p);;
                  auto idStart = p.pos;
                  if (_acceptIdentifier(p)) {
                     output.name.assign(idStart, p.pos);
                  }
                  _acceptSkippable(p);
                  p.accept(';');
                  return true;
               }
            }
            else if (p.accept('}')) {
               _acceptSkippable(p);
               auto idStart = p.pos;
               if (_acceptIdentifier(p)) {
                  output.name.assign(idStart, p.pos);
               }
               _acceptSkippable(p);
               p.accept(';');
               return true;
            }
            _acceptSkippable(p);
         }
      }
   }

   p.pos = start;
   return false;
}
enum FileType {
   FileType_Header = 0,
   FileType_Cpp
};

struct ParsedFile {
   fsys::path path;
   FileType type;

   std::vector<ParsedStruct> structs;
   std::vector<ParsedEnum> enums;

   operator bool() { return !structs.empty() || !enums.empty(); }
};


static bool _parseNode(VexNode* node, ParsedFile& targetFile) {
   StringParser p = { node->body.begin, node->body.end };

   _acceptSkippable(p);
   ParsedStruct newStruct;
   ParsedEnum newEnum;

   if (_acceptStruct(p, newStruct)) {
      targetFile.structs.push_back(newStruct);
      return true;
   }
   else if (_acceptEnum(p, newEnum)) {
      targetFile.enums.push_back(newEnum);
      return true;
   }

   return false;;
}

static ParsedFile _parseFile(fsys::path path) {
   ParsedFile file;
   auto ext = path.extension();

   bool valid = false;


   if (ext == ".h" || ext == ".hpp") {
      file.type = FileType_Header;
      valid = true;
   }
   else if (ext == ".c" || ext == ".cpp") {
      file.type = FileType_Cpp;
      valid = true;
   }

   if(valid){
      file.path = path;

      auto content = fileReadString(path.string().c_str());
      auto vex = vexCreate(content);

      auto vexIter = vex->children;
      while (vexIter) {
         if (vexIter->tag == "reflect") {
            _parseNode(vexIter, file);
         }
         vexIter = vexIter->next;
      }

      vexDestroy(vex);
   }

   return file;
}

static void _generateMainHeader(std::vector<ParsedFile> &files) {
   auto templt = fileReadString("reflection_gen.h.temp");
   auto t = vexTemplateCreate(templt.c_str());

   for (auto&& file : files) {

      if (file.type == FileType_Header) {
         for (auto&& s : file.structs) {
            vexTemplateBeginScope(t, "struct_metadata");
            vexTemplateAddSubstitution(t, "struct_name", s.name.c_str());
            vexTemplateEndScope(t);
         }

         for (auto&& e : file.enums) {
            vexTemplateBeginScope(t, "enum_metadata");
            vexTemplateAddSubstitution(t, "enum_name", e.name.c_str());
            vexTemplateEndScope(t);
         }
      }
   }

   auto output = vexTemplateRender(t);
   vexTemplateDestroy(t);

   writeStringFile("reflection_gen.h", output.c_str());
   printf("Generated reflection_gen.h\n");
}

static void _generateMainHeaderImpl(std::vector<ParsedFile> &files) {
   auto templt = fileReadString("reflection_gen.temp");
   auto t = vexTemplateCreate(templt.c_str());

   for (auto&& file : files) {

      if (file.type == FileType_Header) {
         for (auto&& e : file.enums) {
            vexTemplateBeginScope(t, "struct_metadata");
            vexTemplateAddSubstitution(t, "struct_name", e.name.c_str());
            vexTemplateEndScope(t);

            vexTemplateBeginScope(t, "enum_metadata_entry_init");
            vexTemplateAddSubstitution(t, "enum_name", e.name.c_str());

            for (auto && m : e.members) {
               vexTemplateBeginScope(t, "enum_metadata_entry_init");
               vexTemplateAddSubstitution(t, "entry_name", m.name.c_str());
               vexTemplateEndScope(t);
            }

            vexTemplateEndScope(t);

            
         }

         for (auto&& s : file.structs) {
            vexTemplateBeginScope(t, "struct_metadata");
            vexTemplateAddSubstitution(t, "struct_name", s.name.c_str());
            vexTemplateEndScope(t);

            vexTemplateBeginScope(t, "struct_metadata_init");
            vexTemplateAddSubstitution(t, "struct_name", s.name.c_str());

            for (auto && m : s.members) {
               vexTemplateBeginScope(t, "struct_metadata_member_init");
               vexTemplateAddSubstitution(t, "member_name", m.name.c_str());
               vexTemplateAddSubstitution(t, "member_type", m.type.name.c_str());
               vexTemplateEndScope(t);
            }

            vexTemplateEndScope(t);
         }
         
      }
      else {
         vexTemplateBeginScope(t, "initialize_file_reflection");
         vexTemplateAddSubstitution(t, "file", file.path.stem().string().c_str());
         vexTemplateEndScope(t);
      }
   }

   auto output = vexTemplateRender(t);
   vexTemplateDestroy(t);

   writeStringFile("reflection_gen.inl", output.c_str());
   printf("Generated reflection_gen.inl\n");
}

static void _generateFileInline(ParsedFile &file) {
   auto templt = fileReadString("reflection_gen_file.temp");
   auto t = vexTemplateCreate(templt.c_str());

   std::string fname = file.path.stem().string().c_str();

   vexTemplateAddSubstitution(t, "file", fname.c_str());

   for (auto&& e : file.enums) {
      vexTemplateBeginScope(t, "struct_metadata");
      vexTemplateAddSubstitution(t, "struct_name", e.name.c_str());
      vexTemplateEndScope(t);

      vexTemplateBeginScope(t, "enum_metadata_entry_init");
      vexTemplateAddSubstitution(t, "enum_name", e.name.c_str());

      for (auto && m : e.members) {
         vexTemplateBeginScope(t, "enum_metadata_entry_init");
         vexTemplateAddSubstitution(t, "entry_name", m.name.c_str());
         vexTemplateEndScope(t);
      }

      vexTemplateEndScope(t);


   }

   for (auto&& s : file.structs) {
      vexTemplateBeginScope(t, "struct_metadata");
      vexTemplateAddSubstitution(t, "struct_name", s.name.c_str());
      vexTemplateEndScope(t);

      vexTemplateBeginScope(t, "struct_metadata_init");
      vexTemplateAddSubstitution(t, "struct_name", s.name.c_str());

      for (auto && m : s.members) {
         vexTemplateBeginScope(t, "struct_metadata_member_init");
         vexTemplateAddSubstitution(t, "member_name", m.name.c_str());
         vexTemplateAddSubstitution(t, "member_type", m.type.name.c_str());
         vexTemplateEndScope(t);
      }

      vexTemplateEndScope(t);
   }


   auto output = vexTemplateRender(t);
   vexTemplateDestroy(t);

   auto outFile = format("%s_reflection_gen.inl", fname.c_str());

   writeStringFile(outFile.c_str(), output.c_str());
   printf("Generated %s\n", outFile.c_str());

}


void runReflectGen(AppConfig const& config) {
   std::vector<ParsedFile> files;

   forEachFileInFolder(config.reflectTarget, [&](fsys::path const& path) {
      if (auto file = _parseFile(path)) {
         files.push_back(file);
      }
   });

   _generateMainHeader(files);
   _generateMainHeaderImpl(files);

   for (auto&& file : files) {
      if (file.type == FileType_Cpp) {
         _generateFileInline(file);
      }
   }

   return;
}