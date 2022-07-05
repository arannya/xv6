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

int main()
{
   trace(1);
   sleep(1);
   sleep(1);
   sleep(1);
   sleep(1);
   printf(1,"number of system calls till exit = %d\n",trace(0));
   exit();
   //return 0;
}
