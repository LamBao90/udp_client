/**
 * @file udp_client.h
 * @brief ESPHome UDP Client Component for ESP32-H2 Thread Network
 * 
 * This component provides IPv6-only UDP communication over Thread network
 * with binary sensor feedback for received packets.
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esp_timer.h"

#ifdef USE_ESP32
#include <lwip/sockets.h>
#include <lwip/netdb.h>
//#include <esp_openthread.h>
//#include <esp_openthread_lock.h>
//#include <openthread/thread.h>
//#include <openthread/instance.h>
#endif

namespace esphome {
namespace udp_client {

/**
 * @brief UDP packet structure for send/receive
 * Total size: 11 bytes (1 + 2 + 2*4)
 */
struct UdpPacket {
  uint8_t id;      	 // Device identifier
  uint8_t type;      // Packet type identifier
  uint16_t seq;      // Sequence number
  uint16_t data1;     // Data field 1
  uint16_t data2;     // Data field 2
  uint16_t data3;     // Data field 3
  uint16_t data4;     // Data field 4
} __attribute__((packed));

/**
 * @brief Main UDP Client Component Class
 * 
 * Manages IPv6 UDP communication over Thread network with automatic
 * Thread connection checking and binary sensor feedback.
 */
class UDPClientComponent : public Component {
 public:
  UDPClientComponent() = default;

  // Component lifecycle methods
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Configuration setters
  void set_server_address(const std::string &address) { server_address_ = address; }
  void set_server_port(uint16_t port) { server_port_ = port; }
  void set_local_port(uint16_t port) { local_port_ = port; }
  //void set_response_timeout(uint32_t timeout_ms) { response_timeout_ms_ = timeout_ms; }

  // Binary sensor setter for response detection
  void set_response_sensor(binary_sensor::BinarySensor *sensor) { response_sensor_ = sensor; }

  /**
   * @brief Send UDP packet to specified IPv6 address
   * 
   * @param address IPv6 address string (e.g., "fd53::1" or "[fd53::1]")
   * @param port Destination port
   * @param packet UdpPacket structure to send
   * @return true if send successful, false otherwise
   */
  bool post(const std::string &address, uint16_t port, const UdpPacket &packet);

  /**
   * @brief Send UDP packet to configured server
   * 
   * @param packet UdpPacket structure to send
   * @return true if send successful, false otherwise
   */
  bool post(const UdpPacket &packet);

  /**
   * @brief Update binary sensor state
   */
  void reset_sensor();
  
  // Getters for last received packet data
  uint8_t get_last_id() const { return last_received_.id; }
  uint8_t get_last_type() const { return last_received_.type; }
  uint16_t get_last_seq() const { return last_received_.seq; }
  int16_t get_last_data1() const { return last_received_.data1; }
  int16_t get_last_data2() const { return last_received_.data2; }
  int16_t get_last_data3() const { return last_received_.data3; }
  int16_t get_last_data4() const { return last_received_.data4; }
  bool thread_connected_{false};
 protected:
  /**
   * @brief Check if device has joined Thread network
   * @return true if connected, false otherwise
   */
  //bool check_thread_connection_();

  /**
   * @brief Initialize UDP socket for IPv6
   * @return true if successful, false otherwise
   */
  bool init_socket_();

  /**
   * @brief Close UDP socket
   */
  void close_socket_();

  /**
   * @brief Process received UDP data
   * @param data Pointer to received data
   * @param len Length of received data
   */
  void process_received_data_(const uint8_t *data, size_t len);

  /**
   * @brief Strip brackets from IPv6 address if present
   * @param address IPv6 address string
   * @return Cleaned IPv6 address
   */
  std::string strip_ipv6_brackets_(const std::string &address);

  // Configuration
  std::string server_address_{};
  uint16_t server_port_{5683};
  uint16_t local_port_{0};
  uint32_t response_timeout_ms_{2000};

  // Socket management
  int sock_{-1};
  bool socket_initialized_{false};
  
  bool thread_connected_last{false};
  uint32_t last_thread_check_{0};
  static const uint32_t THREAD_CHECK_INTERVAL = 5000;  // Check every 5 seconds

  // Received data storage
  UdpPacket last_received_{};
  bool sensor_active_{false};

  // Binary sensor for response detection
  binary_sensor::BinarySensor *response_sensor_{nullptr};
};

}  // namespace udp_client
}  // namespace esphome
