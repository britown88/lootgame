#pragma once
#include <string>

struct VexSpan {
   const char* begin = nullptr;
   const char* end = nullptr; 

   bool operator==(VexSpan const& other);
   bool operator==(std::string const& other);
};


struct VexNode {
   VexSpan span, tag, body;
   VexNode* children = nullptr, *next = nullptr;
};

VexNode* vexCreate(const char* docStart, const char* docEnd);
VexNode* vexCreate(VexSpan const& doc);
VexNode* vexCreate(const char* doc);
VexNode* vexCreate(std::string const& doc);

void vexDestroy(VexNode* node);
