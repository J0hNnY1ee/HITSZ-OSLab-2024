#include "../kernel/types.h"   // 包含 xv6 操作系统内核类型定义
#include "../kernel/stat.h"    // 包含文件状态相关的定义
#include "user.h"              // 包含用户空间库函数定义
#include "../kernel/fs.h"      // 包含文件系统相关定义

#define BUF_SIZE 512  // 定义缓冲区大小

// 从路径中提取文件或目录的最后一个部分的名称
char* fmtname(char *path) {
  static char buf[DIRSIZ + 1]; // 存储文件名的缓冲区
  char *p;

  // 找到路径中最后一个 '/' 之后的第一个字符
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++; // 移动到 '/' 后的第一个字符

  // 如果文件名长度超过 DIRSIZ，则直接返回
  if (strlen(p) >= DIRSIZ)
    return p;
  // 否则将文件名复制到缓冲区，并确保以 '\0' 结尾
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

// 递归函数，用于在指定路径中查找目标文件或目录
void find(char *path, char *target) {
  char buf[BUF_SIZE], *p;  // 定义缓冲区和指针 p
  int fd;                  // 文件描述符
  struct dirent de;        // 目录项结构体
  struct stat st;          // 文件状态结构体

  // 打开指定的路径
  if ((fd = open(path, 0)) < 0) {
    printf("find: cannot open %s\n", path); // 打开失败，打印错误信息
    return;
  }

  // 获取文件或目录的状态信息
  if (fstat(fd, &st) < 0) {
    printf("find: cannot stat %s\n", path); // 获取状态失败，打印错误信息
    close(fd);
    return;
  }

  // 根据文件类型进行处理
  switch (st.type) {
    case T_FILE: // 如果是文件类型
      // 判断文件名是否与目标名称匹配
      if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path); // 匹配则打印相对路径
      }
      break;

    case T_DIR: // 如果是目录类型
      // 检查目录名是否与目标名称匹配
      if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path); // 目录名匹配则打印相对路径
      }

      // 防止路径过长导致缓冲区溢出
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);     // 复制当前路径到缓冲区
      p = buf + strlen(buf); // 指针 p 指向路径末尾
      *p++ = '/';            // 在路径末尾添加 '/'

      // 读取目录中的每个条目
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) // 如果条目无效则跳过
          continue;

        // 跳过 "." 和 ".." 目录，以避免递归回溯
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
          continue;

        // 构建新的完整路径
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // 获取新路径的状态信息
        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }

        // 递归搜索子目录
        find(buf, target);
      }
      break;
  }
  close(fd); // 关闭文件描述符
}

// 程序入口点
int main(int argc, char *argv[]) {
  // 检查命令行参数是否正确
  if (argc != 3) {
    printf("Usage: find <path> <name>\n"); // 打印正确用法
    exit(1);
  }

  // 调用 find 函数从给定路径查找目标名称
  find(argv[1], argv[2]);
  exit(0); // 正常退出
}
