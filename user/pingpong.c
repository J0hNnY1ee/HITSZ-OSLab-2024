#include "../kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  int f2c[2];  // 父进程到子进程的管道
  int c2f[2];  // 子进程到父进程的管道

  // 创建两个管道
  pipe(f2c);
  pipe(c2f);

  int pid = fork();
  if (pid < 0) {
    // 创建子进程失败
    fprintf(2, "fork error\n");
    exit(1);
  } else if (pid == 0) {
    // 子进程
    close(f2c[1]);  // 关闭父进程到子进程管道的写端
    close(c2f[0]);  // 关闭子进程到父进程管道的读端

    char buf[5];
    int parent_pid;
    // 从父进程读取父进程的 PID
    read(f2c[0], &parent_pid, sizeof(parent_pid));
    // 从父进程读取数据
    read(f2c[0], buf, sizeof(buf));
    printf("%d: received ping from pid %d\n", getpid(), parent_pid);

    // 向父进程发送数据
    write(c2f[1], "pong", 5);

    close(f2c[0]);  // 关闭父进程到子进程管道的读端
    close(c2f[1]);  // 关闭子进程到父进程管道的写端
  } else {
    // 父进程
    close(f2c[0]);  // 关闭父进程到子进程管道的读端
    close(c2f[1]);  // 关闭子进程到父进程管道的写端

    int parent_pid = getpid();
    // 向子进程发送父进程的 PID
    write(f2c[1], &parent_pid, sizeof(parent_pid));
    // 向子进程发送数据
    write(f2c[1], "ping", 5);

    char buf[5];
    // 从子进程读取数据
    read(c2f[0], buf, sizeof(buf));
    printf("%d: received pong from pid %d\n", getpid(), pid);

    close(f2c[1]);  // 关闭父进程到子进程管道的写端
    close(c2f[0]);  // 关闭子进程到父进程管道的读端
  }

  exit(0);
}
