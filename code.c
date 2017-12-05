LAB 4
trivial trap_dispatch()
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi();
		sched_yield();
		return;
	}
 
kern/trap.c
        // LAB 4: Your code here.
        if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
                lapic_eoi();
                sched_yield();
                return;
        }
        // Handle keyboard and serial interrupts.
        // LAB 5: Your code here.
        if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
                kbd_intr();
                return;
        }
        if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
                serial_intr();
                return;
        }
void
trap_init_percpu(void)
{
        size_t i = cpunum();
        // Setup a TSS so that we get the right stack
        // when we trap to the kernel.
        thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
        thiscpu->cpu_ts.ts_ss0 = GD_KD;
        thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

        // Initialize the TSS slot of the GDT.
        gdt[(GD_TSS0 >> 3) + i] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
                                        sizeof(struct Taskstate) - 1, 0);
        gdt[(GD_TSS0 >> 3) + i].sd_s = 0;

        // Load the TSS selector (like other segment selectors, the
        // bottom three bits are special; we leave them 0)
        ltr(GD_TSS0 + i * sizeof(struct Segdesc));

        lidt(&idt_pd);
}
void
trap_init(void)
{
        extern struct Segdesc gdt[];

        // LAB 3: Your code here.
        extern void (*th[])();
        for (int i = 0; i <= 16; ++i)
                if (i==T_BRKPT)
                        SETGATE(idt[i], 0, GD_KT, th[i], 3)
                else if (i!=2 && i!=15) {
                        SETGATE(idt[i], 0, GD_KT, th[i], 0);
                }
        SETGATE(idt[48], 0, GD_KT, th[48], 3);

        for (int i = 0; i < 16; ++i)
                SETGATE(idt[IRQ_OFFSET+i], 0, GD_KT, th[IRQ_OFFSET+i], 0);

        // Per-CPU setup 
        trap_init_percpu();
}
ker/trapenry.S
th:
.text
/*
 * Challenge: my code here
 */
        noec(th0, 0)
        noec(th1, 1)
        zhanwei()
        noec(th3, 3)
        noec(th4, 4)
        noec(th5, 5)
        noec(th6, 6)
        noec(th7, 7)
        ec(th8, 8)
        noec(th9, 9)
        ec(th10, 10)
        ec(th11, 11)
        ec(th12, 12)
        ec(th13, 13)
        ec(th14, 14)
        zhanwei()
        noec(th16, 16)
.data
        .space 60
.text
        noec(th32, 32)
        noec(th33, 33)
        noec(th34, 34)
        noec(th35, 35)
        noec(th36, 36)
        noec(th37, 37)
        noec(th38, 38)
        noec(th39, 39)
        noec(th40, 40)
        noec(th41, 41)
        noec(th42, 42)
        noec(th43, 43)
        noec(th44, 44)
        noec(th45, 45)
        noec(th46, 46)
        noec(th47, 47)
        noec(th48, 48)
lib/fork.c
static void
pgfault(struct UTrapframe *utf)
{
        void *addr = (void *) utf->utf_fault_va;
        uint32_t err = utf->utf_err;
        int r;

        // Check that the faulting access was (1) a write, and (2) to a
        // copy-on-write page.  If not, panic.
        // Hint:
        //   Use the read-only page table mappings at uvpt
        //   (see <inc/memlayout.h>).

        // LAB 4: Your code here.
        if (!((err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) && 
                (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW)))
                panic("pgfault: not copy-on-write");
        // Allocate a new page, map it at a temporary location (PFTEMP),
        // copy the data from the old page to the new page, then move the new
        // page to the old page's address.
        // Hint:
        //   You should make three system calls.

        // LAB 4: Your code here.
        addr = ROUNDDOWN(addr, PGSIZE);
        if (sys_page_alloc(0, PFTEMP, PTE_W|PTE_U|PTE_P) < 0)
                panic("pgfault: sys_page_alloc");
        memcpy(PFTEMP, addr, PGSIZE);
        if (sys_page_map(0, PFTEMP, 0, addr, PTE_W|PTE_U|PTE_P) < 0)
                panic("pgfault: sys_page_map");
        if (sys_page_unmap(0, PFTEMP) < 0)
                panic("pgfault: sys_page_unmap");
        return;
}               
static int
duppage(envid_t envid, unsigned pn)
{
        // LAB 4: Your code here.
        int r;

        void *addr;
        pte_t pte;
        int perm;

        addr = (void *)((uint32_t)pn * PGSIZE);
        pte = uvpt[pn];
        if (pte & PTE_SHARE) {
                //sys_getenvid()
            if ((r = sys_page_map(sys_getenvid(), addr, envid, addr, pte & PTE_SYSCALL)) < 0) {

                        panic("duppage: page mapping failed %e", r);
                        return r;
                }
        }
        else {
                perm = PTE_P | PTE_U;
                if ((pte & PTE_W) || (pte & PTE_COW))
                        perm |= PTE_COW;
                if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, perm)) < 0) {
                        panic("duppage: page remapping failed %e", r);
                        return r;
                }
                if (perm & PTE_COW) {
                        if ((r = sys_page_map(thisenv->env_id, addr, thisenv->env_id, addr, perm)) < 0) {
                                panic("duppage: page remapping failed %e", r);
                                return r;
                                        }
                }
        }

        return 0;

}
envid_t
fork(void)
{
        // LAB 4: Your code here.
        set_pgfault_handler(pgfault);

        envid_t envid;
        uint32_t addr;
        envid = sys_exofork();
        if (envid == 0) {
                thisenv = &envs[ENVX(sys_getenvid())];
                return 0;
        }
        if (envid < 0)
                panic("fork: sys_exofork: %e", envid);

        for (addr = 0; addr < USTACKTOP; addr += PGSIZE) {
                if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
                        && (uvpt[PGNUM(addr)] & PTE_U)) {
                        duppage(envid, PGNUM(addr));
                }
        }
        if (sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_U | PTE_W | PTE_P) < 0)
                panic("fork: sys_page_alloc");

        sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);

        if (sys_env_set_status(envid, ENV_RUNNABLE) < 0)
                panic("fork: sys_env_set_status");

        return envid;
}

lib/pgfault.c
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
        if (_pgfault_handler == 0) {
                // First time through!
                // LAB 4: Your code here.
                if (sys_page_alloc(0, (void*)(UXSTACKTOP-PGSIZE), PTE_W | PTE_U | PTE_P) < 0)
                        panic("set_pgfault_handler: no phys mem");
        }
        // Save handler pointer for assembly to call.
        _pgfault_handler = handler;
        if (sys_env_set_pgfault_upcall(0, _pgfault_upcall) < 0)
                panic("set_pgfault_handler:sys_env_set_pgfault_upcall failed");
}
kern/syscall.c
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
        // LAB 4: Your code here.
        struct Env *e;
        int ret = envid2env(envid, &e, 0);

        if (ret)
                return ret;
        if (!e->env_ipc_recving)
                return -E_IPC_NOT_RECV;
        if (srcva < (void*)UTOP) {
                pte_t *pte;
                struct PageInfo *pg = page_lookup(curenv->env_pgdir, srcva, &pte);
                if (!pg)
                        return -E_INVAL;
                if ((*pte & perm & 7) != (perm & 7))
                        return -E_INVAL;
                if ((perm & PTE_W) && !(*pte & PTE_W))
                        return -E_INVAL;
                if (srcva != ROUNDDOWN(srcva, PGSIZE))
                        return -E_INVAL;
                if (e->env_ipc_dstva < (void*)UTOP) {
                        ret = page_insert(e->env_pgdir, pg, e->env_ipc_dstva, perm);
                        if (ret)
                                return ret;
                        e->env_ipc_perm = perm;
                }
        }
        e->env_ipc_recving = 0;
        e->env_ipc_from = curenv->env_id;
        e->env_ipc_value = value;
        e->env_status = ENV_RUNNABLE;
        e->env_tf.tf_regs.reg_eax = 0;

        return 0;
}
static int
sys_ipc_recv(void *dstva)
{
        // LAB 4: Your code here.
        if (dstva < (void*)UTOP)
                if (dstva != ROUNDDOWN(dstva, PGSIZE))
                        return -E_INVAL;
        curenv->env_ipc_recving = 1;
        curenv->env_status = ENV_NOT_RUNNABLE;
        curenv->env_ipc_dstva = dstva;
        sys_yield();

        return 0;
}
static int sys_env_set_status(envid_t envid, int status)
{
        if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
                return -E_INVAL;
        struct Env *env;
        if (envid2env(envid, &env, 1) != 0)
                return -E_BAD_ENV;
        env->env_status = status;
        return 0;
}
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
        // LAB 4: Your code here.
        struct Env *env;
        if (envid2env(envid, &env, 1) != 0)
                return -E_BAD_ENV;;
        env->env_pgfault_upcall = func;

        return 0;
}
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
        // Hint: This function is a wrapper around page_alloc() and
        //   page_insert() from kern/pmap.c.
        //   Most of the new code you write should be to check the
        //   parameters for correctness.
        //   If page_insert() fails, remember to free the page you
        //   allocated!

        // LAB 4: Your code here.
        struct Env *env;
        if (envid2env(envid, &env, 1) != 0)
                return -E_BAD_ENV;
        if ((uint32_t)va >= UTOP)
                return -E_INVAL;
        if (ROUNDUP(va, PGSIZE) != va)
                return -E_INVAL;
        if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || (perm & (~PTE_SYSCALL)))
                return -E_INVAL;
        struct PageInfo *pi = page_alloc(ALLOC_ZERO);
        if (pi == NULL)
                return -E_NO_MEM;
        pi->pp_ref++;
        if (page_insert(env->env_pgdir, pi, va, perm) != 0) {
                page_free(pi);
                return -E_NO_MEM;
        }

        return 0;
}
static int
sys_page_map(envid_t srcenvid, void *srcva,
             envid_t dstenvid, void *dstva, int perm)
{
struct Env *srcenv;
        if (envid2env(srcenvid, &srcenv, 1) != 0) {
                return -E_BAD_ENV;
        }

        struct Env *dstenv;
        if (envid2env(dstenvid, &dstenv, 1) != 0)
                return -E_BAD_ENV;

        if ((uint32_t)srcva >= UTOP)
                return -E_INVAL;

        if (ROUNDUP(srcva, PGSIZE) != srcva)
                return -E_INVAL;

        if ((uint32_t)dstva >= UTOP)
                return -E_INVAL;

        if (ROUNDUP(dstva, PGSIZE) != dstva)
                return -E_INVAL;

        if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || (perm & (~PTE_SYSCALL)))
                return -E_INVAL;

        pte_t *srcpte;
        struct PageInfo *srcpi = page_lookup(srcenv->env_pgdir, srcva, &srcpte);
        if (srcpi == NULL)
                return -E_INVAL;

        if (((*srcpte) & PTE_W) == 0 && ((perm & PTE_W) == PTE_W))
                return -E_INVAL;

        return page_insert(dstenv->env_pgdir, srcpi, dstva, perm);

}
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
        // LAB 4: Your code here.
        struct Env *e;
        int ret = envid2env(envid, &e, 0);

        if (ret)
                return ret;
        if (!e->env_ipc_recving)
                return -E_IPC_NOT_RECV;
        if (srcva < (void*)UTOP) {
                pte_t *pte;
                struct PageInfo *pg = page_lookup(curenv->env_pgdir, srcva, &pte);
                if (!pg)
                        return -E_INVAL;
                if ((*pte & perm & 7) != (perm & 7))
                        return -E_INVAL;
                if ((perm & PTE_W) && !(*pte & PTE_W))
                        return -E_INVAL;
                if (srcva != ROUNDDOWN(srcva, PGSIZE))
                        return -E_INVAL;
                if (e->env_ipc_dstva < (void*)UTOP) {
                        ret = page_insert(e->env_pgdir, pg, e->env_ipc_dstva, perm);
                        if (ret)
                                return ret;
                        e->env_ipc_perm = perm;
                }
        }
        e->env_ipc_recving = 0;
        e->env_ipc_from = curenv->env_id;
        e->env_ipc_value = value;
        e->env_status = ENV_RUNNABLE;
        e->env_tf.tf_regs.reg_eax = 0;

        return 0;
}
static int
sys_ipc_recv(void *dstva)
{
        // LAB 4: Your code here.
        if (dstva < (void*)UTOP)
                if (dstva != ROUNDDOWN(dstva, PGSIZE))
                        return -E_INVAL;
        curenv->env_ipc_recving = 1;
        curenv->env_status = ENV_NOT_RUNNABLE;
        curenv->env_ipc_dstva = dstva;
        sys_yield();

        return 0;
}
static int
sys_page_unmap(envid_t envid, void *va)
{
        // Hint: This function is a wrapper around page_remove().

        // LAB 4: Your code here.
        struct Env *env;
        if (envid2env(envid, &env, 1) != 0)
                return -E_BAD_ENV;

        if ((uint32_t)va >= UTOP)
                return -E_INVAL;

        if (ROUNDUP(va, PGSIZE) != va)
                return -E_INVAL;

        page_remove(env->env_pgdir, va);

        return 0;
}
kern/sched.c
void sched_yield(void)
{
        struct Env *idle;
        int i, envx, cur;
        if (curenv)
                cur = ENVX(curenv->env_id);
        else    cur = 0;
        for (i = 0; i < NENV; ++i) {
                envx = ENVX((cur + i) % NENV);
                if (envs[envx].env_status == ENV_RUNNABLE) {
                        env_run(&envs[envx]);
                }
        }
        if (curenv && curenv->env_status == ENV_RUNNING)
                env_run(curenv);
        sched_halt();
}
kern/trap.c
void trap_init_percpu(void)
{
        size_t i = cpunum();
        thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
        thiscpu->cpu_ts.ts_ss0 = GD_KD;
        thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);
        gdt[(GD_TSS0 >> 3) + i] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
                                        sizeof(struct Taskstate) - 1, 0);
        gdt[(GD_TSS0 >> 3) + i].sd_s = 0;
        ltr(GD_TSS0 + i * sizeof(struct Segdesc));
        lidt(&idt_pd);
}
void
page_fault_handler(struct Trapframe *tf)
{
uint32_t fault_va;
fault_va = rcr2();
cprintf("tf->tf_cs == %p\n", tf->tf_cs);
        if (tf->tf_cs == GD_KT) {
                print_trapframe(tf);
                panic("kernel fault va %08x\n", fault_va);
        }
        if (curenv->env_pgfault_upcall) {
                struct UTrapframe *utf;
                uintptr_t utf_addr;
                if (UXSTACKTOP-PGSIZE<=tf->tf_esp && tf->tf_esp <= UXSTACKTOP - 1)
                        utf_addr = tf->tf_esp - sizeof(struct UTrapframe) - 4;
                else
                        utf_addr = UXSTACKTOP - sizeof(struct UTrapframe);
                user_mem_assert(curenv, (void*)utf_addr,
                        sizeof(struct UTrapframe), PTE_W);
                utf = (struct UTrapframe *) utf_addr;

                utf->utf_fault_va = fault_va;
                utf->utf_err = tf->tf_err;
                utf->utf_regs = tf->tf_regs;
                utf->utf_eip = tf->tf_eip;
                utf->utf_eflags = tf->tf_eflags;
                utf->utf_esp = tf->tf_esp;

//              curenv->env_tf.env_tf
                curenv->env_tf.tf_eip = (uintptr_t)curenv->env_pgfault_upcall;
                curenv->env_tf.tf_esp = utf_addr;
                env_run(curenv);
        }
        cprintf("[%08x] user fault va %08x ip %08x\n",
                curenv->env_id, fault_va, tf->tf_eip);
        print_trapframe(tf);
        env_destroy(curenv);
}
kern/pmap.c
static void mem_init_mp(void)
{
        size_t i;
        uint32_t kstacktop_i;

        for (i = 0; i < NCPU; i++) {
                kstacktop_i = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
                boot_map_region(kern_pgdir,
                                kstacktop_i - KSTKSIZE,
                                ROUNDUP(KSTKSIZE, PGSIZE),
                                PADDR(percpu_kstacks[i]),
                                PTE_W | PTE_P);
        }
}
void * mmio_map_region(physaddr_t pa, size_t size)
{
uintptr_t ret = base;
        base += ROUNDUP(size, PGSIZE);

        if (base > MMIOLIM)
                panic("mmio_map_region: overflow");

        boot_map_region(kern_pgdir, ret, ROUNDUP(size, PGSIZE),
                        pa, PTE_PCD | PTE_PWT | PTE_W | PTE_P);

        return (void *)ret;
}

LAB 5
kern/syscall.c
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
        struct Env *e;
        int r;
        if ((r = envid2env(envid, &e, 1)) < 0)
                return -E_BAD_ENV;
        user_mem_assert(e, tf, sizeof(struct Trapframe), PTE_U);
        e->env_tf = *tf;
        e->env_tf.tf_eflags &= ~FL_IOPL_MASK;
        e->env_tf.tf_eflags &= ~FL_IF;
        e->env_tf.tf_cs |= 0x03;
        return 0;
}
case SYS_ipc_try_send:
                return sys_ipc_try_send(a1, a2, (void*)a3, a4);

static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
        struct Env *e;
        int ret = envid2env(envid, &e, 0);

        if (ret)
                return ret;
        if (!e->env_ipc_recving)
                return -E_IPC_NOT_RECV;
        if (srcva < (void*)UTOP) {
                pte_t *pte;
                struct PageInfo *pg = page_lookup(curenv->env_pgdir, srcva, &pte);
                if (!pg)
                        return -E_INVAL;
                if ((*pte & perm & 7) != (perm & 7))
                        return -E_INVAL;
                if ((perm & PTE_W) && !(*pte & PTE_W))
                        return -E_INVAL;
                if (srcva != ROUNDDOWN(srcva, PGSIZE))
                        return -E_INVAL;
                if (e->env_ipc_dstva < (void*)UTOP) {
                        ret = page_insert(e->env_pgdir, pg, e->env_ipc_dstva, perm);
                        if (ret)
                                return ret;
                        e->env_ipc_perm = perm;
                }
        }
        e->env_ipc_recving = 0;
        e->env_ipc_from = curenv->env_id;
        e->env_ipc_value = value;
        e->env_status = ENV_RUNNABLE;
        e->env_tf.tf_regs.reg_eax = 0;

        return 0;
}

fs/bc.c
static void bc_pgfault(struct UTrapframe *utf){
        void *addr = (void *) utf->utf_fault_va;
        uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
        int r;
        if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
                panic("page fault in FS: eip %08x, va %08x, err %04x",
                      utf->utf_eip, addr, utf->utf_err);
        if (super && blockno >= super->s_nblocks)
                panic("reading non-existent block %08x\n", blockno);
        addr = ROUNDDOWN(addr, PGSIZE);

        if ((r = sys_page_alloc(0, addr, (PTE_W | PTE_U | PTE_P))) < 0)
                panic("bc_pgfault: sys_page_alloc failed %e", r);
        if ((r = ide_read(blockno * BLKSECTS, addr, BLKSECTS)))
                panic("bc_pgfault: ide_read failed %e", r);
        if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
                panic("in bc_pgfault, sys_page_map: %e", r);
        if (bitmap && block_is_free(blockno))
                panic("reading free block %08x\n", blockno);
}
fs/fs.c
int
alloc_block(void)
{
        int blockno;

        for (blockno = 0; blockno < super->s_nblocks; blockno++) {
                if (block_is_free(blockno)) {
                        flush_block(diskaddr(blockno));
                        bitmap[blockno / 32] &= ~(1 << (blockno % 32));
                        return blockno;
                }
        }
        return -E_NO_DISK;
}
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
        uint32_t *ppdiskbno;
        int r = file_block_walk(f, filebno, &ppdiskbno, 1);

        if (r < 0)
                return r;
        if (!(*ppdiskbno)) {
                r = alloc_block();
                if (r < 0)
                        return -E_NO_DISK;
                *ppdiskbno = r;
        }
        *blk = diskaddr(*ppdiskbno);
        return 0;
}
