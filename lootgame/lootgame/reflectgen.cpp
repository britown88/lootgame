#include "app.h"
#include "vex.h"
#include "win.h"

#include "fsys.h"

#include "parser.h"


#include <vector>

struct StructMember {
   std::string type;
   std::string name;
   bool staticArray = false;
   std::string arraySize;

   std::string constructor;
};

struct StructContents {
   std::string name;
   std::string anonMemberName;
   std::vector<StructMember> members;
   std::vector<StructContents> children;
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

static bool _acceptType(StringParser& p, std::string& output) {
   const char* typeStart = p.pos;

   if (_acceptIdentifier(p)) {
      const char* typeEnd = p.pos;
      _acceptSkippable(p);
      
      while (!p.atEnd() && (p.accept('*') || p.accept('&') || p.accept("const") ||
                            _acceptTemplateArguments(p))) {
         typeEnd = p.pos;
         _acceptSkippable(p);       
      }
      output.assign(typeStart, typeEnd);
      return true;
   }

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

static bool _acceptConstructor(StringParser& p, std::string& output) {
   if (p.accept('=')) {
      _acceptSkippable(p);

      auto outStart = p.pos;
      const char* szEnd = nullptr;

      while (!p.atEnd()) {
         if (!_acceptStringLiteral(p)) {
            if (p.accept(';')) {
               output.assign(outStart, p.pos - 1);
               return true;
            }
            p.skip();
         }
      }
   }
   return false;
}

// rewinds to start if failed
static bool _acceptMember(StringParser& p, StructMember &output) {
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

static bool _acceptStruct(StringParser& p, StructContents &output) {
   if (p.accept("struct")) {
      _acceptSkippable(p);

      auto idStart = p.pos;
      _acceptIdentifier(p);
      output.name.assign(idStart, p.pos);

      _acceptSkippable(p);
      if (p.accept('{')) {
         _acceptSkippable(p);

         while (!p.atEnd()) {
            StructMember newMember;
            StructContents child;
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

            _acceptSkippable(p);
         }
      }

   }

   return false;
}


void reflectNode(VexNode* node) {
   StringParser p = { node->body.begin, node->body.end };

   _acceptSkippable(p);
   StructContents newStruct;

   _acceptStruct(p, newStruct);

   return;
}


void runReflectGen(AppConfig const& config) {
   forEachFileInFolder(config.reflectTarget, [](fsys::path const& path) {
      auto ext = path.extension();

      if (ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cpp") {
         auto content = fileReadString(path.string().c_str());
         auto vex = vexCreate(content);

         auto vexIter = vex->children;
         while (vexIter) {
            if (vexIter->tag == "reflect") {
               reflectNode(vexIter);
            }
            vexIter = vexIter->next;
         }

         vexDestroy(vex);
      }
   });
}