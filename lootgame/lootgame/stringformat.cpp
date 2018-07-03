#include <string>
#include <stdarg.h>

#include "defs.h"

#include <stb/stb_sprintf.h>

std::string format(StringView fmt, ...) {
   int size = ((int)strlen(fmt)) * 2 + 50;   // Use a rubric appropriate for your code
   std::string str;
   va_list ap;
   while (1) {     // Maximum two passes on a POSIX system...
      str.resize(size);
      va_start(ap, fmt);
      int n = vsnprintf((char *)str.data(), size, fmt, ap);
      va_end(ap);
      if (n > -1 && n < size) {  // Everything worked
         str.resize(n);
         return str;
      }
      if (n > -1)  // Needed size returned
         size = n + 1;   // For null char
      else
         size *= 2;      // Guess at a larger size (OS specific)
   }
   return str;
}