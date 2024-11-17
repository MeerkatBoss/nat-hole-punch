/**
 * @file Server.hpp
 * @author MeerkatBoss (solodovnikov.ia@phystech.su)
 *
 * @brief
 *
 * @version 0.0.1
 * @date 2024-11-17
 *
 * @copyright Copyright MeerkatBoss (c) 2024
 */
#ifndef __SERVER_HPP
#define __SERVER_HPP

#include <cstdint>
namespace stun {

void run_server(uint8_t ip_addr[4], uint16_t port);

} // namespace stun

#endif /* Server.hpp */
