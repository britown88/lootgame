#include "vex.h"
#include "parser.h"

#include <string.h>

static bool _vexspancmp(VexSpan const& lhs, const char* rhs, int rhsSize) {
   if (rhsSize != lhs.end - lhs.begin) {
      return false;
   }
   auto a = lhs.begin;
   auto b = rhs;
   while (a < lhs.end) {
      if (*a++ != *b++) {
         return false;
      }
   }
   return true;
}

bool VexSpan::operator==(VexSpan const& other) { return _vexspancmp(*this, other.begin, other.end - other.begin); }
bool VexSpan::operator==(std::string const& other) { return _vexspancmp(*this, other.c_str(), other.size()); }

static bool _acceptSigil(StringParser& p) {
   while (!p.atEnd()) {
      if (p.accept('@')) return true;
      p.skip();
   }
   return false;      
}

static bool _acceptTagCharacter(StringParser& p) {
   return p.acceptInRanges({ { 'a', 'z' },{ 'A', 'Z' },{ '0', '9' },{ '_' } });
}

static bool _acceptTag(StringParser& p) {
   auto start = p.pos;
   while (!p.atEnd() && _acceptTagCharacter(p));
   return p.pos != start;
}

static void _parseSpan(VexSpan span, VexNode* parent);

static void _parseNode(VexNode* node, StringParser &p) {
   node->tag.begin = p.pos;
   _acceptTag(p);
   node->tag.end = p.pos;

   bool parseBody = false;

   if (p.atEnd()) {
      node->body = { p.pos, p.pos };
   }
   else {
      if (p.accept(':')) { // body is until line ending
         node->body.begin = p.pos;
         while (!p.atEnd() && p.peek() != '\n') p.skip();
         node->body.end = p.pos;
         parseBody = true;
      }
      else if (p.accept('=')) { // body is next tag
         node->body.begin = p.pos;
         _acceptTag(p);
         node->body.end = p.pos;
      }
      else if (p.accept('{')) { // start scope
         int bracketCount = 1;
         while (p.accept('{')) { ++bracketCount; }

         node->body.begin = p.pos;

         // find end
         int scopeStack = 1;
         while (true) {
            if (p.atEnd()) {
               node->body.end = p.pos;
               break;
            }

            if (p.accept('}')) {
               int newbracketCount = 1;
               while (newbracketCount < bracketCount && p.accept('}')) { ++newbracketCount; }
               if (newbracketCount == bracketCount) {
                  --scopeStack;
               }

               if (!scopeStack) {
                  node->body.end = p.pos - bracketCount;
                  break;
               }
            }
            else if (p.accept('{')) {
               int newbracketCount = 1;
               while (newbracketCount < bracketCount && p.accept('{')) { ++newbracketCount; }
               if (newbracketCount == bracketCount) {
                  ++scopeStack;
               }               
            }
            else {
               p.skip();
            }
         }
         parseBody = true;
      }
      else if (p.accept('(')) { // everythings a node
         int bracketCount = 1;
         while (p.accept('(')) { ++bracketCount; }

         node->body.begin = p.pos;
         int scopeStack = 1;

         // find end parens
         while (true) {
            if (p.atEnd()) {
               node->body.end = p.pos;
               break;
            }  

            if (p.accept(')')) {
               int newbracketCount = 1;
               while (newbracketCount < bracketCount && p.accept(')')) { ++newbracketCount; }
               if (newbracketCount == bracketCount) {
                  --scopeStack;
               }
               if (!scopeStack) {
                  node->body.end = p.pos - bracketCount;
                  break;
               }
            } 
            else if (p.accept('(')) {
               int newbracketCount = 1;
               while (newbracketCount < bracketCount && p.accept('(')) { ++newbracketCount; }
               if (newbracketCount == bracketCount) {
                  ++scopeStack;
               }
            }
            else {
               p.skip();
            }
         }

         // now parse the inner span for nodes
         StringParser subp = { node->body.begin, node->body.end };
         VexNode* lastChild = nullptr;

         while (!subp.atEnd()) {
            VexNode child;
            child.span.begin = subp.pos;
            _parseNode(&child, subp);

            // if the parse found a valid node, alloc it and link it up
            if (child.span.end > child.span.begin) {
               auto childPtr = new VexNode(child);
               if (!lastChild) { node->children = childPtr; }
               else { lastChild->next = childPtr; }
               lastChild = childPtr;
            }

            // skip to next tag
            while (!subp.atEnd()) {                   // the sad vex mustache man
               if (_acceptTagCharacter(subp) || subp.acceptAnyOf(":={(")) {
                  subp.rewind();
                  break;
               }
               subp.skip();
            }
         }
      }
      else { // this node is done
         node->body = { p.pos, p.pos };         
      }
   }

   node->span.end = p.pos;
   if (parseBody) {
      _parseSpan(node->body, node);
   }   
}


static void _parseSpan(VexSpan span, VexNode* parent) {
   VexNode* lastChild = nullptr;

   StringParser p = { span.begin, span.end };

   while (_acceptSigil(p)) {
      auto newNode = new VexNode();

      if (!lastChild) {// first child
         parent->children = newNode;
      }
      else {
         lastChild->next = newNode;
      }
      lastChild = newNode;
      newNode->span.begin = p.pos;
      _parseNode(newNode, p);
   }
}

VexNode* vexCreate(const char* docStart, const char* docEnd) {
   auto out = new VexNode();
   out->span = out->body = { docStart, docEnd };
   out->children = out->next = nullptr;
   out->tag = { docStart, docStart };

   _parseSpan(out->body, out);

   return out;
}

VexNode* vexCreate(VexSpan const& doc) { return vexCreate(doc.begin, doc.end); }
VexNode* vexCreate(const char* doc) { return vexCreate(doc, doc + strlen(doc)); }
VexNode* vexCreate(std::string const& doc) { return vexCreate(doc.c_str(), doc.c_str() + doc.size()); }

void vexDestroy(VexNode* node) {
   while (node->children) {
      auto next = node->children->next;
      vexDestroy(node->children);
      node->children = next;
   }
   delete node;
}