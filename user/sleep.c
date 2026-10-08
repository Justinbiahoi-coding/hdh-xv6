#include "kernel/types.h"
#include "user/user.h"

// Parse a non-negative decimal integer.  Returns -1 on anything else:
// empty string, a non-digit character, or a value past INT_MAX.
// atoi() alone cannot report these -- it returns 0 for "abc".
static int
parseticks(const char *s)
{
  int n = 0;

  if(*s == '\0')
    return -1;
  for(; *s; s++){
    if(*s < '0' || *s > '9')
      return -1;
    if(n > (0x7fffffff - (*s - '0')) / 10)  // would overflow
      return -1;
    n = n * 10 + (*s - '0');
  }
  return n;
}

int
main(int argc, char *argv[])
{
  int ticks;

  if(argc != 2){
    fprintf(2, "usage: sleep ticks\n");
    exit(1);
  }

  if((ticks = parseticks(argv[1])) < 0){
    fprintf(2, "sleep: ticks phai la so nguyen khong am\n");
    exit(1);
  }

  if(sleep(ticks) < 0){
    fprintf(2, "sleep: sleep failed\n");
    exit(1);
  }

  exit(0);
}
