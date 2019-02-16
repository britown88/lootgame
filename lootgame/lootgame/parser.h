#pragma once

#include "defs.h"
#include <initializer_list>
#include <assert.h>

struct StringParser {
   const char *pos, *end;

   char peek() { return *pos; }
   bool atEnd() { return pos >= end; }
   void skip(int skip = 1) { pos += skip; }
   void rewind(int skip = 1) { pos -= skip; }

   bool accept(char c) {
      if (*pos == c) {
         ++pos;
         return true;
      }
      return false;
   }
   bool accept(const char* str) {
      auto start = pos;
      while (*str) {
         if (!accept(*str)) {
            pos = start;
            return false;
         }
         ++str;
      }
      return true;
   }
   bool acceptAnyOf(const char *str, char *cout = nullptr) {
      while (*str) {
         if (accept(*str)) {
            if (cout) {
               *cout = *str;
            }
            return true;
         }
         ++str;
      }
      return false;
   }
   bool acceptInRanges(std::initializer_list<std::initializer_list<char>> ranges, char *cout = nullptr) {
      auto r = ranges.begin();
      for (auto&&range : ranges) {
         if (range.size() == 1) {
            if (*pos == *range.begin()) {
               if (cout) { *cout = *pos; }
               ++pos;
               return true;
            }
         }
         else if (range.size() == 2) {
            if (*pos >= *range.begin() && *pos <= *(range.begin() + 1)) {
               if (cout) { *cout = *pos; }
               ++pos;
               return true;
            }
         }
      }

      return false;
   }
};
