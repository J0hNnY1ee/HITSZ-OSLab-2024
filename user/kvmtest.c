#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
  printf("kvmtest: start\n");
  if (checkvm()) {
    printf("kvmtest: OK\n");
  } else {
    printf("kvmtest: FAIL Kernel is using global kernel pagetable\n");
  }
  exit(0);
}
