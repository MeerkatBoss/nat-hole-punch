#include "Server.hpp"
#include "Common.hpp"

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


namespace stun {

static int make_socket(uint8_t ip_address[4], uint16_t port);

static void interrupt_handler(int) { /* Enter handler but do nothing */ }
static void setup_interrupt_handler() {
  int res = 0;
  struct sigaction action;
  action.sa_handler = &interrupt_handler;
  action.sa_flags = 0;
  res = sigemptyset(&action.sa_mask);
  assert(res == 0);

  res = sigaction(SIGINT, &action, NULL);
  assert(res == 0);
}

void run_server(uint8_t ip_addr[4], uint16_t port) {
  int socket = make_socket(ip_addr, port);
  setup_interrupt_handler();
  printf("Server listening on %hhu.%hhu.%hhu.%hhu:%hu\n",
      ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3], port);

  for (;;) {
    ServerRequest requests[2];
    struct sockaddr_in addrs[2];

    bool interrupted = false;
    bool error = false;
    for (size_t i = 0; i < 2; ++i) {
      socklen_t addr_len = sizeof(struct sockaddr_in);
      ssize_t res = recvfrom(
          socket,
          requests + i,
          sizeof(ServerRequest),
          MSG_TRUNC,
          (struct sockaddr*) addrs + i,
          &addr_len
      );
      
      if (res == -1 && errno == EINTR) {
        interrupted = true;
        break;
      }

      if (res < 0) {
        perror("Err in recv");
        error = true;
        break;
      }
      
      if (res != sizeof(ServerRequest)) {
        fputs("Invalid server request\n", stderr);
        error = true;
        break;
      }
    }

    if (error) {
      continue;
    }

    if (interrupted) {
      break;
    }

    if (requests[0].conn_id != requests[1].conn_id) {
      fputs("Connection ids do not match\n", stderr);
      break;
    }

    for (size_t i = 0; i < 2; ++i) {
      ServerResponse response;

      memcpy(&response.local_addr, &requests[1 - i].local_addr, sizeof(in_addr));
      response.local_port = requests[1 - i].local_port;

      memcpy(&response.global_addr, &addrs[1 - i].sin_addr, sizeof(in_addr));
      response.global_port = addrs[1 - i].sin_port;

      ssize_t res = sendto(
          socket,
          &response,
          sizeof(ServerResponse),
          0,
          (const struct sockaddr*) addrs + i,
          sizeof(struct sockaddr_in)
      );

      if (res == -1 && errno == EINTR) {
        interrupted = true;
        break;
      }

      if (res < -1) {
        perror("Err in send");
        error = true;
        break;
      }
    }

    if (interrupted) {
      break;
    }

    if (error) {
      continue;
    }
  }

  puts("");
  puts("Server stopped!");
}

static int make_socket(uint8_t ip_address[4], uint16_t port) {
  constexpr size_t IpAddrMaxLength = 12 + 3;  // 12 digits and 3 dots
  static char addr_buffer[IpAddrMaxLength + 1] = "";
  int res = 0;

  snprintf(addr_buffer, IpAddrMaxLength,
      "%hhd.%hhd.%hhd.%hhd",
      ip_address[0], ip_address[1], ip_address[2], ip_address[3]
  );

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = ntohl(port);
  res = inet_aton(addr_buffer, &address.sin_addr);
  assert(res == 1);

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  assert(fd >= 0);
  res = bind(fd, (const struct sockaddr*) &address, sizeof(address));
  assert(res == 0);

  return fd;
}

}
