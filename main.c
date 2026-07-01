#include "lib.h"

[[noreturn]] int main(int argc, char **argv) {
  WRITE_LITERAL(STDERR_FILENO, "Hello world!!\n");
  exit(1);
}
