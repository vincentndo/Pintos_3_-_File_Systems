/* tries to write from a buffer that crosses into kernel memory*/ 

#include "tests/lib.h"
#include "tests/main.h"
#include <syscall.h>
#include <syscall-nr.h>

void
test_main (void)
{
	create ("testfile", 12*sizeof(char));
	int fd = open ("testfile");
	void *bad_buff = (void *) (long) 0xbffffffc; // 1 word below PHYS_BASE
	write (fd, bad_buff, 8);
	fail ("should have exited due to bad buffer");
}
	