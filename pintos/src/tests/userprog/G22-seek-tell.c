/* G22 - Test seek, tell. */

#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  size_t handle, pos, byte_cnt;
  char sample[12] = "hello world\0";
  size_t size = (int) sizeof(sample);
  
  CHECK (create ("G22_file.txt", size), "create G22_file.txt");
  CHECK ((handle = open ("G22_file.txt")) > 1, "open G22_file.txt");
  CHECK ((byte_cnt = (size_t) write (handle, sample, size)) == size, "write G22_file.txt");
  CHECK ((pos = (size_t) tell(handle)) == size, "end of file");
  msg("file pointer at position %d", pos);
  msg("rewind file pointer");
  seek(handle, 0);
  pos = tell(handle);
  msg("file pointer at position %d", pos);
  CHECK (remove ("G22_file.txt"), "remove G22_file.txt");
}
