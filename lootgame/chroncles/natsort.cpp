
#include "stdafx.h"
#include "defs.h"

int count_digits(const char* s) {
   const char * p = s;
   while (isdigit(*p))
      p++;
   return (int)(p - s);
}

int natstrcmp(const char* a_ptr, const char* b_ptr) {
   const char *a = a_ptr;
   const char *b = b_ptr;

   for (;;) {
      auto ac = *a;
      auto bc = *b;
      if (ac >= 'A' && ac <= 'Z') { ac -= 'A' - 'a'; }
      if (bc >= 'A' && bc <= 'Z') { bc -= 'A' - 'a'; }

      if (isdigit(*a) && isdigit(*b)) {
         int a_count = count_digits(a);
         int diff = a_count - count_digits(b);
         if (diff)
            return diff;
         diff = memcmp(a, b, a_count);
         if (diff)
            return diff;
         a += a_count;
         b += a_count;
      }
      if (ac != bc)
         return ac - bc;
      if (ac == '\0')
         return 0;
      a++,  b++;
   }
}