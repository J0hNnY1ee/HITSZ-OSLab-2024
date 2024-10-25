#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

const int nonblock = 1;

void waittest(void) {
  printf("wait test\n");

  int pid = fork();
  if (pid == 0) {
    // 这里睡眠足够长的时间，因此后面父进程进行非阻塞wait时该子进程一定还没有退出
    sleep(10);
    exit(0);
  }

  for (int i = 0; i < 3; i++) {
    if (wait(0, nonblock) < 0) {
      printf("no child exited yet, round %d\n", i);
    } else {
      printf("wait shouldn't block!\n");
      printf("wait test FAIL\n");
      exit(1);
    }
  }
  int child_pid = wait(0, 0);
  printf("child exited, pid %d\n", child_pid);

  printf("wait test OK\n");
}

int main(int argc, char *argv[]) {
  waittest();
  exit(0);
}