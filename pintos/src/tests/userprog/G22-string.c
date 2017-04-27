/*passes a string that continues into kernel memory*/

#include "tests/lib.h"
#include "tests/main.h"
#include <syscall.h>
#include <syscall-nr.h>

void
test_main (void)
{
	char *bad_str = (char *)(long) 0xbfffffff;
	*bad_str = 'a';
	open(bad_str);
	fail("should have exited with -1");
}
	