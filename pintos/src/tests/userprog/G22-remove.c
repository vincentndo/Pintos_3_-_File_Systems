/* G22 - Test removing a file in various scenarios. */

#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  CHECK (create ("G22_file_1.txt", 0), "create G22_file_1.txt");
  CHECK (remove ("G22_file_1.txt"), "remove G22_file_1.txt");

  int handle, byte_cnt;

  CHECK (create ("G22_file_2.txt", sizeof sample - 1), "create G22_file_2.txt");
  CHECK ((handle = open ("G22_file_2.txt")) > 1, "open G22_file_2.txt");
  CHECK (remove ("G22_file_2.txt"), "remove G22_file_2.txt");
  CHECK ((byte_cnt = write (handle, sample, sizeof sample - 1)) == sizeof sample - 1, "still write successfully after remove");
  if (byte_cnt != sizeof sample - 1)
    fail ("write() returned %d instead of %zu", byte_cnt, sizeof sample - 1);
}
