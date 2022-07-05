#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

char buf[8192];
char name[3];
char *echoargv[] = { "echo", "ALL", "TESTS", "PASSED", 0 };
int stdout = 1;

int main(void)
{
   int cs1, cs2, cs3, cs4;
   //trace(1);
   
   cs1 = csinfo();
   cs2 = csinfo();
   sleep(1);
   cs3 = csinfo();
   sleep(1);
   cs4 = csinfo();
   printf(1, "context switch counts = %d, %d, %d, %d\n", cs1, cs2, cs3, cs4);
   //printf(1,"number of system calls till exit = %d\n",trace(0));
   exit();
}
