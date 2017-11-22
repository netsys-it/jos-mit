// test user-level fault handler -- just exit when we fault

#include <inc/lib.h>

void
handler(struct UTrapframe *utf)
{
	cprintf("handler\n");
	void *addr = (void*)utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	cprintf("i faulted at va %x, err %x\n", addr, err & 7);
	sys_env_destroy(sys_getenvid());
}

void
umain(int argc, char **argv)
{
	set_pgfault_handler(handler);
	cprintf("after set_pgfault_handler: all good\n");
	*(int*)0xDeadBeef = 0;
	cprintf("*(int*)0xDeadBeef = 0;: all good\n");
}
