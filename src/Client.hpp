/**
 * @file Client.hpp
 * @author MeerkatBoss (solodovnikov.ia@phystech.su)
 *
 * @brief
 *
 * @version 0.0.1
 * @date 2024-11-17
 *
 * @copyright Copyright MeerkatBoss (c) 2024
 */
#ifndef __CLIENT_HPP
#define __CLIENT_HPP

#include <cstdint>
namespace stun {

void run_client(
    uint8_t my_addr[4],
    uint16_t my_port,
    uint8_t server_addr[4],
    uint16_t server_port
);

} // namespace stun

#endif /* Client.hpp */
