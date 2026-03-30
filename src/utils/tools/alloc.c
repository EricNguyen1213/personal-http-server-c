#include "alloc.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

void* safe_malloc(size_t size) {
  void* ptr = malloc(size);
  if (ptr == NULL) {
    printf("Failed to Allocate: %s \n", strerror(errno));
    exit(1);
  }
  return ptr;
}

void* safe_realloc(void* ptr, size_t size) {
  void* temp = realloc(ptr, size);
  if (temp == NULL) {
    printf("Failed to Reallocate: %s \n", strerror(errno));
    exit(1);
  }
  return temp;
}
