/**
 * @file Common.hpp
 * @author MeerkatBoss (solodovnikov.ia@phystech.su)
 *
 * @brief
 *
 * @version 0.0.1
 * @date 2024-11-17
 *
 * @copyright Copyright MeerkatBoss (c) 2024
 */
#ifndef __COMMON_HPP
#define __COMMON_HPP

#include <cstdint>
#include <netinet/in.h>

namespace stun {

struct ServerRequest {
  uint64_t conn_id;
  struct in_addr local_addr;
  in_port_t local_port;
};

struct ServerResponse {
  struct in_addr local_addr;
  in_port_t local_port;

  struct in_addr global_addr;
  in_port_t global_port;
};

} // namespace stun

#endif /* Common.hpp */
