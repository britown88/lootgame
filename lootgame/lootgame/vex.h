#pragma once


struct VexSpan {
   const char* begin = nullptr;
   const char* end = nullptr; 

   bool operator==(VexSpan const& other);
   bool operator==(std::string const& other);
   bool operator==(const char* other);
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


typedef struct VexTemplate VexTemplate;

VexTemplate* vexTemplateCreate(const char* tmplt);
void vexTemplateDestroy(VexTemplate* self);

void vexTemplateAddSubstitution(VexTemplate* self, const char* tag, const char* substitution);
void vexTemplateBeginScope(VexTemplate* self, const char* tag);
void vexTemplateEndScope(VexTemplate* self);

std::string vexTemplateRender(VexTemplate* self);




