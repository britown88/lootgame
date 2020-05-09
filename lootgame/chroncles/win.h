#pragma once

//insert all windows shit here


#include "defs.h"

struct OpenFileConfig {
   std::string filterNames;
   std::string filterExtensions;
   int filterIndex = 0;
   std::string initialDir;
   std::string title;
};

std::string openFile(OpenFileConfig const& config);

std::string cwd();

byte *fileReadBinary(StringView path, uint64_t *fsize);
std::string fileReadString(StringView path);

int writeBinaryFile(StringView path, byte* buffer, uint64_t size);
int writeStringFile(StringView path, const char* buffer);

std::string pathGetFilename(StringView path);