#include <stdint.h>

/* Exported to the main core as ulp_counter. */
uint32_t counter = 0;

/* Runs once per wakeup period; returning halts the coprocessor until the
 * timer starts it again. */
int main(void) {
  counter++;
  return 0;
}
