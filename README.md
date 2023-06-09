## Homework 3 for Uni

The purpose of this homework was to allow parent-child processes to communicate through shared memory structures. The parent process reports the shared memory structures to the OS (system call share_data). Direct children can get access to that shared memory from the OS (system call get_data).


### Structures shared and proc

The new shared structure contains: <br>
    - name (max 10 characters) <br>
    - pointer to the start of shared memory (virtual address) <br>
    - size of shared memory <br>

Proc structure now has additional: <br>
    - max number of shared strctures (10) <br>
    - array of shared structures <br>


### System calls

**int share_data(char \*name, void \*addr, int size)** <br>
Parent reports a new shared struct. If it is successfully added to the array of shared structs, process returns the index of the added struct. In case of an error, the function returns: <br>
    - -1: bad parameter <br>
    - -2: there is already a shared struct of the same name <br>
    - -3: array of shared structs is full <br>

**int get_data(char \*name, void \*\*addr)** <br>
Child asks for the shared struct that the parent already reported. The second parameter is passed through a reference where we store the addr of the found shared structure. In case of an error, the function returns: <br>
    - -1: bad parameter <br>
    - -2: no shared struct of the given name <br>

### System (kernel part) changes

1.  **fork() modification**:<br/>
When system call fork is called, new procces should inherit all shared structures from it's parent. At this point child has just copy of the shared memory. That means that child still can't communicate with it's parent.<br/>

2.  **exec() modification**:<br/>
Every process has it's user space (0-2GB) and kernel space (2-4GB, starts from KERNBASE defined in kernel/memlayout.h).
We wish to map the physical addresses parent process points to in the child's user space starting from 1GB. Now parent and child have different addresses in their user space that point to their PTE that point to the physical addr. of their shared data. 

3.  **exit() modification**:<br/>
Every process frees the shared memory struct it has access to by freeing the pointer to the virtual memory and flaging it as free (memstart -> 0 and size -> 0). Additionally, the parent process clears the physical memory. [kernel/proc.c]

### User programs
There are 3 user programs: parent one (**dalle**) and 2 children (**liSa** and **coMMa**).

- **dalle**:
    This program starts an interactive system for analysing text files. It starts children processes and waits for their end to end itself. Structures which are shared with it's children: <br>
        - path to file - char* <br>
        - current sentence - int <br>
        - longest word in current sentence - char* <br>
        - shortest word in current sentence - char* <br>
        - longest word in file - char* <br>
        - shortest word in file - char* <br>
        - length of the global longest word - int <br>
        - length of the global shortest word - int <br>
        - command indicator - int <br>

- **coMMa**:
    This program receives commands from the user: <br>
        - 1 -> latest: current sentence number +  shortest & longest word in curr sent <br>
        - 2 -> global extrema: shortest & longest word up to & including curr sent + their lengths <br>
        - 3 -> pause: pause analysis of file <br>
        - 4 -> resume: resumes analysis of file <br>
        - 5 -> notifies liSa to end and ends itself <br>

- **liSa**:
    This program analyses the file. First it finds the global values and then analyses every sentence. After every sentence it sleep(150) to simulate the analysis of data. After every sentence, it checks if the command indicator has changed. If it is 3 (pause) it sleep(1) before again checking the command indicator.

## Important - Makefile is for M1 Macs
Makefile given in this repo is the one one configured so the xv6 works on Macs <br> 
with M1 chip. After cloning this repo to M1 Mac, run these commands in terminal: <br>
`brew install qemu` <br>
`brew install x86_64-elf-gcc`

Create a .bash_profile that contains additional variables for running xv6. <br>
`cd ~/` <br>
`touch .bash_profile` <br>
`open -e .bash_profile` 

Add these commands to your .bash_profile file: <br>
`export TOOLPREFIX=x86_64-elf- ` <br>
`export QEMU=qemu-system-x86_64`

Finally, run the file: <br>
`source .bash_profile`

After cmpleting this, you can go into the cloned repo and start the xv6 with <br>
`make qemu`