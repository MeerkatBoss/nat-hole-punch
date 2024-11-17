#include "Client.hpp"
#include "Common.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace stun {

sockaddr_in convert_addr(uint8_t addr[4], uint16_t port);

int make_socket(sockaddr_in addr);

void set_timeout(int socket, size_t timeout_ms);

int receive_from_addr(int socket, void* buf, size_t buf_size, sockaddr_in addr);


struct ClientConfig {
  sockaddr_in peer_addr;
  ClientMessageType message_type;
};

int probe_peer(int socket, sockaddr_in peer, ClientConfig* config);
ClientConfig connect_to_peer(int socket, sockaddr_in self, sockaddr_in server);

void run_ping(int socket, sockaddr_in peer);
void run_pong(int socket, sockaddr_in peer);

void run_client(
    uint8_t my_addr[4],
    uint16_t my_port,
    uint8_t server_addr[4],
    uint16_t server_port
) {
  sockaddr_in self = convert_addr(my_addr, my_port);
  sockaddr_in server = convert_addr(server_addr, server_port);
  
  int socket = make_socket(self);

  ClientConfig config = connect_to_peer(socket, self, server);
  if (config.message_type == ClientMessageType::PING) {
    run_ping(socket, config.peer_addr);
    return;
  }
  if (config.message_type == ClientMessageType::PONG) {
    run_pong(socket, config.peer_addr);
    return;
  }
  assert(0 && "Unreachable");
}

sockaddr_in convert_addr(uint8_t addr[4], uint16_t port) {
  constexpr size_t IpAddrMaxLength = 12 + 3;  // 12 digits and 3 dots
  static char addr_buffer[IpAddrMaxLength + 1] = "";
  int res = 0;

  snprintf(addr_buffer, IpAddrMaxLength,
      "%hhd.%hhd.%hhd.%hhd",
      addr[0], addr[1], addr[2], addr[3]
  );

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = ntohs(port);
  res = inet_aton(addr_buffer, &address.sin_addr);
  assert(res == 1);

  return address;
}

int make_socket(sockaddr_in addr) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  assert(fd >= 0);
  int res = bind(fd, (const struct sockaddr*) &addr, sizeof(addr));
  assert(res == 0);

  return fd;
}

void set_timeout(int socket, size_t timeout_ms) {
  timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;

  int res = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  assert(res >= 0);
}

ClientConfig connect_to_peer(int socket, sockaddr_in self, sockaddr_in server) {
  ServerRequest request;
  request.conn_id = 0xDEADBEEF;
  memcpy(&request.local_addr, &self.sin_addr, sizeof(self.sin_addr));
  request.local_port = self.sin_port;

  int res = sendto(
      socket,
      &request,
      sizeof(request),
      0,
      (const struct sockaddr*) &server,
      sizeof(server)
  );
  if (res < 0) {
    perror("Err in send to server");
    assert(0);
  }

  ServerResponse response;
  res = receive_from_addr(socket, &response, sizeof(response), server);
  if (res < 0) {
    perror("Recv from server");
  }
  assert(res == sizeof(response));

  sockaddr_in local_peer;
  local_peer.sin_family = AF_INET;
  memcpy(&local_peer.sin_addr, &response.local_addr, sizeof(response.local_addr));
  local_peer.sin_port = response.local_port;

  ClientConfig conf;
  res = probe_peer(socket, local_peer, &conf);
  if (res < 0) {
    perror("Recv from local peer");
    assert(0);
  }
  if (res == 1) {
    return conf;
  }
  
  sockaddr_in global_peer;
  global_peer.sin_family = AF_INET;
  memcpy(&global_peer.sin_addr, &response.global_addr, sizeof(response.global_addr));
  global_peer.sin_port = response.global_port;

  res = probe_peer(socket, global_peer, &conf);
  if (res < 0) {
    perror("Recv from global peer");
    assert(0);
  }

  assert(res == 1);
  return conf;
}

uint64_t get_random() {
  int rand_fd = open("/dev/urandom", O_RDONLY);
  uint64_t val = 0;
  read(rand_fd, &val, sizeof(val));
  close(rand_fd);
  return val;
}

int probe_peer(int socket, sockaddr_in peer, ClientConfig* config) {
  ClientHello message;
  message.type = ClientMessageType::HELLO_1;
  message.random = get_random();

  int res = sendto(
      socket,
      &message,
      sizeof(message),
      0,
      (const struct sockaddr*) &peer,
      sizeof(peer)
  );
  if (res < 0) {
    return 0;
  }
  ClientHello peer_response;
  set_timeout(socket, 500);
  res = receive_from_addr(socket, &peer_response, sizeof(peer_response), peer);
  if (res < 0 && errno == EAGAIN) {
    return 0;
  }
  if (res < 0 && errno != EAGAIN) {
    return res;
  }
  set_timeout(socket, 0);

  assert(res == sizeof(peer_response));
  if (peer_response.type == ClientMessageType::HELLO_2) {
    config->message_type = ClientMessageType::PONG;
    config->peer_addr = peer;
    return 1;
  }
  assert(peer_response.type == ClientMessageType::HELLO_1);

  if (peer_response.random > message.random) {
    message.type = ClientMessageType::HELLO_2;
    res = sendto(
        socket,
        &message,
        sizeof(message),
        0,
        (const struct sockaddr*) &peer,
        sizeof(peer)
    );
    assert(res > 0);
    config->message_type = ClientMessageType::PING;
    config->peer_addr = peer;
    return 1;
  }
  
  res = receive_from_addr(socket, &peer_response, sizeof(peer_response), peer);
  if (res == -1 && errno == EAGAIN) {
    message.type = ClientMessageType::HELLO_2;
    res = sendto(
        socket,
        &message,
        sizeof(message),
        0,
        (const struct sockaddr*) &peer,
        sizeof(peer)
    );
    assert(res > 0);
    config->message_type = ClientMessageType::PING;
    config->peer_addr = peer;
    return 1;
  }
  assert(peer_response.type == ClientMessageType::HELLO_2);

  config->message_type = ClientMessageType::PONG;
  config->peer_addr = peer;
  return 1;
}

int receive_from_addr(int socket, void* buf, size_t buf_size, sockaddr_in addr) {
  while (true) {
    sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);

    int res = recvfrom(
        socket,
        buf,
        buf_size,
        MSG_TRUNC,
        (struct sockaddr*) &recv_addr,
        &recv_addr_len
    );

    if (res < 0) {
      return res;
    }

    if (memcmp(&recv_addr.sin_addr, &addr.sin_addr, sizeof(addr.sin_addr)) != 0) {
      continue;
    }
    if (recv_addr.sin_port != addr.sin_port) {
      continue;
    }
    
    return res;
  }
}

void run_ping(int socket, sockaddr_in peer) {
  puts("I am ping!");
  for (size_t i = 0; i < 5; ++i) {
    ClientMessageType message = ClientMessageType::PING;
    int res = sendto(
        socket,
        &message,
        sizeof(message),
        0,
        (const struct sockaddr*) &peer,
        sizeof(peer)
    );
    assert(res > 0);
    puts("> ping");
    
    ClientMessageType response;
    res = receive_from_addr(socket, &response, sizeof(response), peer);
    assert(res == sizeof(response));
    assert(response == ClientMessageType::PONG);
    puts("< pong");
  }

  ClientMessageType message = ClientMessageType::GOODBYE;
  int res = sendto(
      socket,
      &message,
      sizeof(message),
      0,
      (const struct sockaddr*) &peer,
      sizeof(peer)
  );
  assert(res > 0);
  puts("> goodbye");
}

void run_pong(int socket, sockaddr_in peer) {
  puts("I am pong!");
  while (true) {
    ClientMessageType message;
    int res = receive_from_addr(socket, &message, sizeof(message), peer);
    assert(res == sizeof(message));
    if (message == ClientMessageType::GOODBYE) {
      puts("< goodbye");
      return;
    }
    assert(message == ClientMessageType::PING);
    puts("< ping");

    message = ClientMessageType::PONG;
    res = sendto(
        socket,
        &message,
        sizeof(message),
        0,
        (const struct sockaddr*) &peer,
        sizeof(peer)
    );
    assert(res > 0);

    puts("> pong");
  }
}

} // namespace stun
