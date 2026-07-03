#ifndef _HYPERBOX_LIB_H
#define _HYPERBOX_LIB_H

#ifdef __ANDROID__
#include "lib/android.h"
#else
#include "nolibc/nolibc.h"
#endif

#include <sys/syscall.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Buffers sizes
#define STDOUT_WRITEBUFFER_SIZE 32768
#define STDERR_WRITEBUFFER_SIZE 4096
#define STDIN_READBUFFER_SIZE 16384

// Print
typedef struct {
  char *buf;
  size_t size;
  size_t pos;
  int fd;
} io_buffer_t;

static char STDERR_WRITEBUFFER[STDERR_WRITEBUFFER_SIZE];
static io_buffer_t STDERR_IO = {STDERR_WRITEBUFFER, STDERR_WRITEBUFFER_SIZE, 0,
                                STDERR_FILENO};
static char STDOUT_WRITEBUFFER[STDOUT_WRITEBUFFER_SIZE];
static io_buffer_t STDOUT_IO = {STDOUT_WRITEBUFFER, STDOUT_WRITEBUFFER_SIZE, 0,
                                STDOUT_FILENO};

static char STDIN_READBUFFER[STDIN_READBUFFER_SIZE];
static io_buffer_t STDIN_IO = {STDIN_READBUFFER, STDIN_READBUFFER_SIZE, 0, -2};

typedef struct {
  char dummy;
} flush_flag_t;
typedef struct {
  char dummy;
} errno_flag_t;
typedef struct {
  char dummy;
} endl_flag_t;
static const flush_flag_t _flush = {0};
static const errno_flag_t _errno = {0};
static const endl_flag_t _endl = {0};

static inline void print_flush(io_buffer_t *b) {
  if (b->pos == 0)
    return;
  write(b->fd, b->buf, b->pos);
  b->pos = 0;
}
static inline void print_flush_flag(io_buffer_t *b, flush_flag_t flag) {
  (void)flag;
  if (b->pos == 0)
    return;
  write(b->fd, b->buf, b->pos);
  b->pos = 0;
}
static inline void print_array(io_buffer_t *b, const char *arr,
                               unsigned long length) {
  if (b->pos + length > b->size && b->fd == -2) {
    print_array(&STDERR_IO, "Cannot write to buffer: target buffer is full",
                46);
    return;
  }

  if (b->pos == b->size) {
    write(b->fd, b->buf, b->size);
    b->pos = 0;
  }

  while (b->pos + length > b->size) {
    unsigned long tocopy = b->size - b->pos;
    memcpy(b->buf, arr, tocopy);
    write(b->fd, b->buf, b->size);
    b->pos = 0;
  }

  memcpy(b->buf + b->pos, arr, length);
  b->pos += length;
}
static inline void print_endl_flag(io_buffer_t *b, endl_flag_t flag) {
  (void)flag;
  print_array(b, "\n", 1);
  write(b->fd, b->buf, b->pos);
  b->pos = 0;
}
static inline long print_fd_to_end(io_buffer_t *b, int fd) {
  long ret;
  while ((ret = read(fd, b->buf + b->pos, b->size - b->pos)) > 0) {
    b->pos += ret;

    if (b->pos == b->size) {
      write(b->fd, b->buf, b->pos);
      b->pos = 0;
    }
  }

  return ret;
}

static inline void print_string(io_buffer_t *b, const char *X) {
  print_array(b, X, strlen(X));
}
static inline void print_char(io_buffer_t *b, char X) { print_array(b, &X, 1); }
static inline void print_long(io_buffer_t *b, long num) {
  print_string(b, itoa(num));
}
static inline void print_errno_formatted(io_buffer_t *b, errno_flag_t flag) {
  (void)flag;
  bool unknown = false;
  const char *msg;

  switch (errno) {
  case EACCES:
    msg = "Permission denied";
    break;
  case ENOENT:
    msg = "No such file or directory";
    break;
  case EEXIST:
    msg = "File exists";
    break;
  case EINTR:
    msg = "Interrupted system call";
    break;
  case EIO:
    msg = "I/O error";
    break;
  case EISDIR:
    msg = "Is a directory";
    break;
  case ENOTDIR:
    msg = "Not a directory";
    break;
  case ENOMEM:
    msg = "Out of memory";
    break;
  case EBUSY:
    msg = "Device or resource busy";
    break;
  case EPERM:
    msg = "Operation not permitted";
    break;
  default:
    msg = "Unknown errno = ";
    unknown = true;
    break;
  }

  print_string(b, msg);
  if (unknown)
    print_long(b, errno);
}

#define is_static_string(X)                                                    \
  (__builtin_constant_p(X) &&                                                  \
   (__builtin_types_compatible_p(__typeof__(X), const char[]) ||               \
    __builtin_types_compatible_p(__typeof__(X), char[])))

#define print_any(b, X)                                                        \
  do {                                                                         \
    __builtin_choose_expr(                                                     \
        is_static_string(X),                                                   \
        print_array(                                                           \
            (b),                                                               \
            (const char *)__builtin_choose_expr(is_static_string(X), (X), ""), \
            sizeof(__builtin_choose_expr(is_static_string(X), (X), "")) - 1),  \
        _Generic((X),                                                          \
            flush_flag_t: print_flush_flag,                                    \
            errno_flag_t: print_errno_formatted,                               \
            endl_flag_t: print_endl_flag,                                      \
            const char *: print_string,                                        \
            char *: print_string,                                              \
            char: print_char,                                                  \
            long: print_long,                                                  \
            int: print_long,                                                   \
            unsigned long: print_long,                                         \
            uint32_t: print_long,                                              \
            uint16_t: print_long,                                              \
            int8_t: print_long,                                                \
            uint8_t: print_long)((b), (X)));                                   \
  } while (0)

#define print1(b, X1) print_any(b, X1)
#define print2(b, X1, X2)                                                      \
  print_any(b, X1);                                                            \
  print_any(b, X2)
#define print3(b, X1, X2, X3)                                                  \
  print_any(b, X1);                                                            \
  print_any(b, X2);                                                            \
  print_any(b, X3)
#define print4(b, X1, X2, X3, X4)                                              \
  print_any(b, X1);                                                            \
  print_any(b, X2);                                                            \
  print_any(b, X3);                                                            \
  print_any(b, X4)
#define print5(b, X1, X2, X3, X4, X5)                                          \
  print_any(b, X1);                                                            \
  print_any(b, X2);                                                            \
  print_any(b, X3);                                                            \
  print_any(b, X4);                                                            \
  print_any(b, X5)
#define print6(b, X1, X2, X3, X4, X5, X6)                                      \
  print_any(b, X1);                                                            \
  print_any(b, X2);                                                            \
  print_any(b, X3);                                                            \
  print_any(b, X4);                                                            \
  print_any(b, X5);                                                            \
  print_any(b, X6);
#define print7(b, X1, X2, X3, X4, X5, X6, X7)                                  \
  print_any(b, X1);                                                            \
  print_any(b, X2);                                                            \
  print_any(b, X3);                                                            \
  print_any(b, X4);                                                            \
  print_any(b, X5);                                                            \
  print_any(b, X6);                                                            \
  print_any(b, X7);
#define print8(b, X1, X2, X3, X4, X5, X6, X7, X8)                              \
  print_any(b, X1);                                                            \
  print_any(b, X2);                                                            \
  print_any(b, X3);                                                            \
  print_any(b, X4);                                                            \
  print_any(b, X5);                                                            \
  print_any(b, X6);                                                            \
  print_any(b, X7);                                                            \
  print_any(b, X8);

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
#define print(b, ...)                                                          \
  GET_MACRO(__VA_ARGS__, print8, print7, print6, print5, print4, print3,       \
            print2, print1)(b, __VA_ARGS__)

// Utils
#define WRITE_LITERAL(fd, str_literal)                                         \
  write(fd, "" str_literal, sizeof(str_literal) - 1)

#define STR_TO_UINT64(str) (*(const uint64_t *)(str "\0\0\0\0\0\0\0\0"))

static inline const char *get_basename(const char *path) {
  const char *base = path;
  while (*path) {
    if (*path == '/')
      base = path + 1;
    path++;
  }
  return base;
}

static inline char val_to_hex(uint8_t val) {
  if (val < 10)
    return '0' + val;
  return 'A' + (val - 10);
}

static inline int hex_to_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

#define HTONS(n) ((((n) & 0xFF) << 8) | (((n) & 0xFF00) >> 8))

#define AF_INET 2
#define SOCK_STREAM 1

#define SOL_SOCKET 1
#define SOL_TCP 6
#define SO_REUSEADDR 2

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define TCP_CORK 3

#define TCP_FASTOPEN 23

#define MSG_NOSIGNAL 16384
#define MSG_MORE 32768

struct sockaddr_in {
  uint16_t sin_family;
  uint16_t sin_port;
  uint32_t sin_addr;
  uint8_t sin_zero[8];
};

#endif
