#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fs.h"

#define BUF_SIZE 512


// Extract the last component of the path
char* fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after the last '/'
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return the name with null-termination
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

// Recursive function to search for target file or directory
void find(char *path, char *target) {
  char buf[BUF_SIZE], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // Open the directory or file
  if ((fd = open(path, 0)) < 0) {
    printf("find: cannot open %s\n", path);
    return;
  }

  // Get the statistics of the file or directory
  if (fstat(fd, &st) < 0) {
    printf("find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_FILE:
      // If it's a file and the name matches, print the relative path
      if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path);
      }
      break;

    case T_DIR:
      // If it's a directory, check if the name matches and print the relative path
      if (strcmp(fmtname(path), target) == 0) {
        printf("%s\n", path);  // Directory matches target name
      }

      // If it's a directory, recursively search inside
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';

      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
          continue;

        // Skip "." and ".."
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
          continue;

        // Build the new path
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }

        // Recursively search in subdirectories
        find(buf, target);
      }
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: find <path> <name>\n");
    exit(1);
  }

  find(argv[1], argv[2]);
  exit(0);
}
