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
#include <direct.h>
#define mkdir(n, m) _mkdir(n)
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
    fprintf(stderr, "usage: psvimg-extract [-A aid | -K key] input.psvimg outputdir\n");
    return 1;
  }

  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe\n");
    return 1;
  }

  args1.in = open(argv[3], O_RDONLY | O_BINARY);
  if (args1.in < 0) {
    perror("open");
    return 1;
  }
  args1.out = fds[1];
  args2.in = fds[0];
  args2.out = 0;

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

  args2.prefix = argv[4];
  if (stat(args2.prefix, &st) < 0) {
    mkdir(args2.prefix, 0700);
  }

  pthread_create(&pth1, NULL, unpack_thread, &args2);
  pthread_create(&pth2, NULL, decrypt_thread, &args1);

  pthread_join(pth2, NULL);
  pthread_join(pth1, NULL);

  fprintf(stderr, "all done.\n");

  return 0;
}
