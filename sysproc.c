#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_trace(void)
{
  int is_traced;

  if(argint(0, &is_traced) < 0)
    return -1;
  proc->is_traced = is_traced;
  return proc->syscall_count;
}

int sys_csinfo(void)
{
  return proc->context_switch_count;
}

int sys_settickets(void)
{
  int ticket_count;

  if(argint(0, &ticket_count) < 0)
    return -1;
  proc->ticket_count = ticket_count;
  return proc->ticket_count;
}

int sys_getprocessesinfo(void)
{
  struct processes_info *pi;
  int i, total_tickets = 0;
  if (argptr (0 , (void*)&pi ,sizeof(*pi)) < 0)
    return - 1;
  count_processes(pi);
  for (i = 0; i < pi->num_processes; i++)
  {
     total_tickets += pi->tickets[i];
  }
  return total_tickets;
}

int sys_yield(void)
{
  yield();
  return 0;
}

int sys_random(void)
{
  static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
  unsigned int b;
  unsigned int * rand;
  if (argptr (0 , (void*)&rand ,sizeof(*rand)) < 0)
    return - 1;
  b  = ((z1 << 6) ^ z1) >> 13;
  z1 = ((z1 & 4294967294U) << 18) ^ b;
  b  = ((z2 << 2) ^ z2) >> 27; 
  z2 = ((z2 & 4294967288U) << 2) ^ b;
  b  = ((z3 << 13) ^ z3) >> 21;
  z3 = ((z3 & 4294967280U) << 7) ^ b;
  b  = ((z4 << 3) ^ z4) >> 12;
  z4 = ((z4 & 4294967168U) << 13) ^ b;
  *rand = (z1 ^ z2 ^ z3 ^ z4) / 2;
  return 0;
}

int sys_dumppagetable(void)
{
  int i = 0, pid;
  struct proc * p;
  if(argint(0, &pid) < 0)
    return -1;
  p = getProcByPid(pid);
  cprintf("START PAGE TABLE\n");
  for (i = 0; i < ((p->sz)>>12); i++)
  {
      pte_t * pt_entry = walkpgdir(p->pgdir, (void *) (i<<12), 0);
      if (!(*pt_entry)) continue;
      cprintf("%x ", i&0xff );
      if (*pt_entry & PTE_P) cprintf("P ");
      else cprintf("- ");
      if (*pt_entry & PTE_U) cprintf("U ");
      else cprintf("- ");
      if (*pt_entry & PTE_W) cprintf("W ");
      else cprintf("- ");
      cprintf("%x\n", ((*pt_entry)>>12)&0xff );
  }  
  cprintf("END PAGE TABLE\n");
  return 0;
}
