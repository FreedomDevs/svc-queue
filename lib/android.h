#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static inline char *itoa(int val) {
  static char buf[24];
  int i = 22;
  int is_neg = val < 0;

  buf[23] = '\0';
  if (val == 0) {
    buf[i--] = '0';
  } else {
    unsigned int uval = is_neg ? -val : val;
    while (uval > 0) {
      buf[i--] = (uval % 10) + '0';
      uval /= 10;
    }
    if (is_neg) {
      buf[i--] = '-';
    }
  }
  return &buf[i + 1];
}
