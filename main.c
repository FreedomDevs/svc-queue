#include "hashtable.h"
#include "mikinolibc/lib.h"

#ifndef PORT
#define PORT 8080
#endif

noreturn int main(int argc, char **argv) {
  init_hashtable();

  int server_fd = syscall(SYS_socket, AF_INET6, SOCK_STREAM, 0);
  if (server_fd < 0) {
    print(&STDERR_IO, "Socket failed: ", _errno, _endl);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in6 addr = {0};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = HTONS(PORT);
  addr.sin6_addr = in6_addr_any;

  int reuse = 1;
  syscall(SYS_setsockopt, server_fd, SOL_SOCKET, SO_REUSEADDR, (long)&reuse,
          sizeof(reuse));

  int bind_res = syscall(SYS_bind, server_fd, (long)&addr, sizeof(addr));
  if (bind_res < 0) {
    print(&STDERR_IO, "Не удалось порт послушать: ", _errno, _endl);
    exit(1);
  }

  int qlen = 50;
  syscall(SYS_setsockopt, server_fd, SOL_TCP, TCP_FASTOPEN, &qlen,
          sizeof(qlen));

  int listen_res = syscall(SYS_listen, server_fd, 128, 0);
  if (listen_res < 0) {
    print(&STDERR_IO, "Не удалось порт послушать: ", _errno, _endl);
    exit(1);
  }

  print(&STDERR_IO, "Listening [::]:", PORT, "\n");
  while (1) {
    int client_fd = syscall(SYS_accept, server_fd, 0, 0);
    if (client_fd < 0)
      continue;

    print(&STDOUT_IO, "Accepted client with fd: ", client_fd, _endl);

    long bytes_read = read(client_fd, STDIN_IO.buf, STDIN_IO.size) > 0;
    if (bytes_read < 0) {
      print(&STDOUT_IO, "Cannot read from client ", client_fd,
            " with error: ", _errno, _endl);
      close(client_fd);
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
      print(&STDOUT_IO, "Client sent bad request: ", client_fd, _endl);
      const char response[] = "HTTP/1.1 400 OK\r\nContent-Length: 0\r\n\r\n";
      syscall(SYS_sendto, client_fd, response, sizeof(response) - 1,
              MSG_NOSIGNAL | MSG_MORE, 0, 0);
      close(client_fd);
      continue;
    }

    if (STDIN_IO.buf[0] == 'P' && STDIN_IO.buf[1] == 'O' &&
        STDIN_IO.buf[2] == 'S' && STDIN_IO.buf[3] == 'T' &&
        STDIN_IO.buf[4] == '\0') {
      char *service_name = url_start + 1;

      print(&STDOUT_IO, "Client ", client_fd,
            " sent event for service: ", service_name, " connection closed",
            _endl);
      const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
      syscall(SYS_sendto, client_fd, response, sizeof(response) - 1,
              MSG_NOSIGNAL | MSG_MORE, 0, 0);
      close(client_fd);

      send_to_all(service_name, body_pos, end_pos - body_pos);
      continue;
    }

    if (STDIN_IO.buf[0] == 'G' && STDIN_IO.buf[1] == 'E' &&
        STDIN_IO.buf[2] == 'T' && STDIN_IO.buf[3] == '\0') {
      char *service_name = url_start + 1;

      char *old_start = service_name;
      for (int i = 0;; i++) {
        if (service_name[i] == ',' || service_name[i] == '\0') {
          subscribe(client_fd, old_start);

          print(&STDOUT_IO, "Client (", client_fd, ") subscribed to service ");
          print_array(&STDOUT_IO, service_name, i);
          print_char(&STDOUT_IO, '\n');

          service_name[i] = '\0';
          i++;
          old_start = service_name + i;
        }

        if (service_name[i] == '\0')
          break;
      }

      const char response[] =
          "HTTP/1.1 200 OK\r\nContent-Type: "
          "text/event-stream\r\nCache-Control: no-store\r\n\r\n";
      syscall(SYS_sendto, client_fd, response, sizeof(response) - 1,
              MSG_NOSIGNAL, 0, 0);
      print(&STDOUT_IO, "Established long live connection with fd: ", client_fd,
            _endl);
      continue;
    }

    print(&STDOUT_IO, "Client sent bad request: ", client_fd, _endl);
    const char response[] = "HTTP/1.1 400 OK\r\nContent-Length: 0\r\n\r\n";
    syscall(SYS_sendto, client_fd, response, sizeof(response) - 1,
            MSG_NOSIGNAL | MSG_MORE, 0, 0);
    close(client_fd);
  }

  exit(0);
}
