// LAB 4
// kern/pmap.c
        if (ROUNDUP(base + size, PGSIZE) >= MMIOLIM)
                panic("mmio_map_region(): mapping over MMIOLIM");
        boot_map_region(kern_pgdir, base, ROUNDUP(size, PGSIZE), ROUNDDOWN(pa, PGSIZE), PTE_PCD|PTE_PWT|PTE_W);

        uintptr_t ret = base;
        base = ROUNDUP(base + size, PGSIZE);
        return (void*) ret;

void
sched_yield(void)
{
        struct Env *idle;

        // Implement simple round-robin scheduling.
        //
        // Search through 'envs' for an ENV_RUNNABLE environment in
        // circular fashion starting just after the env this CPU was
        // last running.  Switch to the first such environment found.
        //
        // If no envs are runnable, but the environment previously
        // running on this CPU is still ENV_RUNNING, it's okay to
        // choose that environment.
        //
        // Never choose an environment that's currently running on
        // another CPU (env_status == ENV_RUNNING). If there are
        // no runnable environments, simply drop through to the code
        // below to halt the cpu.

        // LAB 4: Your code here.
        uint32_t envs_index = 0;
        if (curenv)
          envs_index = ENVX(curenv->env_id);    //ak uz nejaky enviroment bezal, zisti jeho index v envs

        for (uint32_t i = 0; i < NENV; i++) {
          uint32_t index = (envs_index + i) % NENV;
          if (envs[index].env_status == ENV_RUNNABLE)
                env_run(envs + index);
        }
        if (curenv && (curenv->env_status == ENV_RUNNING))
                env_run(curenv);

        // sched_halt never returns
        sched_halt();
}
// lib/fork.c
// Custo  page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
        void *addr = (void *) utf->utf_fault_va;
        uint32_t err = utf->utf_err;
        int r;

        // Funkcia 'pgfault()' skontroluje, či ide o chybu zápisu (kontroluje chybový kód, 
        // či obsahuje príznak 'FEC_WR'
        // či je príslušný záznam PTE cieľovej stránky označený ako PTE_COW

        // Check that the faulting access was (1) a write, and (2) to a
        // copy-on-write page.  If not, panic.
        // Hint:
        //   Use the read-only page table mappings at uvpt
        //   (see <inc/memlayout.h>).

        // LAB 4: Your code here.
        if (
                (err & FEC_WR) == 0 ||
                (uvpd[PDX(addr)] & PTE_P)n== 0 ||
                (uvpt[PGNUM(addr)] & PTE_P) == 0 ||
                (uvpt[PGNUM(addr)] & PTE_COW) == 0)

                panic("pgfault: Not copy-on-write");
        r = sys_page_alloc(0, (void*)PFTEMP, PTE_U | PTE_W | PTE_P);
        if (r < 0)
                panic("pgfault: sys_page_alloc failed: %e", r);

        addr = ROUNDDOWN(addr, PGSIZE);
        memcpy(PFTEMP, addr, PGSIZE);

        r = sys_page_map(0, (void*)PFTEMP, 0, addr, PTE_U | PTE_W | PTE_P);
        if(r < 0)
                panic("pgfault: sys_page_map failed with %e", r);

        r = sys_page_unmap(0, (void*) PFTEMP);
        if(r < 0)
                panic("pgfault: sys_page_unmap failed with %e", r);
}
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void) // kopiruje mapovania stranok - nastavuje mapovanie stranok v detskom procese
{
        set_pgfault_handler(pgfault); //rodič inštaluje obsluhu výpadkov stránok v užívateľsko priestore
        envid_t envid;
        uintptr_t addr;
        int r;
        envid = sys_exofork(); // Rodič zavolá 'sys_exofork()', čím vytvorí potomka. 
        if(envid == 0){
                thisenv = &envs[ENVX(sys_getenvid())];
                return 0;
        }
        if(envid < 0)
                panic("sys_exofork: %e", envid);
        for(addr = UTEXT; addr < USTACKTOP; addr += PGSIZE){
                if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
                        duppage(envid, PGNUM(addr));
        }
        r = sys_page_alloc(envid, (void*)(UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W);
        if(r < 0)
                panic("fork: sys_page_alloc failed with %e", r);

        // Rodič nastaví vstupný bod obsluhy výnimiek výpadku stránok v užívateľskom priestore pre potomka
        r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
        if(r < 0)
                panic("fork: sys_env_set_pgfault_upcall failed with %e", r);

        // Potomok je nachystaný na spustenie, takže rodič ho označí ako spustiteľný. 
        r = sys_env_set_status(envid, ENV_RUNNABLE);
        if(r < 0)
                panic("fork: sys_env_set_status failed with %e", r);
        return envid;
}

// LAB 5
// kern/env.c
void
env_create(uint8_t *binary, enum EnvType type)
{
        struct Env *e;
        if(env_alloc(&e, 0) != 0)
          panic("env_create(): env_free_list or page_free_list is empty");
        load_icode(e, binary);
        if (type == ENV_TYPE_FS) {
                e->env_type = ENV_TYPE_FS;
                e->env_tf.tf_eflags |= FL_IOPL_MASK;
        }
        e->env_type = type;
}
//fs/bc.c
static void
bc_pgfault(struct UTrapframe *utf)
{
        void *addr = (void *) utf->utf_fault_va;
        uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
        int r;

        // Check that the fault was within the block cache region
        if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
                panic("page fault in FS: eip %08x, va %08x, err %04x",
                      utf->utf_eip, addr, utf->utf_err);

        // Sanity check the block number.
        if (super && blockno >= super->s_nblocks)
                panic("reading non-existent block %08x\n", blockno);

        // Allocate a page in the disk map region, read the contents
        // of the block from the disk into that page.
        // Hint: first round addr to page boundary. fs/ide.c has code to read
        // the disk.
        //
        // LAB 5: you code here:
        addr = ROUNDDOWN(addr, PGSIZE);
        sys_page_alloc(0, addr, PTE_W | PTE_U | PTE_P);

        r = ide_read(blockno * BLKSECTS, addr, BLKSECTS);
        if (r < 0)
                panic("ide_read(): %e", r);

        // Clear the dirty bit for the disk block page since we just read the
        // block from disk
        if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
                panic("in bc_pgfault, sys_page_map: %e", r);

        // Check that the block we read was allocated. (exercise for
        // the reader: why do we do this *after* reading the block
        // in?)
        if (bitmap && block_is_free(blockno))
                panic("reading free block %08x\n", blockno);
}
void
flush_block(void *addr)
{
        uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

        if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
                panic("flush_block of bad va %08x", addr);

        // LAB 5: Your code here.
        addr = ROUNDDOWN(addr, PGSIZE);
    if (!va_is_mapped(addr) || !va_is_dirty(addr))
      return;

    ide_write(blockno * BLKSECTS, addr, BLKSECTS);
        sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL);
        //panic("flush_block not implemented");
}
// fs/fs.c
int
alloc_block(void)
{
        // The bitmap consists of one or more blocks.  A single bitmap block
        // contains the in-use bits for BLKBITSIZE blocks.  There are
        // super->s_nblocks blocks in the disk altogether.

        // LAB 5: Your code here.
        uint32_t i;

    for (i = 0; i < super->s_nblocks; i++) {
      if (bitmap[i / 32] & (1 << (i%32))) {
        bitmap[i/32] &= ~(1<<(i%32));
        flush_block(bitmap);
        return i;
      }
        }
        return -E_NO_DISK;
        panic("alloc_block not implemented");
}
// Find the disk block number slot for the 'filebno'th block in file 'f'.
// Set '*ppdiskbno' to point to that slot.
// The slot will be one of the f->f_direct[] entries,
// or an entry in the indirect block.
// When 'alloc' is set, this function will allocate an indirect block
// if necessary.
//
// Returns:
//      0 on success (but note that *ppdiskbno might equal 0).
//      -E_NOT_FOUND if the function needed to allocate an indirect block, but
//              alloc was 0.
//      -E_NO_DISK if there's no space on the disk for an indirect block.
//      -E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT).
//
// Analogy: This is like pgdir_walk for files.
// Hint: Don't forget to clear any block you allocate.
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
        // LAB 5: Your code here.
                int r;

                if (filebno >= NDIRECT + NINDIRECT)
                 return -E_INVAL;

                if (filebno < NDIRECT) {
                 *ppdiskbno = &f->f_direct[filebno];
                 }
                else {
                 if (!f->f_indirect) {
                   if (!alloc)
                         return -E_NOT_FOUND;
                   else {
                        r = alloc_block();
                        if (r < 0)
                          return -E_NO_DISK;
                        f->f_indirect = r;
                        memset(diskaddr(f->f_indirect), 0, BLKSIZE);
                   }
                 }
                 *ppdiskbno = (uint32_t*) diskaddr(f->f_indirect) + filebno - NDIRECT;
                 /*uint32_t* indirect=diskaddr(f->f_indirect);
                 *ppdiskbno = &(indirect[filebno - NDIRECT]);*/
                }
                return 0;
        panic("file_block_walk not implemented");
}
// Set *blk to the address in memory where the filebno'th
// block of file 'f' would be mapped.
//
// Returns 0 on success, < 0 on error.  Errors are:
//      -E_NO_DISK if a block needed to be allocated but the disk is full.
//      -E_INVAL if filebno is out of range.
//
// Hint: Use file_block_walk and alloc_block.
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
                // LAB 5: Your code here.
                int r;
                uint32_t *ppdiskno;

                r = file_block_walk(f, filebno, &ppdiskno, 1);
                if (r < 0)
                    return -E_INVAL;
                //cprintf("\n\n\nfile_get_block(): *ppdiskno = %x \n\n\n", *ppdiskno);
                if (*ppdiskno == 0) {
                        r = alloc_block();
                    if (r < 0)
                        return -E_NO_DISK;
                    *ppdiskno = r;
                }

                *blk = diskaddr(*ppdiskno);
                return 0;
                panic("file_get_block not implemented");
}

// fs/serve.c
// Read at most ipc->read.req_n bytes from the current seek position
// in ipc->read.req_fileid.  Return the bytes read from the file to
// the caller in ipc->readRet, then update the seek position.  Returns
// the number of bytes successfully read, or < 0 on error.
int
serve_read(envid_t envid, union Fsipc *ipc)
{
        struct Fsreq_read *req = &ipc->read;
        struct Fsret_read *ret = &ipc->readRet;
        struct OpenFile *o;
        int r;

        if (debug)
                cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

        // Lab 5: Your code here:
        r = openfile_lookup(envid, req->req_fileid, &o); //lookup otvori file pre dane envid
        if (r < 0)
                return r;

        r = file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset);
        if (r < 0)
        return r;

        o->o_fd->fd_offset += r;

        return r;
}
// Write req->req_n bytes from req->req_buf to req_fileid, starting at
// the current seek position, and update the seek position
// accordingly.  Extend the file if necessary.  Returns the number of
// bytes written, or < 0 on error.
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
        if (debug)
                cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

        // LAB 5: Your code here.
        int r;
        struct OpenFile *o;

        r = openfile_lookup(envid, req->req_fileid, &o);
        if (r < 0)
                return r;

        r = file_write(o->o_file, req->req_buf, req->req_n, o->o_fd->fd_offset);
        if (r < 0)
                return r;

        o->o_fd->fd_offset += r;

        return r;

        panic("serve_write not implemented");
}
// lib/file.c
// Write at most 'n' bytes from 'buf' to 'fd' at the current seek position.
//
// Returns:
//       The number of bytes successfully written.
//       < 0 on error.
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
        // Make an FSREQ_WRITE request to the file system server.  Be
        // careful: fsipcbuf.write.req_buf is only so large, but
        // remember that write is always allowed to write *fewer*
        // bytes than requested.
        // LAB 5: Your code here
        int r;

        fsipcbuf.write.req_fileid = fd->fd_file.id;
        fsipcbuf.write.req_n = n;
        memmove(fsipcbuf.write.req_buf, buf, n);

        r = fsipc(FSREQ_WRITE, NULL);
        if(r < 0)
                panic("devfile_write(): %e",r);

        assert(r <= n);
        assert(r <= PGSIZE);

        return r;
        panic("devfile_write not implemented");
}
// kern/syscall.c
// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3), interrupts enabled, and IOPL of 0.
//
// Returns 0 on success, < 0 on error.  Errors are:
//      -E_BAD_ENV if environment envid doesn't currently exist,
//              or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
        // LAB 5: Your code here.
        // Remember to check whether the user has supplied us with a good
        // address!
        struct Env *e;
        int r;

        r = envid2env(envid, &e, true);
        if(r < 0) return r;

        user_mem_assert(curenv, (void *)tf, sizeof(struct Trapframe), PTE_U);
        e->env_tf = *tf;
        e->env_tf.tf_eflags &= ~FL_IOPL_MASK;
        e->env_tf.tf_eflags &= ~FL_IF;
        e->env_tf.tf_cs |= 0x03;
        return 0;
}
case SYS_env_set_trapframe:
                return sys_env_set_trapframe(a1, (struct Trapframe*) a2);

// lib/fork.c
//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
        int r;

        // Funkcia 'duppage()' nastaví PTE záznamy v rodičovi aj v potomkovi tak,
        // aby neobsahovali PTE_W, ale aby obsahovali PTE_COW. 
        // POZOR!!! Zásobník pre obsluhu výnimiek sa takto nesmie mapovať. 

        // Pre potomka treba zvlášť alokovať stránku na tento zásobník. 
// Keďže kopírovanie stránok robí obsluha výpadkov v užívateľskom priestore, 
        // a táto beží na zásobníku pre obsluhu výpadkov, nie je možné skopírovať tento zásobník: 
        // kto by to urobil??? Funkcia 'fork()' taktiež musí riešiť stránky, ktoré sú namapované, 
        // ale nie sú ani PTE_W ani PTE_COW. 

        // LAB 4: Your code here.
        //panic("duppage not implemented");

        void *addr = (void*) (pn * PGSIZE);

        /*if ((uint32_t)addr >= UTOP) { 
                panic("duppage: duplicate page above UTOP!");
                return -1;
        }
        if ((uvpt[PGNUM(addr)] & PTE_P) == 0) {
                panic("duppage: page table not present!");
                return -1;
        }
        if ((uvpd[PDX(addr)] & PTE_P) == 0) {
                panic("duppage: page directory not present!");
                return -1;
        }*/
        // if PTE_SHARE is set, just copy the mapping
        if(uvpt[pn] & PTE_SHARE) {
                r = sys_page_map(0, addr, envid, addr, uvpt[pn] & PTE_SYSCALL);
                if(r < 0){
                        panic("duppage: sys_page_map failed4 with : %e", r);
                        return r;
                }
                return 0;
        }

        if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
                r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P);
                if(r < 0)
                        panic("duppage: sys_page_map failed1 with %e", r);

                r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P);
                if (r < 0)
                        panic("duppage: sys_page_map failed2 with %e", r);
        }

        else {
                r = sys_page_map(0, addr, envid, addr, PTE_U|PTE_P);
                if(r < 0)
                        panic("duppage: sys_page_map failed3 with %e", r);

        }
        return 0;
}

// lib/spawn.c
// Copy the mappings for shared pages into the child address space.
static int
copy_shared_pages(envid_t child)
{
        // LAB 5: Your code here.
        int i, j, pn, r;
  
  for (i = 0; i < NPDENTRIES; i++) { 
    for (j = 0; j < NPTENTRIES; j++) {
      pn = i * NPDENTRIES + j;
      if (pn * PGSIZE < UTOP && uvpd[i] && uvpt[pn]) {
        if (uvpt[pn] & PTE_SHARE) {
          if ((r = sys_page_map(0, (void*)(pn * PGSIZE), child, 
            (void*)(pn * PGSIZE), uvpt[pn] & PTE_SYSCALL)) < 0) {
              cprintf("sys_page_map failed\n");
              return r;
          }
        }
      }
    }
  } 
return 0;
}
// kern/trap.c
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
// user/sh.c
case '<':       // Input redirection
                        // Grab the filename from the argument list
                        if (gettoken(0, &t) != 'w') {
                                cprintf("syntax error: < not followed by word\n");
                                exit();
                        }
                        fd = open(t, O_RDONLY);
                        if(fd < 0){
                                cprintf("open %s for read: %e", t, fd);
                                exit();
                        }
                        if(fd != 0){
                                dup(fd, 0);
                                close(fd);
                        }
                        break;
