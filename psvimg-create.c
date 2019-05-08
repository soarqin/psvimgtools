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
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "backup.h"
#include "psvimg.h"
#include "utils.h"

#define MAX_PATH_LEN 1024

static int is_backup(const char *title) {
  if (strcmp(title, "app") == 0 ||
      strcmp(title, "patch") == 0 ||
      strcmp(title, "addcont") == 0 ||
      strcmp(title, "savedata") == 0 ||
      strcmp(title, "appmeta") == 0 ||
      strcmp(title, "license") == 0 ||
      strcmp(title, "game") == 0) {
    return 0;
  } else {
    return 1;
  }
}

int main(int argc, const char *argv[]) {
  args_t args1, args2;
  struct stat st;
  int fds[4];
  char path[MAX_PATH_LEN];
  int mfd;
  PsvMd_t md;
  pthread_t pth1, pth2;

  if (argc < 7) {
    fprintf(stderr, "usage: psvimg-create [-m metadata|-n name] [-A aid | -K key] inputdir outputdir\n");
    fprintf(stderr, "  specify either a decrypted metadata file as a template or\n");
    fprintf(stderr, "  a name and other metadata fields will retain default values\n");
    return 1;
  }

  // TODO: support more types
  if (strcmp(argv[1], "-m") == 0) {
    mfd = open(argv[2], O_RDONLY | O_BINARY);
    if (mfd < 0) {
      fprintf(stderr, "metadata\n");
      return 1;
    }
    if (read_block(mfd, &md, sizeof(md) - sizeof(md.add_data)) < sizeof(md) - sizeof(md.add_data)) {
      fprintf(stderr, "invalid metadata size\n");
      return 1;
    }
    if (md.type != 2) {
      fprintf(stderr, "metadata type not supported\n");
      close(mfd);
      return 1;
    }
    if (read_block(mfd, &md.add_data, sizeof(md.add_data)) < sizeof(md.add_data)) {
      fprintf(stderr, "invalid metadata size\n");
      close(mfd);
      return 1;
    }
    close(mfd);
  } else if (strcmp(argv[1], "-n") == 0) {
    memset(&md, 0, sizeof(md));
    strncpy(md.name, argv[2], sizeof(md.name));
    md.magic = is_backup(md.name) ? PSVMD_BACKUP_MAGIC : PSVMD_CONTENT_MAGIC;
    md.type = 2;
    md.version = 2;
    md.add_data = 1;
    srand(time(NULL));
    for (int i = 0; i < sizeof(md.iv); i++) {
      md.iv[i] = rand() % 0xFF;
    }
  } else {
    fprintf(stderr, "you must specify either -m or -n!\n");
    return 1;
  }

  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe 1\n");
    return 1;
  }

  args1.in = 0;
  args1.prefix = argv[5];
  args1.content_size = 0;
  args1.out = fds[1];

  if (stat(argv[6], &st) < 0) {
    mkdir(argv[6], 0700);
  }

  snprintf(path, sizeof(path), "%s/%s.psvimg", argv[6], md.name);

  args2.out = open(path, O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, 0644);
  if (args2.out < 0) {
    fprintf(stderr, "psvimg output\n");
    return 1;
  }
  args2.in = fds[0];

  if (strcmp(argv[3], "-K") == 0) {
    if (parse_key(argv[4], args2.key) < 0) {
      fprintf(stderr, "invalid key\n");
      return 1;
    }
  } else if (strcmp(argv[3], "-A") == 0) {
    if (generate_key_from_aid(argv[4], args2.key) < 0) {
      fprintf(stderr, "invalid aid\n");
      return 1;
    }
  } else {
    fprintf(stderr, "you must specify either -A or -K!\n");
    return 1;
  }

  memcpy(args2.iv, md.iv, sizeof(args2.iv));

  pthread_create(&pth1, NULL, encrypt_thread, &args2);
  pthread_create(&pth2, NULL, pack_thread, &args1);

  pthread_join(pth2, NULL);
  pthread_join(pth1, NULL);

  if (stat(path, &st) < 0) {
    fprintf(stderr, "stat\n");
    return 1;
  }
  fprintf(stderr, "created %s (size: %llx, content size: %zx)\n", path, st.st_size, args1.content_size);
  md.total_size = args1.content_size;
  md.psvimg_size = st.st_size;

  // now create the psvmd
  snprintf(path, sizeof(path), "%s/%s.psvmd", argv[6], md.name);

  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe 2\n");
    return 1;
  }
  if (pipe(&fds[2]) < 0) {
    fprintf(stderr, "pipe 3\n");
    return 1;
  }

  args1.in = fds[0];
  args1.out = fds[3];

  args2.in = fds[2];
  args2.out = open(path, O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, 0644);
  if (args2.out < 0) {
    fprintf(stderr, "psvmd output\n");
    return 1;
  }
  for (int i = 0; i < sizeof(md.iv); i++) {
    args2.iv[i] = rand() % 0xFF;
  }

  pthread_create(&pth1, NULL, encrypt_thread, &args2);
  pthread_create(&pth2, NULL, compress_thread, &args1);
  write_block(fds[1], &md, sizeof(md));
  close(fds[1]);

  pthread_join(pth2, NULL);
  pthread_join(pth1, NULL);

  
  fprintf(stderr, "created %s\n", path);

  // finally create the psvinf
  if (is_backup(md.name)) {
    snprintf(path, sizeof(path), "%s/%s.psvinf", argv[6], md.name);
    mfd = open(path, O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, 0644);
    write_block(mfd, md.name, strnlen(md.name, 64) + 1);
    close(mfd);
    fprintf(stderr, "created %s\n", path);
  }

  return 0;
}
