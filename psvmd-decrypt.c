/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "pthread_win32.h"
#define pipe(fds) _pipe(fds, 0x100000, O_BINARY)
#else
#include <pthread.h>
#include <unistd.h>
#define O_BINARY 0
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include "restore.h"
#include "utils.h"

int main(int argc, const char *argv[]) {
  args_t args1, args2;
  struct stat st;
  int fds[2];
  pthread_t pth1, pth2;

  if (argc < 5) {
    fprintf(stderr, "usage: psvmd-decrypt [-A aid | -K key] input.psvmd output\n");
    return 1;
  }

  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe\n");
    return 1;
  }

  args1.in = open(argv[3], O_RDONLY | O_BINARY);
  if (args1.in < 0) {
    fprintf(stderr, "input\n");
    return 1;
  }
  args1.out = fds[1];
  args2.in = fds[0];
  args2.out = open(argv[4], O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, 0644);
  if (args2.out < 0) {
    fprintf(stderr, "output\n");
    return 1;
  }

  if (strcmp(argv[1], "-K") == 0) {
    if (parse_key(argv[2], args1.key) < 0) {
      fprintf(stderr, "invalid key\n");
      return 1;
    }
  } else if (strcmp(argv[1], "-A") == 0) {
    if (generate_key_from_aid(argv[2], args1.key) < 0) {
      fprintf(stderr, "invalid aid\n");
      return 1;
    }
  } else {
    fprintf(stderr, "you must specify either -A or -K!\n");
    return 1;
  }

  pthread_create(&pth1, NULL, decompress_thread, &args2);
  pthread_create(&pth2, NULL, decrypt_thread, &args1);

  pthread_join(pth2, NULL);
  pthread_join(pth1, NULL);

  fprintf(stderr, "all done.\n");

  return 0;
}
