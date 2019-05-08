/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include "utils.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#define COPY_BLOCK_SIZE (1024)

#include "crypto.h"
void cma_keygen(const uint8_t *input, uint8_t output[32]) {
  static const uint8_t passphrase[] = "Sri Jayewardenepura Kotte";
  static const uint8_t key[] = {0xA9,0xFA,0x5A,0x62,0x79,0x9F,0xCC,0x4C,0x72,0x6B,0x4E,0x2C,0xE3,0x50,0x6D,0x38};
  sha256_context ctx;
  aes_context aes;
  sha256_init(&ctx);
  sha256_starts(&ctx);
  sha256_update(&ctx, input, 8);
  sha256_update(&ctx, passphrase, 25);
  sha256_final(&ctx, output);
  aes_init_dec(&aes, key, 128);
  aes_ecb_decrypt(&aes, output, output);
  aes_ecb_decrypt(&aes, output + 16, output + 16);
}

ssize_t read_block(int fd, void *buf, size_t nbyte) {
  ssize_t rd;
  size_t total;
  total = 0;
  while ((rd = read(fd, buf, nbyte)) > 0) {
    nbyte -= rd;
    buf = (char *)buf + rd;
    total += rd;
  }
  if (rd < 0) {
    return rd;
  } else {
    return total;
  }
}

ssize_t write_block(int fd, const void *buf, size_t nbyte) {
  ssize_t wr;
  size_t total;
  total = 0;
  while ((wr = write(fd, buf, nbyte)) > 0) {
    nbyte -= wr;
    buf = (char *)buf + wr;
    total += wr;
  }
  if (wr < 0) {
    return wr;
  } else {
    return total;
  }
}

ssize_t copy_block(int fd_out, int fd_in, size_t nbyte) {
  char buffer[COPY_BLOCK_SIZE];
  ssize_t rd;
  size_t total;

  total = 0;
  while (nbyte > 0 && (rd = read_block(fd_in, buffer, nbyte > sizeof(buffer) ? sizeof(buffer) : nbyte)) > 0) {
    if (write_block(fd_out, buffer, rd) < rd) {
      return -1;
    }
    total += rd;
    nbyte -= rd;
  }
  if (rd < 0) {
    return rd;
  } else {
    return total;
  }
}

int parse_key(const char *ascii, uint8_t key[0x20]) {
  int i;
  for (i = 0; i < 0x20; i++) {
    char byte[3];
    memcpy(byte, &ascii[2*i], 2);
    byte[2] = '\0';
    key[i] = strtol(byte, NULL, 16);
    if (key[i] == 0 && !(byte[0] == '0' && byte[1] == '0')) {
      return -1;
    }
  }
  if (ascii[2*i] != '\0') {
    return -1;
  } else {
    return 0;
  }
}

int generate_key_from_aid(const char *account_id, uint8_t key[0x20]) {
  int i;
  uint8_t aid[8];
  for (i = 0; i < 0x08; i++) {
    char byte[3];
    memcpy(byte, &account_id[2*i], 2);
    byte[2] = '\0';
    aid[i] = strtol(byte, NULL, 16);
    if (aid[i] == 0 && !(byte[0] == '0' && byte[1] == '0')) {
      return -1;
    }
  }
  if (account_id[2*i] != '\0') {
    return -1;
  }
  cma_keygen(aid, key);
  return 0;
}
