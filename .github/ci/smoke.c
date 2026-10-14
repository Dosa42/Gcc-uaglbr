#include <stdlib.h>

static int square(int value) { return value * value; }

int main(void)
{
  int total = 0;
  for (int i = 1; i <= 4; ++i)
    total += square(i);
  return total == 30 ? EXIT_SUCCESS : EXIT_FAILURE;
}
