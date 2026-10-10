#include <stdint.h>

#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"

/* Exported to the main core as ulp_counter. */
uint32_t counter = 0;

int main(void) {
  counter++;
  /* ulp_riscv_halt() is called when main returns; the timer restarts us. */
  return 0;
}
