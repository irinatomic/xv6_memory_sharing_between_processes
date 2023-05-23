#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  		// defined by kernel.ld
pde_t *kpgdir;  			// for use in scheduler()

// Set up CPU's kernel segment descriptors. Run once on entry on each CPU.
void seginit(void)
{
	struct cpu *c;

	// Map "logical" addresses to virtual addresses using identity map. Cannot share a CODE descriptor for both kernel and user
	// because it would have to have DPL_USR, but the CPU forbids an interrupt from CPL=0 to DPL=3.
	c = &cpus[cpuid()];
	c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
	c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
	c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
	c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
	lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE [PAGE TABLE ENTRY] in page table pgdir that corresponds to virtual address (va).  
// If alloc!=0, create any required page table pages.
static pte_t * walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
	pde_t *pde;						// page directory entry
	pte_t *pgtab;					// page table

	// PDX(va) izvlaci gornjih 10 bita iz va i to ce biti index u direktorijumu (index >= 0 && index < 1024); kada mapiramo kernel u pgdir, pde ce biti 512
	// kada se mapira deljena memorija od roditelja u pgdir kod dece, pde bi trebalo da bude 256, jer krecemo od 1GB da mapiramo
	pde = &pgdir[PDX(va)];
	if(*pde & PTE_P){
		pgtab = (pte_t*)P2V(PTE_ADDR(*pde));		// PTE_ADDR(*pde) izvlaci gornjih 20 bita
	} else {
		if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
			return 0;
		memset(pgtab, 0, PGSIZE);					// Make sure all those PTE_P bits are zero.
		// The permissions here are overly generous, but they can be further restricted by the permissions in the page table entries, if necessary.
		*pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
	}
	return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to physical addresses starting at pa. 
// va and size might not be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
	char *a, *last;
	pte_t *pte;

	a = (char*)PGROUNDDOWN((uint)va);
	last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
	for(;;){
		if((pte = walkpgdir(pgdir, a, 1)) == 0)
			return -1;
		if(*pte & PTE_P)
			panic("remap");
		*pte = pa | perm | PTE_P;
		if(a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}
/*
There is one page table per process, plus one that's used when
a CPU is not running any process (kpgdir). The kernel uses the
current process's page table during system calls and interrupts;
page protection bits prevent user code from using the kernel's
mappings.

setupkvm() and exec() set up every page table like this:
	0..KERNBASE: user memory (text+data+stack+heap), mapped to phys memory allocated by the kernel
	KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
	KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data) for the kernel's instructions and r/o data
	data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP, rw data + free physical memory
	0xfe000000..0: mapped direct (devices such as ioapic)

The kernel allocates physical memory for its heap and for user memory
between V2P(end) and the end of physical memory (PHYSTOP)
(directly addressable from end..P2V(PHYSTOP)).
*/

// This table defines the kernel's mappings, which are present in every process's page table.
// Memory that the parent shares with child needs to be mapped to child's virtual mem. starting from 1GB
// whereas the memory the parent shares is in parent's memory data segment
static struct kmap {
	void *virt;												// start in virtual memory 
	uint phys_start;										// start in physical memory
	uint phys_end;											// end in phsycial memory [phys_end - phys_start = size of region]
	int perm;												// permissions (writable or not) - one child can write, others only ready from the shared mem.
} kmap[] = {
	{ (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, 	// I/O space, KERNBASE = 2GB
	{ (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     	// kern text+rodata (kernel code)
	{ (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, 	// kern data+memory
	{ (void*)DEVSPACE, DEVSPACE,      0,         PTE_W},	// more devices
};

// Set up kernel part of a page table. [kernel virtual mem]
pde_t*
setupkvm(void)
{
	pde_t *pgdir;
	struct kmap *k;

	if((pgdir = (pde_t*)kalloc()) == 0)						// kalloc - vraca slobodnu stranicu iz fizcke memorije (4KB); postoji lista slobodnih stranica fizicke memorije 
		return 0;
	memset(pgdir, 0, PGSIZE);								// PGSIZE = 4KB, P (present bit) is set to 0 which means that the directorium is not good
	if (P2V(PHYSTOP) > (void*)DEVSPACE)
		panic("PHYSTOP too high");
	for(k = kmap; k < &kmap[NELEM(kmap)]; k++)				// 4 prolaza kroz pretlju (kmap has 4 items)
		// mapiramo kernel na pgdir pocevsi od 2GB pa navise
		if(mappages(pgdir, k->virt, k->phys_end - k->phys_start, (uint)k->phys_start, k->perm) < 0) {
			freevm(pgdir);
			return 0;
		}
	return pgdir;
}

// Allocate one page table for the machine for the kernel address space for scheduler processes.
void kvmalloc(void)
{
	kpgdir = setupkvm();									// pdt_t kpgdir - globalna promenljva (iz types.h); globalni direktorjum za kernel
	switchkvm();
}

// Switch h/w page table register to the kernel-only page table, for when no process is running.
void switchkvm(void)
{
	// loads cr3 register [cr3 register has PHYSICAL ADRESS of direktory of curr. process]
	lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
// switch user virtual memory
void switchuvm(struct proc *p)
{
	// sets to cr3 register page directory of process p [cr3 - PHYSICAL ADRESS]
	if(p == 0)
		panic("switchuvm: no process");
	if(p->kstack == 0)
		panic("switchuvm: no kstack");
	if(p->pgdir == 0)
		panic("switchuvm: no pgdir");

	pushcli();
	mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
		sizeof(mycpu()->ts)-1, 0);
	SEG_CLS(mycpu()->gdt[SEG_TSS]);
	mycpu()->ts.ss0 = SEG_KDATA << 3;
	mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
	// setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g., inb and outb) from user space
	mycpu()->ts.iomb = (ushort) 0xFFFF;
	ltr(SEG_TSS << 3);
	lcr3(V2P(p->pgdir));  // switch to process's address space
	popcli();
}

// Load the initcode into address 0 of pgdir. [sz must be less than a page] 
// init user virtual memory
void inituvm(pde_t *pgdir, char *init, uint sz)		
{
	char *mem;

	if(sz >= PGSIZE)
		panic("inituvm: more than a page");
	mem = kalloc();												// allocate one page for the init process in user space
	memset(mem, 0, PGSIZE);										// set memory of the page (size is 4KB = PGSIZE)
	mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);			// to pgdir's virtual addr 0 is mapped that page which is at a physical addr V2P(mem)
	memmove(mem, init, sz);										// addr mem contains code of the init processu
}

// Load a program segment into pgdir.  addr must be page-aligned and the pages from addr to addr+sz must already be mapped.
int loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
	uint i, pa, n;
	pte_t *pte;

	if((uint) addr % PGSIZE != 0)
		panic("loaduvm: addr must be page aligned");
	for(i = 0; i < sz; i += PGSIZE){
		if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
			panic("loaduvm: address should exist");
		pa = PTE_ADDR(*pte);
		if(sz - i < PGSIZE)
			n = sz - i;
		else
			n = PGSIZE;
		if(readi(ip, P2V(pa), offset+i, n) != n)
			return -1;
	}
	return 0;
}

// [allocate user virtual memory - dynamic allocation of memory]
// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned. Returns new size or 0 on error.
int allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	char *mem;
	uint a;

	if(newsz >= SHAREDBASE)						// switched to 0x40000000 for 1GB
		return 0;
	if(newsz < oldsz)
		return oldsz;

	a = PGROUNDUP(oldsz);						
	for(; a < newsz; a += PGSIZE){
		mem = kalloc();							// new page in physical memory
		if(mem == 0){
			cprintf("allocuvm out of memory\n");
			deallocuvm(pgdir, newsz, oldsz);
			return 0;
		}
		memset(mem, 0, PGSIZE);
		// map a page of size PGSIZE (4KB), which is at physical addr V2P(mem), to virtual addr a in pgdir
		if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
			cprintf("allocuvm out of memory (2)\n");
			deallocuvm(pgdir, newsz, oldsz);
			kfree(mem);
			return 0;
		}
	}
	return newsz;
}

// [dealocate user virtual memory]
// Deallocate user pages to bring the process size from oldsz to newsz. oldsz and newsz need not be page-aligned, nor
// does newsz need to be less than oldsz.  oldsz can be larger than the actual process size. Returns the new process size.
int deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	pte_t *pte;
	uint a, pa;

	if(newsz >= oldsz)
		return oldsz;

	a = PGROUNDUP(newsz);
	for(; a  < oldsz; a += PGSIZE){
		pte = walkpgdir(pgdir, (char*)a, 0);					// returns pte = page table entry
		if(!pte)
			a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
		else if((*pte & PTE_P) != 0){
			pa = PTE_ADDR(*pte);								// pa = physical addr of pte
			if(pa == 0)
				panic("kfree");
			char *v = P2V(pa);
			kfree(v);											// free virtual memory
			*pte = 0;											// pte is set to 0 [same as if we had set the P to 0]
		}
	}
	return newsz;
}

// Free a page table and all the physical memory pages in the user part.
void freevm(pde_t *pgdir)
{
	uint i;

	if(pgdir == 0)
		panic("freevm: no pgdir");
	deallocuvm(pgdir, SHAREDBASE, 0);						// switched KERNBASE to 0x40000000 for 1GB
	for(i = 0; i < NPDENTRIES; i++){
		if(pgdir[i] & PTE_P){
			char * v = P2V(PTE_ADDR(pgdir[i]));
			kfree(v);
		}
	}
	kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible page beneath the user stack.
void clearpteu(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if(pte == 0)
		panic("clearpteu");
	*pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy of it for a child.
pde_t* copyuvm(pde_t *pgdir, uint sz)
{
	pde_t *d;
	pte_t *pte;
	uint pa, i, flags;
	char *mem;

	if((d = setupkvm()) == 0)										// d = new directorium
		return 0;
	for(i = 0; i < sz; i += PGSIZE){								// copies virtuel pages from the old proc to the new one (same pages, different mem)
		if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)			// addr of pte in page table directory...
			panic("copyuvm: pte should exist");
		if(!(*pte & PTE_P))
			panic("copyuvm: page not present");
		pa = PTE_ADDR(*pte);										// pa = physical addr (top 20 bits from *pte)
		flags = PTE_FLAGS(*pte);									// flags = bottom 12 bits from *pte
		if((mem = kalloc()) == 0)									// kalloc = allocate page in physical mem.
			goto bad;
		memmove(mem, (char*)P2V(pa), PGSIZE);						// copy a page from vitural memory at P2V(pa) to mem
		if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {	// in directorium d: map page from V2P(mem) to virtual addr i
			kfree(mem);
			goto bad;
		}
	}
	return d;

bad:
	freevm(d);
	return 0;
}

// Map user virtual address to kernel address.
char* uva2ka(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if((*pte & PTE_P) == 0)
		return 0;
	if((*pte & PTE_U) == 0)
		return 0;
	return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int copyout(pde_t *pgdir, uint va, void *p, uint len)
{
	char *buf, *pa0;
	uint n, va0;

	buf = (char*)p;
	while(len > 0){
		va0 = (uint)PGROUNDDOWN(va);
		pa0 = uva2ka(pgdir, (char*)va0);
		if(pa0 == 0)
			return -1;
		n = PGSIZE - (va - va0);
		if(n > len)
			n = len;
		memmove(pa0 + (va - va0), buf, n);
		len -= n;
		buf += n;
		va = va0 + PGSIZE;
	}
	return 0;
} 

/*	youtube: https://www.youtube.com/watch?v=dn55T2q63RU&ab_channel=NeilRhodes
	(mapping va of process to the pa of data)
	xv6 has 2 levels of paging (1. = directories, 2. = pages)
	linear = virtual addr: [dir (10 bits), table (10 bits), offset (12 bits)]
		- dir: takes us to a PDE in directory (is pointer to a table)
		- table: takes us to a PTE in table (is pointer to a page)
		- offset: in page (is a physical addr)
	
	cr3 = pointer to directorium of curr. active process
	Each process has it's own memory = it's page directorium with 1024 pointer to page tables 
	Process:
		- space for 1 process: 1024 * 1024 * 4KB = 4GB
		- user part: 0-2GB
		- kernel part: 2-4GB (from KERNELBASE)
		- kernel part for every process is the same
	Flags:
		- P (present) bit: 1 = page is loaded into memory, 0 = page is swapped into disk
		- W bit: 0 = read, 1 = read & write
		- U bit: privilege level

	Kalloc():
		- returns head page from physical memory (list of pages)
	
	pgdir = pointer to dir of curr process
	pde_t = PDE (page directory entry)
	pte_t = PTE (page table entry)
*/

/*  Map each shared region (max 10) from the physical addrs to new virtual addr in user space.
	Then update the va for the shared mem region in the parent proc to match the child.
	PTE_W           0x002   (writeable)
	PTE_U           0x004   (can be accessed from user space)
*/
int access_shared_memory(pde_t *pgdir){

	uint old_perm;
	pte_t *pte;	
	char* va = SHAREDBASE;
	struct proc *curproc = myproc(); 		

	for(int i = 0; i < SHAREDCOUNT; i++){
		if(curproc->shared[i].size == 0)
			break;

		// Map the physical memory starting from 'pa_start' to virtual memory starting from '*va'.
		// Map pages (PGSIZE = 4KB) until a total of 'size' bytes are mapped.
		// Roounds up the size to the nearest page size

		old_perm = PTE_FLAGS(curproc->shared[i].memstart);		//last 12 bits
		uint vpage_start = PGROUNDUP((uint)va);
		uint pa_start = curproc->shared[i].memstart;
		uint size = PGROUNDUP(curproc->shared[i].size);

		for(int i = 0; i < size; i += PGSIZE){

			if((pte = walkpgdir(curproc->parent_pgdir, pa_start, 0)) == 0)
				return -1;
			if(!(*pte & PTE_P))
				return -1;	

			uint pa = PTE_ADDR(*pte);

			if(mappages(pgdir, (uint*)vpage_start, PGSIZE, pa, PTE_W|PTE_U) < 0){
				cprintf("mapp_pa2va: couldn't map the pages \n");
				deallocuvm(pgdir, va + size, va);
				return -1;
			}	

			vpage_start += PGSIZE;
			pa_start += PGSIZE;
		}

		curproc->shared[i].memstart = va + old_perm;		
		va += size;
	}
	
	return 0;
}