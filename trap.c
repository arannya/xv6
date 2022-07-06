#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

int
alloc_page(uint addr)
{
  char *mem;
  uint a;

  if(addr >= KERNBASE)
    return 0;
  if(addr > proc->sz)
    return 0;

  a = PGROUNDDOWN(addr);
  
  mem = kalloc();
  if(mem == 0){
     cprintf("alloc_page out of memory\n");
     
     return 0;
  }
  memset(mem, 0, PGSIZE);
  if(mappages(proc->pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("alloc_page out of memory (2)\n");
      //deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
  }
  return 1;
}

void copyOnWrite()
{ 

   uint va = rcr2();       // get va

  //errors taken care of
  if(proc == 0)     // null process
  { 
      cprintf("Error in copyOnWrite: No user process for cr2=0x%x\n", va);
      panic("PageFault");
  }

  pte_t *pte;

  pte = walkpgdir(proc->pgdir, (void*)va, 0);

  // page has write perm_S enabled
  if(PTE_W & *pte)
  {
      cprintf("addr 0x%x\n already writeable", va);
      
      panic("Error in COW_handle_pgfault: Already writeable");
  }

  if( pte == 0  || !(*pte) || va >= KERNBASE || !PTE_U || ! PTE_P )
  { 
      proc->killed = 1;
      cprintf("Error in COW_handle_pgfault: Illegal (virtual) addr at address 0x%x, killing proc %s id (pid) %d\n", va, proc->name, proc->pid);

      return;
  }

  uint pa = PTE_ADDR(*pte);                     
  uint refcount = get_refcount(pa);                

  if(refcount < 1)
  {
      panic("Error in copyOnWrite: Invalid Reference Count");
  }

  else if(refcount == 1)
  {
      *pte = PTE_W | *pte;   
      lcr3(V2P(proc->pgdir));
      return;
  }

  else                      
  {

      char* mem = kalloc();

      if(mem != 0)  
      {   
        memmove(mem, (char*)P2V(pa), PGSIZE);

        *pte =  PTE_U | PTE_W | PTE_P | V2P(mem);

        decrement_refcount(pa);

        lcr3(V2P(proc->pgdir));
        return;
      }

      proc->killed = 1;

      cprintf("Error in copyOnWrite: Out of memory, kill proc %s with pid %d\n", proc->name, proc->pid);          
      return;

  }

   
  lcr3(V2P(proc->pgdir));
}


void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpunum() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpunum(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    cprintf("Page fault occurred.\n");
    cprintf("rcr2 = %d\n", rcr2());
    pte_t * pt_entry = walkpgdir(proc->pgdir, (void *) PGROUNDDOWN(rcr2()),0);
    if (pt_entry && !(*pt_entry & PTE_W) && (*pt_entry & PTE_P))
    {
	copyOnWrite();
    }  
    else if (!alloc_page(rcr2())) panic("trap");
    break;

  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpunum(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpunum(), tf->eip,
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
