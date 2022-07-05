/* VERSION OF 2021-03-27 */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define COUNT_MSG ">> If this test works correctly, %d \"Test passed\" messages should follow.\n"
#define PASS_MSG "Test passed: "
#define FAIL_MSG "Test failed: "
#define STARTDUMP ">> About to call dumppagetable() for "
#define ENDDUMP ">> Finished call to dumppagetable() for "

#define MAX_CHILDREN 16

int enable_dump = 0;
int dump_count;

void dump_for(const char *reason, int pid) {
    if (enable_dump) {
        if (dump_count >= 0) {
            printf(1, STARTDUMP "%s#%d\n", reason, dump_count);
        } else {
            printf(1, STARTDUMP "%s\n", reason);
        }
        dumppagetable(pid);
        if (dump_count >= 0) {
            printf(1, ENDDUMP "%s#%d\n", reason, dump_count);
        } else {
            printf(1, ENDDUMP "%s\n", reason);
        }
    }
}

void setup() {
    dump_count = -1;
    if (getpid() == 1) {
        mknod("console", 1, 1);
        open("console", O_RDWR);
        dup(0);
        dup(0);
    }
}

void finish() {
    if (getpid() == 1) {
        exit();
    } else {
        exit();
    }
}

void test_simple_crash_no_fork(void (*test_func)(), const char *no_crash_message) {
    test_func();
    printf(1, "%s\n", no_crash_message);
}
      
int test_simple_crash(void (*test_func)(), const char *crash_message, const char *no_crash_message) {
    int fds[2];
    pipe(fds);
    int pid = fork();
    if (pid == -1) {
        printf(1, FAIL_MSG "fork failed");
    } else if (pid == 0) {
        /* child process */
        close(1);
        dup(fds[1]);
        test_func();
        write(1, "X", 1);
        exit();
    } else {
        char text[1];
        close(fds[1]);
        int size = read(fds[0], text, 1);
        wait();
        close(fds[0]);
        if (size == 1) {
            printf(1, "%s\n", no_crash_message);
            return 0;
        } else {
            printf(1, "%s\n", crash_message);
            return 1;
        }
    }
    return 0;
}

static unsigned out_of_bounds_offset = 1;
void test_out_of_bounds_internal() {
    volatile char *end_of_heap = sbrk(0);
    (void) end_of_heap[out_of_bounds_offset];
}

int test_out_of_bounds_fork(int offset, const char *crash_message, const char *no_crash_message) {
    out_of_bounds_offset = offset;
    return test_simple_crash(test_out_of_bounds_internal, crash_message, no_crash_message);
}

void test_out_of_bounds_no_fork(int offset, const char *no_crash_message) {
    out_of_bounds_offset = offset;
    test_simple_crash_no_fork(test_out_of_bounds_internal, no_crash_message);
}

void _allocation_failure_message(int size, char *code) {
    if (size == 2 && code[0] == 'N') {
        if (code[1] == 'A') {
            printf(1, FAIL_MSG "allocating (but not using) memory with sbrk() returned error\n");
        } else if (code[1] == 'I') {
            printf(1, FAIL_MSG "allocation initialized to non-zero value\n");
        } else if (code[1] == 'R') {
            printf(1, FAIL_MSG "using parts of allocation read wrong value\n");
        } else if (code[1] == 'S') {
            printf(1, FAIL_MSG "sbrk() returned wrong value (wrong amount allocated?)\n");
        } else if (code[1] == 's') {
            printf(1, FAIL_MSG "sbrk() failed (returned -1)\n");
        } else if (code[1] == 'F') {
            printf(1, FAIL_MSG "fork failed\n");
        } else {
            printf(1, FAIL_MSG "unknown error\n");
        }
    } else if (size == 0) {
        printf(1, FAIL_MSG "unknown crash?\n");
    } else {
        printf(1, FAIL_MSG "unknown error\n");
    }
}

void _fail_allocation_test(int pipe_fd, char reason) {
    char temp[2] = {'N', reason};
    if (pipe_fd == -1) {
      _allocation_failure_message(2, temp);
    } else {
      write(pipe_fd, temp, 2);
      exit();
    }
}

void _pass_allocation_test(int pipe_fd, const char *message) {
    char temp[2] = {'Y', 'Y'};
    if (pipe_fd == -1) {
      printf(1, PASS_MSG "%s", message);
    } else {
      write(pipe_fd, temp, 2);
      exit();
    }
}

int _test_allocation_generic(
    int fork_before, int fork_after,
    int size, const char *describe_size, const char *describe_amount, int offset1, int count1, int offset2, int count2, int check_zero,
    int write_after
) {
  printf(1, "testing allocating %s and reading/writing to %s segments of it\n", describe_size, describe_amount);
  if (check_zero)
    printf(1, "... and verifying that (at least some of) the heap is initialized to zeroes\n");
  if (fork_before)
    printf(1, "... in a subprocess\n");
  if (fork_after)
    printf(1, "... and fork'ing%s after writing to parts of the heap\n",
        fork_before ? " again" : "");
  if (write_after)
    printf(1, "... and writing in the child process after forking and reading from the parent after that\n");
  dump_for("allocation-pre-allocate", getpid());
  int fds[2] = {-1, -1};
  int main_pid = -1;
  if (fork_before) {
    pipe(fds);
    main_pid = fork();
    if (main_pid == -1) {
      printf(1, FAIL_MSG "fork failed");
    } else if (main_pid != 0) {
      /* parent process */
      char text[10];
      close(fds[1]);
      wait();
      int size = read(fds[0], text, 10);
      close(fds[0]);
      if (fork_after) {
        if (size != 4) {
          printf(1, FAIL_MSG "allocation test did not return result from both processes after fork()ing after allocation?");
          return 0;
        } else if (text[0] != 'Y') {
          printf(1, "... test failed in child process:\n");
          _allocation_failure_message(2, text);
          return 0;
        } else if (text[2] != 'Y') {
          printf(1, "... test failed in grandchild process :\n");
          _allocation_failure_message(2, text + 2);
          return 0;
        }
      } else if (size < 1 || text[0] != 'Y') {
        _allocation_failure_message(size, text);
        return 0;
      } else {
        printf(1, PASS_MSG "allocating %s and using %s parts of allocation passed\n", describe_size, describe_amount);
        return 1;
      }
    } else {
      close(fds[0]);
    }
  }
  char *old_end_of_heap = sbrk(size);
  char *new_end_of_heap = sbrk(0);
  if (old_end_of_heap == (char*) -1) {
    _fail_allocation_test(fds[1], 's');
    return 0;
  } else if (new_end_of_heap - old_end_of_heap != size) {
    _fail_allocation_test(fds[1], 'S');
    return 0;
  } else {
    dump_for("allocation-pre-access", getpid());
    char *place_one = &old_end_of_heap[offset1];
    char *place_two = &old_end_of_heap[offset2];
    int i;
    for (i = 0; i < count1; ++i) {
      if (check_zero && place_one[i] != '\0') {
        _fail_allocation_test(fds[1], 'I');
        return 0;
      }
      place_one[i] = 'A';
    }
    for (i = 0; i < count2; ++i) {
      if (check_zero && place_two[i] != '\0') {
        _fail_allocation_test(fds[1], 'I');
        return 0;
      }
      place_two[i] = 'B';
    }
    dump_for("allocation-post-access", getpid());
    int pid = 1;
    if (fork_after) {
      pid = fork();
      if (pid == -1) {
        _fail_allocation_test(fds[1], 'F');
        return 0;
      }
    }
    if (pid == 0) {
      dump_for("allocation-post-fork-child", getpid());
      for (i = 0; i < count1; ++i) {
        if (place_one[i] != 'A') {
          _fail_allocation_test(fds[1], 'R');
          return 0;
        }
      } 
      for (i = 0; i < count2; ++i) {
        if (place_two[i] != 'B') {
          _fail_allocation_test(fds[1], 'R');
          return 0;
        }
      }
      _pass_allocation_test(fds[1], "allocation passed in child (expand + write + fork + read heap in child)\n");
      if (write_after) {
        place_one[i] = 'X';
        place_two[i] = 'Y';
      }
      exit();
    } else {
      if (fork_after) {
        wait();
        dump_for("allocation-post-fork-parent", getpid());
      }
      for (i = 0; i < count1; ++i) {
        if (place_one[i] != 'A') {
          _fail_allocation_test(fds[1], 'R');
          return 0;
        }
      } 
      for (i = 0; i < count2; ++i) {
        if (place_two[i] != 'B') {
          _fail_allocation_test(fds[1], 'R');
          return 0;
        }
      }
      _pass_allocation_test(fds[1], fork_after ?
        "allocation passed in parent (expand + write + fork + wait + read heap in parent)\n" :
        "allocation passed (expand + write + read heap)\n"
      );
      return 1;
    }
  }
}


int test_allocation_no_fork(int size, const char *describe_size, const char *describe_amount, int offset1, int count1, int offset2, int count2, int check_zero) {
    return _test_allocation_generic(0, 0, size, describe_size, describe_amount, offset1, count1, offset2, count2, check_zero, 0);
}

int test_allocation_then_fork(int size, const char *describe_size, const char *describe_amount, int offset1, int count1, int offset2, int count2, int check_zero, int write_after) {
    return _test_allocation_generic(0, 1, size, describe_size, describe_amount, offset1, count1, offset2, count2, check_zero, write_after);
}


int test_allocation_fork(int size, const char *describe_size, const char *describe_amount, int offset1, int count1, int offset2, int count2) {
    return _test_allocation_generic(1, 0, size, describe_size, describe_amount, offset1, count1, offset2, count2, 1, 0);
}

void wait_forever() {
  while (1) { sleep(1000); }
}

void test_copy_on_write_main_child(int result_fd, int size, const char *describe_size, int forks) {
  char *old_end_of_heap = sbrk(size);
  char *new_end_of_heap = sbrk(0);
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
      *p = 'A';
  }
  int children[MAX_CHILDREN] = {0};
  if (forks > MAX_CHILDREN) {
    printf(2, "unsupported number of children in test_copy_on_write\n");
  }
  int failed = 0;
  char failed_code = ' ';
  dump_for("copy-write-parent-before", getpid());
  for (int i = 0; i < forks; ++i) {
    int child_fds[2];
    pipe(child_fds);
    children[i] = fork();
    if (children[i] == -1) {
      printf(2, "fork failed\n");
      failed = 1;
      failed_code = 'f';
      break;
    } else if (children[i] == 0) {
      dump_for("copy-write-child-before-writes", getpid());
      int found_wrong_memory = 0;
      for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
        if (*p != 'A') {
          found_wrong_memory = 1;
        }
      }
      int place_one = size / 2;
      old_end_of_heap[place_one] = 'B' + i;
      int place_two = 4096 * i;
      if (place_two >= size) {
          place_two = size - 1;
      }
      if (size <= 4096) {
          dump_for("copy-write-child-after-first-write", getpid());
      } else if (size > 4096) {
          dump_for("copy-write-child-after-write-1", getpid());
      }
      old_end_of_heap[place_two] = 'C';
      int place_three = 4096 * (i - 1);
      if (place_three >= size || place_three < 0) {
          place_three = size - 2;
      }
      if (size > 4096) {
          dump_for("copy-write-child-after-write-2", getpid());
      }
      int place_four = 4096 * (i + 1);
      if (place_four >= size) {
          place_four = size - 3;
      }
      if (size > 4096) {
          dump_for("copy-write-child-after-write-3", getpid());
      }
      /*
      printf(1, "[Debugging info: three: %c; one: %c; four: %c; already_wrong: %d; i: %d]\n",
        old_end_of_heap[place_three],
        old_end_of_heap[place_one],
        old_end_of_heap[place_four],
        found_wrong_memory,
        i);
      */
      if (old_end_of_heap[place_three] != 'A' || old_end_of_heap[place_one] != 'B' + i ||
          old_end_of_heap[place_four] != 'A') {
          found_wrong_memory = 1;
      }
      write(child_fds[1], found_wrong_memory ? "-" : "+", 1);
      wait_forever();
    } else {
      char buffer[1] = {'X'};
      read(child_fds[0], buffer, 1);
      if (buffer[0] != '+') {
        failed = 1;
        failed_code = 'c';
      }
      close(child_fds[0]); close(child_fds[1]);
      dump_for("copy-write-parent-after", getpid());
      dump_for("copy-write-child-after", children[i]);
    }
  }
  old_end_of_heap[size / 2] = 'B';
  old_end_of_heap[size / 2] = 'A';
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
    if (*p != 'A') {
      failed = 1;
      failed_code = 'p';
    }
  }
  for (int i = 0; i < forks; ++i) {
    kill(children[i]);
    wait();
  }
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
    if (*p != 'A') {
      failed = 1;
      failed_code = 'p';
    }
  }
  if (failed) {
    char buffer[2] = {'N', ' '};
    buffer[1] = failed_code;
    write(result_fd, buffer, 2);
  } else {
    write(result_fd, "YY", 2);
  }
}

void test_copy_on_write_main_child_alt(int result_fd, int size, const char *describe_size, int forks, int early_term) {
  char *old_end_of_heap = sbrk(size);
  char *new_end_of_heap = sbrk(0);
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
      *p = 'A';
  }
  int children[MAX_CHILDREN] = {0};
  int child_fds[MAX_CHILDREN][2];
  if (forks > MAX_CHILDREN) {
    printf(2, "unsupported number of children in test_copy_on_write\n");
  }
  int failed = 0;
  char failed_code = ' ';
  for (int i = 0; i < forks; ++i) {
    sleep(1);
    pipe(child_fds[i]);
    children[i] = fork();
    if (children[i] == -1) {
      printf(2, "fork failed\n");
      failed = 1;
      failed_code = 'f';
      break;
    } else if (children[i] == 0) {
      int found_wrong_memory = 0;
      for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
        if (*p != 'A') {
          found_wrong_memory = 1;
        }
      }
      int place_one = size / 2;
      old_end_of_heap[place_one] = 'B' + i;
      int place_two = 4096 * i;
      if (place_two >= size) {
          place_two = size - 1;
      }
      old_end_of_heap[place_two] = 'C' + i;
      int place_three = 4096 * (i - 1);
      if (place_three >= size || place_three < 0) {
          place_three = size - 2;
      }
      int place_four = 4096 * (i + 1);
      if (place_four >= size || place_four < 0) {
          place_four = size - 3;
      }
      if (old_end_of_heap[place_three] != 'A' || old_end_of_heap[place_one] != 'B' + i ||
          old_end_of_heap[place_four] != 'A') {
          found_wrong_memory = 1;
      }
      sleep(5);
      if (old_end_of_heap[place_three] != 'A' || 
          old_end_of_heap[place_four] != 'A' ||
          old_end_of_heap[place_two] != 'C' + i || old_end_of_heap[place_one] != 'B' + i) {
          found_wrong_memory = 1;
      }
      write(child_fds[i][1], found_wrong_memory ? "-" : "+", 1);
      if (early_term) {
          exit();
      } else {
          wait_forever();
      }
    }
  }
  for (int i = 0; i < forks; ++i) {
    if (children[i] != -1) {
      char buffer[1] = {'X'};
      read(child_fds[i][0], buffer, 1);
      if (buffer[0] == 'X') {
        failed = 1;
        failed_code = 'P';
      } else if (buffer[0] != '+') {
        failed = 1;
        failed_code = 'c';
      }
      close(child_fds[i][0]); close(child_fds[i][1]);
      dump_for("copy-write-child", children[i]);
    }
  }
  dump_for("copy-write-parent", getpid());
  for (int i = 0; i < forks; ++i) {
    kill(children[i]);
    wait();
  }
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
    if (*p != 'A') {
      failed = 1;
      failed_code = 'p';
    }
  }
  if (failed) {
    char buffer[2] = {'N', ' '};
    buffer[1] = failed_code;
    write(result_fd, buffer, 2);
  } else {
    write(result_fd, "YY", 2);
  }
}

void _show_cow_test_error(char *code) {
  if (code[0] == 'X') {
    printf(1, FAIL_MSG "copy on write test failed --- crash?\n");
  } else if (code[0] == 'N') {
    switch (code[1]) {
    case 'f':
      printf(1, FAIL_MSG "copy on write test failed --- fork failed\n");
      break;
    case 'p':
      printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in parent\n");
      break;
    case 'P':
      printf(1, FAIL_MSG "copy on write test failed --- pipe read problem\n");
      break;
    case 'c':
      printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in child\n");
      break;
    default:
      printf(1, FAIL_MSG"copy on write test failed --- unknown reason\n");
      break;
    }
  }
}

int test_copy_on_write_less_forks(int size, const char *describe_size, int forks) {
  int fds[2];
  pipe(fds);
  test_copy_on_write_main_child(fds[1], size, describe_size, forks);
  char text[2] = {'X', 'X'};
  read(fds[0], text, 2);
  close(fds[0]); close(fds[1]);
  if (text[0] != 'Y') {
    _show_cow_test_error(text);
    return 0;
  } else {
    printf(1, PASS_MSG "copy on write test passed --- allocate %s; "
           "fork %d children; read+write small parts in each child\n",
           describe_size, forks);
    return 1;
  }
}

int test_copy_on_write_less_forks_alt(int size, const char *describe_size, int forks, int early_term) {
  int fds[2];
  pipe(fds);
  test_copy_on_write_main_child_alt(fds[1], size, describe_size, forks, early_term);
  char text[2] = {'X', 'X'};
  read(fds[0], text, 2);
  close(fds[0]); close(fds[1]);
  if (text[0] != 'Y') {
    _show_cow_test_error(text);
    return 0;
  } else {
    printf(1, PASS_MSG "copy on write test passed --- allocate %s; "
           "fork %d children; read+write small parts in each child\n",
           describe_size, forks);
    return 1;
  }
}

int _test_copy_on_write(int size,  const char *describe_size, int forks, int use_alt, int early_term, int pre_alloc, const char* describe_prealloc) {
  int fds[2];
  pipe(fds);
  int pid = fork();
  if (pid == -1) {
    printf(1, FAIL_MSG "fork failed");
  } else if (pid == 0) {
    if (pre_alloc > 0) {
      sbrk(pre_alloc);
    }
    if (use_alt) {
      test_copy_on_write_main_child_alt(fds[1], size, describe_size, forks, early_term);
    } else {
      test_copy_on_write_main_child(fds[1], size, describe_size, forks);
    }
    exit();
  } else if (pid > 0) {
    printf(1, "running copy on write test: ");
    if (pre_alloc > 0) {
      printf(1, "allocate but do not use %s; ", describe_prealloc);
    }
    printf(1, "allocate and use %s; fork %d children; read+write small parts in each child",
        describe_size, forks);
    if (use_alt) {
      printf(1, " [and try to keep children running in parallel]");
    }
    printf(1, "\n");
    char text[10] = {'X', 'X'};
    close(fds[1]);
    read(fds[0], text, 10);
    wait();
    close(fds[0]);
    if (text[0] != 'Y') {
      _show_cow_test_error(text);
      return 0;
    } else {
      printf(1, PASS_MSG "copy on write test passed --- allocate %s; "
             "fork %d children; read+write small parts in each child\n",
             describe_size, forks);
      return 1;
    }
  } else if (pid == -1) {
     printf(1, FAIL_MSG "copy on write test failed --- first fork failed\n");
  }
  return 0;
}

int test_copy_on_write(int size, const char *describe_size, int forks) {
  return _test_copy_on_write(size, describe_size, forks, 0, 0, 0, "");
}

int test_copy_on_write_alloc_unused(int unused_size, const char *describe_unused_size, int size, const char *describe_size, int forks) {
  return _test_copy_on_write(size, describe_size, forks, 0, 0, unused_size, describe_unused_size);
}

int test_copy_on_write_alt(int size, const char *describe_size, int forks) {
  return _test_copy_on_write(size, describe_size, forks, 1, 0, 0, "");
}

int test_read_into_alloc_no_fork(int size, int offset, int read_count, char *describe_size, char *describe_offset) {
    printf(1, "testing read(), writing %d bytes to a location %s into a %s allocation\n",
        read_count, describe_offset, describe_size);
    int fd = open("tempfile", O_WRONLY | O_CREATE);
    static char buffer[128]; // static to avoid running out of stack space
    for (int i = 0 ; i < sizeof buffer; ++i) {
        buffer[i] = 'X';
    }
    for (int i = 0; i < read_count; i += sizeof buffer) {
        write(fd, buffer, sizeof buffer);
    }
    close(fd);
    fd = open("tempfile", O_RDONLY);
    if (fd == -1) {
        printf(2, "error opening tempfile");
    }
    char *heap = sbrk(0);
    sbrk(size);
    char *loc = heap + offset;
    int count = read(fd, loc, read_count);
    int failed_value = 0;
    failed_value = loc[-1] != '\0';
    for (int i = 0; i < read_count; ++i) {
        if (loc[i] != 'X') {
            failed_value = 1;
        }
    }
    if (loc[read_count] != '\0') {
        failed_value = 1;
    }
    close(fd);
    unlink("tempfile");
    if (count != read_count) {
        printf(1, FAIL_MSG "wrong return value from read()\n");
        return 0;
    } else if (failed_value) {
        printf(1, FAIL_MSG "wrong value written to memory by read()\n");
        return 0;
    } else {
        printf(1, PASS_MSG "read() into heap allocation\n");
        return 1;
    }
}

int test_read_into_alloc(int size, int offset, int read_count, char *describe_size, char *describe_offset) {
    int pipe_fds[2];
    pipe(pipe_fds);
    int pid = fork();
    if (pid == -1) {
        printf(1, FAIL_MSG "fork failed");
    } else if (pid == 0) {
        close(pipe_fds[0]);
        char result_str[1] = {'N'};
        if (test_read_into_alloc_no_fork(size, offset, read_count, describe_size, describe_offset)) {
            result_str[0] = 'Y';
        }
        write(pipe_fds[1], result_str, 1);
        exit();
    } else {
        close(pipe_fds[1]);
        char result_str[1] = {'N'};
        read(pipe_fds[0], result_str, 1);
        wait();
        return result_str[0] == 'Y';
    }
    return 0;
}

int test_read_into_cow_less_forks(int size, int offset, int read_count, char *describe_size, char *describe_offset) {
    printf(1, "testing read(), writing %d bytes to a location %s into a %s copy-on-write allocation\n",
        read_count, describe_offset, describe_size);
    int fd = open("tempfile", O_WRONLY | O_CREATE);
    static char buffer[128]; // static to avoid running out of stack space
    for (int i = 0 ; i < sizeof buffer; ++i) {
        buffer[i] = 'X';
    }
    for (int i = 0; i < read_count; i += sizeof buffer) {
        write(fd, buffer, sizeof buffer);
    }
    close(fd);
    fd = open("tempfile", O_RDONLY);
    if (fd == -1) {
        printf(2, "error opening tempfile");
    }
    char *heap = sbrk(0);
    sbrk(size);
    for (int i = 0; i < size; ++i) {
        heap[i] = 'Y';
    }
    char *loc = heap + offset;
    int pipe_fds[2];
    pipe(pipe_fds);
    int pid = fork();
    if (pid == -1) {
        printf(1, FAIL_MSG "fork failed");
        exit();
    } else if (pid == 0) {
        close(pipe_fds[0]);
        int count = read(fd, loc, read_count);
        int failed_value = 0;
        failed_value = loc[-1] != 'Y';
        for (int i = 0; i < read_count; ++i) {
            if (loc[i] != 'X') {
                failed_value = 1;
            }
        }
        if (loc[read_count] != 'Y') {
            failed_value = 1;
        }
        close(fd);
        unlink("tempfile");
        if (count != read_count) {
            printf(1, FAIL_MSG "wrong return value from read()\n");
            write(pipe_fds[1], "N", 1);
        } else if (failed_value) {
            printf(1, FAIL_MSG "wrong value written to memory by read()\n");
            write(pipe_fds[1], "N", 1);
        } else {
            printf(1, PASS_MSG "correct value read into copy-on-write allocation\n");
            write(pipe_fds[1], "Y", 1);
        }
        close(pipe_fds[1]);
        exit();
    } else {
        close(pipe_fds[1]);
        char result_buf[1] = {'N'};
        read(pipe_fds[0], result_buf, 1);
        close(pipe_fds[0]);
        wait();
        printf(1, "testing correct value for heap in parent after read() in child\n");
        int found_wrong = 0;
        for (int i = 0; i < size; ++i) {
            if (heap[i] != 'Y') {
                found_wrong = 1;
            }
        }
        if (found_wrong) { 
            printf(1, FAIL_MSG "wrong value in parent after read() in child\n");
        } else {
            printf(1, PASS_MSG "correct value in parent after read into copy-on-write allocation\n");
        }
        return (found_wrong == 0) + (result_buf[0] == 'Y');
    }
}

int test_read_into_cow(int size, int offset, int read_count, char *describe_size, char *describe_offset) {
    int pipe_fds[2];
    pipe(pipe_fds);
    int pid = fork();
    if (pid == -1) {
        printf(1, FAIL_MSG "fork failed");
        exit();
    } else if (pid == 0) {
        close(pipe_fds[0]);
        char result_str[1] = {'N'};
        if (test_read_into_cow_less_forks(size, offset, read_count, describe_size, describe_offset)) {
            result_str[0] = 'Y';
        }
        write(pipe_fds[1], result_str, 1);
        exit();
    } else {
        close(pipe_fds[1]);
        char result_str[1] = {'N'};
        read(pipe_fds[0], result_str, 1);
        wait();
        return result_str[0] == 'Y';
    }
}

int test_dealloc_cow_less_forks(int size) {
    char *heap = sbrk(0);
    sbrk(size);
    printf(1, "testing that deallocating (with sbrk()) shared copy-on-write memory in child does not change it in parent\n");
    for (int i = 0; i < size; ++i) {
        heap[i] = 'Y';
    }
    int pid = fork();
    if (pid == 0) {
        sbrk(-size);
        exit();
    } else {
        wait();
        int found_wrong = 0;
        for (int i = 0; i < size; ++i) {
            if (heap[i] != 'Y') {
                found_wrong = 1;
            }
        }
        if (found_wrong) {
            printf(1, FAIL_MSG "wrong value in parent after sbrk(-size) in child\n");
            return 0;
        } else {
            printf(1, PASS_MSG "correct values in parent after sbrk(-size) in child\n");
            return 1;
        }
    }
}

int test_copy_on_write_control_parent_child(int size, int parent_write, int child_write) {
    char *heap = sbrk(0);
    printf(1, "testing allocating %d bytes, and %s in parent and %s in child\n",
        size,
        parent_write ? "writing+reading" : "reading",
        child_write ? "writing+reading" : "reading"
    );
    sbrk(size);
    for (int i = 0; i < size; ++i) {
        heap[i] = 'X';
    }
    int pipe_fds[2];
    pipe(pipe_fds);
    int other_pipe_fds[2];
    pipe(other_pipe_fds);
    int pid = fork();
    if (pid == 0) {
        if (child_write) {
            for (int i = size - 1; i >= 0; --i) {
                heap[i] = 'Y';
            }
        }
        write(pipe_fds[1], "_", 1);
        char c;
        read(other_pipe_fds[0], &c, 1);
        int failed = 0;
        for (int i = size - 1; i >= 0; --i) {
            if (heap[i] != (child_write ? 'Y' : 'X')) {
                failed = 1;
            }
        }
        write(pipe_fds[1], failed ? "N" : "Y", 1);
        exit();
    } else {
        if (parent_write) {
            for (int i = size - 1; i >= 0; --i) {
                heap[i] = 'Z';
            }
        }
        char c;
        read(pipe_fds[0], &c, 1);
        write(other_pipe_fds[1], "_", 1);
        int failed = 0;
        for (int i = size - 1; i >= 0; --i) {
            if (heap[i] != (parent_write ? 'Z' : 'X')) {
                failed = 1;
            }
        }
        c = 'N';
        read(pipe_fds[0], &c, 1);
        close(pipe_fds[0]);
        close(other_pipe_fds[0]);
        close(pipe_fds[1]);
        close(other_pipe_fds[1]);
        wait();
        if (failed) {
            printf(1, FAIL_MSG "wrong value in parent\n");
            return 0;
        } else if (c != 'Y') {
            printf(1, FAIL_MSG "wrong value in child\n");
            return 0;
        } else {
            printf(1, PASS_MSG "correct values in parent/child\n");
            return 1;
        }
    }
}

int test_copy_on_write_control_parent_child_fork(int size, int parent_write, int child_write) {
    int fds[2] = {-1,-1};
    pipe(fds);
    int pid = fork();
    if (pid == 0) {
        close(fds[0]);
        if (test_copy_on_write_control_parent_child(size, parent_write, child_write)) {
            write(fds[1], "Y", 1);
        } else {
            write(fds[1], "N", 1);
        }
        exit();
        return 0;
    } else {
        close(fds[1]);
        char c = '?';
        read(fds[0], &c, 1);
        close(fds[0]);
        wait();
        if (c == '?') {
            printf(1, "unknown failure of copy-on-write test\n");
            return 0;
        } else
            return c == 'Y';
    }
}
