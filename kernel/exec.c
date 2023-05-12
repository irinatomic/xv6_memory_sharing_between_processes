#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

/*  reads program from disk and readies the memory for the program */

int exec(char *path, char **argv) {
	char *s, *last;
	int i, off;
	uint argc, sz, sp, ustack[3+MAXARG+1];
	struct elfhdr elf;
	struct inode *ip;
	struct proghdr ph;						// ph = proghdr = program section header
	pde_t *pgdir, *oldpgdir;				// pgdir = new page directory to hold the process's virtual memory layout
	struct proc *curproc = myproc();

	// Begin a file system transaction to ensure consistency
	begin_op();				

	// Look up the file to be executed -> returns inode path to the code we want to execute
	if((ip = namei(path)) == 0){
		end_op();
		cprintf("exec: fail\n");
		return -1;
	}
	ilock(ip);
	pgdir = 0;			

	// Check for ELF header (ELF = izvrsni fajl)
	if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))				// readi(struct inode *ip, char *destination, uint off, uint n)
		goto bad;															
	if(elf.magic != ELF_MAGIC)
		goto bad;

	// Create a new page directory for the process
	if((pgdir = setupkvm()) == 0)
		goto bad;

	// Load program into memory (sz = size of new proces)
	sz = 0;
	for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){				// go through every header
		if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))			
			goto bad;
		if(ph.type != ELF_PROG_LOAD)
			continue;
		if(ph.memsz < ph.filesz)
			goto bad;
		if(ph.vaddr + ph.memsz < ph.vaddr)
			goto bad;
		if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)			// allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
			goto bad;
		if(ph.vaddr % PGSIZE != 0)
			goto bad;
		if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)		// loaduvm - Load a program segment from disk into pgdir 
			goto bad;														// (addr must be page-aligned && the pages from addr to addr+sz must already be mapped)
	}																		// loaduvm(pde_t *pgdir, char *va, struct inode *ip, uint offset, uint sz)
	iunlockput(ip);
	end_op();
	ip = 0;

	// NEW: dozvolimo dete procesu da pise u deljenu memoriju
	if(strncmp("comma", argv[0], SHAREDNAME) == 0 || strncmp("lisa", argv[0], SHAREDNAME) == 0)
		access_shared_memory(pgdir, 1);

	// Allocate 2 pages at the next page boundary. Make the 1. inaccessible. Use the 2. as the user stack [TOP TO BOTTOM]
	// Allocates 2 pages -> stack goes from the top (2. page) and when it reaches the 1. page throws error [page fault]
	sz = PGROUNDUP(sz);
	if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
		goto bad;
	clearpteu(pgdir, (char*)(sz - 2*PGSIZE));								// clear the page table entries for the page just above the user stack to mark it as inaccessible
	sp = sz;																//set the stack pointer (sp) to the top of the user stack 

	// Push argument strings, prepare rest of stack in ustack [puts arguments on stack]
	for(argc = 0; argv[argc]; argc++) {
		if(argc >= MAXARG)
			goto bad;
		sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
		if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
			goto bad;
		ustack[3+argc] = sp;
	}
	ustack[3+argc] = 0;

	ustack[0] = 0xffffffff;  													// fake return PC
	ustack[1] = argc;
	ustack[2] = sp - (argc+1)*4;  												// argv pointer

	sp -= (3+argc+1) * 4;														// pomeramo vrh steka
	if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
		goto bad;

	// Save program name for debugging.
	for(last=s=path; *s; s++)
		if(*s == '/')
			last = s+1;
	safestrcpy(curproc->name, last, sizeof(curproc->name));

	// Commit to the user image.
	oldpgdir = curproc->pgdir;
	curproc->pgdir = pgdir;
	curproc->sz = sz;
	curproc->tf->eip = elf.entry;  		// elf.entry = pointer to main of the program
	curproc->tf->esp = sp;
	switchuvm(curproc);					// postavlja u cr3 registar page directory od curproc
	freevm(oldpgdir);
	return 0;							

	bad:							
	if(pgdir)
		freevm(pgdir);
	if(ip){
		iunlockput(ip);
		end_op();
	}
	return -1;
}
