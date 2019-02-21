#include "defs.h"

#include <unordered_set>
#include <cstring>

struct StringViewEqual {
   bool operator()(const StringView &lhs, const StringView &rhs) const {
      return strcmp(lhs, rhs) == 0;
   }
};

struct StringViewHash {
   size_t operator()(const StringView &str) const {
      size_t out = 5381;
      auto c = str;
      while (*c) {
         out = (out << 5) + (out << 1) + *c++;
      }
      return out;
   }
};

Symbol* intern(StringView str) {
   static std::unordered_set < StringView, StringViewHash, StringViewEqual > table;
   auto search = table.find(str);
   if (search != table.end()) {
      return *search;
   }

   auto allocd = _strdup(str);
   table.insert(allocd);
   return allocd;
}