#include "../kernel/types.h"   // 包含 xv6 操作系统的类型定义
#include "user.h"              // 包含用户空间库函数定义

int main(int argc, char *argv[]) {
  int f2c[2];  // 管道数组，用于父进程到子进程的通信
  int c2f[2];  // 管道数组，用于子进程到父进程的通信

  // 创建两个管道
  pipe(f2c);   // f2c[0] 是读端，f2c[1] 是写端
  pipe(c2f);   // c2f[0] 是读端，c2f[1] 是写端

  // 创建子进程
  int pid = fork();
  if (pid < 0) {  // fork() 返回负值表示子进程创建失败
    fprintf(2, "fork error\n"); // 输出错误信息到标准错误流
    exit(1); // 以状态码 1 退出程序
  } else if (pid == 0) {
    // 子进程代码执行区域
    close(f2c[1]);  // 关闭父进程到子进程管道的写端，因为子进程只需要读
    close(c2f[0]);  // 关闭子进程到父进程管道的读端，因为子进程只需要写

    char buf[5];    // 用于接收 "ping" 的缓冲区
    int parent_pid; // 存储父进程 PID 的变量

    // 从父进程管道读取父进程的 PID
    read(f2c[0], &parent_pid, sizeof(parent_pid));
    // 从父进程管道读取 "ping" 数据
    read(f2c[0], buf, sizeof(buf));
    // 输出收到的信息
    printf("%d: received ping from pid %d\n", getpid(), parent_pid);

    // 向父进程发送 "pong" 数据
    write(c2f[1], "pong", 5);

    // 关闭管道，释放资源
    close(f2c[0]);  // 关闭父进程到子进程管道的读端
    close(c2f[1]);  // 关闭子进程到父进程管道的写端
  } else {
    // 父进程代码执行区域
    close(f2c[0]);  // 关闭父进程到子进程管道的读端，因为父进程只需要写
    close(c2f[1]);  // 关闭子进程到父进程管道的写端，因为父进程只需要读

    int parent_pid = getpid(); // 获取父进程的 PID
    // 向子进程发送父进程的 PID
    write(f2c[1], &parent_pid, sizeof(parent_pid));
    // 向子进程发送 "ping" 数据
    write(f2c[1], "ping", 5);

    char buf[5]; // 用于接收 "pong" 的缓冲区
    // 从子进程管道读取 "pong" 数据
    read(c2f[0], buf, sizeof(buf));
    // 输出收到的信息
    printf("%d: received pong from pid %d\n", getpid(), pid);

    // 关闭管道，释放资源
    close(f2c[1]);  // 关闭父进程到子进程管道的写端
    close(c2f[0]);  // 关闭子进程到父进程管道的读端
  }

  // 正常退出程序
  exit(0);
}
