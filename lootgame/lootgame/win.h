#pragma once

//insert all windows shit here

#include <string>
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

byte *fileReadBinary(StringView path, u64 *fsize);
std::string fileReadString(StringView path);

int writeBinaryFile(StringView path, byte* buffer, u64 size);

std::string pathGetFilename(StringView path);