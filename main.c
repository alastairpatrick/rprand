#include <stdio.h>

#include "pico/stdio.h"

#include "rprand.h"

int main() {
  stdio_init_all();
  //rprand_maximize_rosc();
  rprand_init(0);

  for (;;) {
    int r = rprand_get_32();
    printf("Random %d\n", r);
  }
}