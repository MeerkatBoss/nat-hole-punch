#include "Server.hpp"
#include <cstring>
#include <iostream>

int parse_ip(const char* ip_str, uint8_t* addr);

int parse_port(const char* port_str, uint16_t* port);

int main(int argc, char** argv)
{
  const char* usage =
    "Usage:\n"
    "nat-hole-punch <client|server> IP PORT\n";

  if (argc != 4) {
    std::cerr << usage;
    return 1;
  }

  uint8_t addr[4];
  uint16_t port;
  int res = 0;

  res = parse_ip(argv[2], addr);
  if (res < 0) {
    std::cerr << "Invalid ip address '" << argv[2] << "'\n";
    std::cerr << usage;
    return 1;
  }
  res = parse_port(argv[3], &port);
  if (res < 0) {
    std::cerr << "Invalid port '" << argv[3] << "'\n";
    std::cerr << usage;
    return 1;
  }

  if (strcmp("server", argv[1]) == 0) {
    stun::run_server(addr, port);
    return 0;
  }
  if (strcmp("client", argv[1]) == 0) {
    std::cerr << "Client is not implemented yet\n";
    return 2;
  }

  std::cerr << "Unknown mode '" << argv[1] << "'\n";
  std::cerr << usage;
  return 1;
}

int parse_ip(const char* ip_str, uint8_t* addr) {
  int length = strlen(ip_str);
  int parsed = 0;
  int res = sscanf(ip_str, "%hhu.%hhu.%hhu.%hhu%n",
      addr, addr + 1, addr + 2, addr + 3, &parsed);

  if (res < 0) {
    return -1;
  }
  if (parsed != length) {
    return -1;
  }

  return 0;
}

int parse_port(const char* port_str, uint16_t* port) {
  int length = strlen(port_str);
  int parsed = 0;
  int res = sscanf(port_str, "%hu%n", port, &parsed);
  if (res < 0 || parsed != length) {
    return -1;
  }

  return 0;
}
