#include <stdlib.h>

int main(int argc, char** argv) {
  int a = atoi(argv[1]);
  switch (a) {
    case 0:
      return 1;
    case 1:
      return 2;
    case 2:
      return 3;
    default:
      return 10;
  }
}