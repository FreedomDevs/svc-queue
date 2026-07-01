#include "lib.h"
#include <sys/syscall.h>

#define PORT 8080

int send_to_all(char *service_name, char *body, size_t body_size) {
  print(&STDOUT_IO, service_name, ":");
  print_array(&STDOUT_IO, body, body_size);
  print(&STDOUT_IO, _endl);
  return 0;
}

[[noreturn]] int main(int argc, char **argv) {
  int server_fd;

  server_fd = syscall(SYS_socket, AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = 0x901F;
  addr.sin_addr = 0;

  int reuse = 1;
  syscall(SYS_setsockopt, server_fd, SOL_SOCKET, SO_REUSEADDR, (long)&reuse,
          sizeof(reuse));

  int bind_res = syscall(SYS_bind, server_fd, (long)&addr, sizeof(addr));
  if (bind_res < 0) {
    print(&STDERR_IO, "Не удалось порт послушать: ", _errno, _endl);
    exit(1);
  }

  int listen_res = syscall(SYS_listen, server_fd, 128, 0);
  if (listen_res < 0) {
    print(&STDERR_IO, "Не удалось порт послушать: ", _errno, _endl);
    exit(1);
  }

  WRITE_LITERAL(STDERR_FILENO, "Listening 0.0.0.0:8080\n");
  while (1) {
    int client_fd = syscall(SYS_accept, server_fd, 0, 0);
    if (client_fd < 0)
      continue;

    long bytes_read = read(client_fd, STDIN_IO.buf, STDIN_IO.size);
    if (bytes_read < 0) {
      print(&STDERR_IO, "Не удалось сделать read чета");
      syscall(SYS_close, client_fd, 0, 0);
      continue;
    }

    char counted = 0;
    char *body_pos = NULL;
    char *url_start = NULL;
    bool url_found = false;
    char *end_pos = STDIN_IO.buf + bytes_read;
    for (char *pos = STDIN_IO.buf; pos < end_pos; pos++) {
      if (counted == 0 && *pos == '\r')
        counted = 1;
      else if (counted == 1 && *pos == '\n')
        counted = 2;
      else if (counted == 2 && *pos == '\r')
        counted = 3;
      else if (counted == 3 && *pos == '\n') {
        body_pos = pos + 1;
        break;
      } else if (counted > 0)
        counted = 0;

      if (url_start == 0 && *pos == ' ') {
        *pos = '\0';
        url_start = pos + 1;
      } else if (!url_found && *pos == ' ') {
        *pos = '\0';
        url_found = true;
      }
    }

    if (!url_found || (url_start == NULL) || (body_pos == NULL) ||
        (url_start[0] != '/')) {
      const char response[] =
          "HTTP/1.1 400 OK\r\nContent-Length: 4\r\n\r\nHui\n";
      write(client_fd, response, sizeof(response) - 1);
      syscall(SYS_close, client_fd, 0, 0);
      continue;
    }

    if (STDIN_IO.buf[0] == 'P' && STDIN_IO.buf[1] == 'O' &&
        STDIN_IO.buf[2] == 'S' && STDIN_IO.buf[3] == 'T' &&
        STDIN_IO.buf[4] == '\0') {
      char *service_name = url_start + 1;

      send_to_all(service_name, body_pos, end_pos - body_pos);
    }

    const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    write(client_fd, response, sizeof(response) - 1);
    syscall(SYS_close, client_fd, 0, 0);
  }

  exit(0);
}
