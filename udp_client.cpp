/**
 * @file udp_client.cpp
 * @brief ESPHome UDP Client Component Implementation
 */

#include "udp_client.h"
#include "esphome/core/log.h"
//#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef USE_ESP32
#include <esp_netif.h>
#include <arpa/inet.h>
#endif

namespace esphome {
namespace udp_client {

static const char *const TAG = "udp_client";

void UDPClientComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up UDP Client Component...");
  
  // Don't initialize socket yet - wait for Thread connection
  this->thread_connected_ = false;
  this->socket_initialized_ = false;
  this->last_thread_check_ = 0;
  this->sensor_active_ = false;
  this->response_sensor_->publish_state(false);
  ESP_LOGCONFIG(TAG, "UDP Client Component setup complete");
}

void UDPClientComponent::loop() {
  //uint32_t now = esp_timer_get_time() / 1000;
  
  // Periodically check Thread connection status
    
    //this->thread_connected_ = this->check_thread_connection_();

    if (this->thread_connected_ && !this->thread_connected_last) {
      ESP_LOGI(TAG, "Thread network connected - initializing UDP socket");
      this->init_socket_();
    } else if (!this->thread_connected_ && this->thread_connected_last) {
      ESP_LOGW(TAG, "Thread network disconnected - closing socket");
      this->close_socket_();
    }
	this->thread_connected_last = this->thread_connected_;
  
  // Only process UDP if socket is initialized
  if (!this->socket_initialized_ || this->sock_ < 0) {
    return;
  }
  
  // Non-blocking receive check
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(this->sock_, &read_fds);
  
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;  // Non-blocking
  
  int ret = select(this->sock_ + 1, &read_fds, nullptr, nullptr, &timeout);
  
  if (ret > 0 && FD_ISSET(this->sock_, &read_fds)) {
    // Data available to read
    uint8_t buffer[256];
    struct sockaddr_in6 source_addr;
    socklen_t addr_len = sizeof(source_addr);
    
    ssize_t len = recvfrom(this->sock_, buffer, sizeof(buffer), 0,
                          (struct sockaddr *)&source_addr, &addr_len);
    
    if (len > 0) {
      char addr_str[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &source_addr.sin6_addr, addr_str, sizeof(addr_str));
      
      ESP_LOGI(TAG, "Received %d bytes from [%s]:%d", len, addr_str, 
               ntohs(source_addr.sin6_port));
      
      this->process_received_data_(buffer, len);
    } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
    }
  }
  
}

void UDPClientComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UDP Client Component:");
  if (!this->server_address_.empty()) {
    ESP_LOGCONFIG(TAG, "  Server Address: %s", this->server_address_.c_str());
    ESP_LOGCONFIG(TAG, "  Server Port: %d", this->server_port_);
  }
  ESP_LOGCONFIG(TAG, "  Local Port: %d", this->local_port_);
  ESP_LOGCONFIG(TAG, "  Response Timeout: %d ms", this->response_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Thread Connected: %s", this->thread_connected_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Socket Initialized: %s", this->socket_initialized_ ? "YES" : "NO");
}

/* bool UDPClientComponent::check_thread_connection_() {
#ifdef USE_ESP32
  // Get OpenThread instance
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance *instance = esp_openthread_get_instance();
  
  if (instance == nullptr) {
    esp_openthread_lock_release();
    ESP_LOGD(TAG, "OpenThread instance not available");
    return false;
  }
  
  // Check if device role indicates connection
  otDeviceRole role = otThreadGetDeviceRole(instance);
  esp_openthread_lock_release();
  
  // Device is connected if it's a child, router, or leader
  bool connected = (role == OT_DEVICE_ROLE_CHILD || 
                   role == OT_DEVICE_ROLE_ROUTER || 
                   role == OT_DEVICE_ROLE_LEADER);
  
  if (connected) {
    ESP_LOGD(TAG, "Thread role: %d (connected)", role);
  } else {
    ESP_LOGD(TAG, "Thread role: %d (not connected)", role);
  }
  
  return connected;
#else
  ESP_LOGW(TAG, "OpenThread not available on this platform");
  return false;
#endif
} */

bool UDPClientComponent::init_socket_() {
#ifdef USE_ESP32
  if (this->socket_initialized_ && this->sock_ >= 0) {
    ESP_LOGD(TAG, "Socket already initialized");
    return true;
  }
  
  // Close old socket if exists
  this->close_socket_();
  
  // Create IPv6 UDP socket
  this->sock_ = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (this->sock_ < 0) {
    ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
    return false;
  }
  
  // Set socket to non-blocking mode
  int flags = fcntl(this->sock_, F_GETFL, 0);
  if (flags < 0 || fcntl(this->sock_, F_SETFL, flags | O_NONBLOCK) < 0) {
    ESP_LOGE(TAG, "Failed to set socket non-blocking: errno %d", errno);
    this->close_socket_();
    return false;
  }
  
  // Bind to local port if specified
  if (this->local_port_ > 0) {
    struct sockaddr_in6 bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_port = htons(this->local_port_);
    bind_addr.sin6_addr = in6addr_any;
    
    if (bind(this->sock_, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
      ESP_LOGE(TAG, "Failed to bind to port %d: errno %d", this->local_port_, errno);
      this->close_socket_();
      return false;
    }
    
    ESP_LOGI(TAG, "Socket bound to port %d", this->local_port_);
  }
  
  this->socket_initialized_ = true;
  ESP_LOGI(TAG, "UDP socket initialized successfully");
  return true;
  
#else
  ESP_LOGE(TAG, "Socket initialization only supported on ESP32");
  return false;
#endif
}

void UDPClientComponent::close_socket_() {
  if (this->sock_ >= 0) {
    close(this->sock_);
    this->sock_ = -1;
    ESP_LOGD(TAG, "Socket closed");
  }
  this->socket_initialized_ = false;
}

bool UDPClientComponent::post(const std::string &address, uint16_t port, const UdpPacket &packet) {
#ifdef USE_ESP32
  // Check Thread connection
  if (!this->thread_connected_) {
    ESP_LOGW(TAG, "Cannot send: Thread network not connected");
    return false;
  }
  
  // Ensure socket is initialized
  if (!this->socket_initialized_) {
    if (!this->init_socket_()) {
      ESP_LOGE(TAG, "Failed to initialize socket");
      return false;
    }
  }
  
  // Strip brackets from IPv6 address if present
  std::string clean_address = this->strip_ipv6_brackets_(address);
  
  // Prepare destination address
  struct sockaddr_in6 dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sin6_family = AF_INET6;
  dest_addr.sin6_port = htons(port);
  
  // Convert IPv6 address string to binary
  if (inet_pton(AF_INET6, clean_address.c_str(), &dest_addr.sin6_addr) <= 0) {
    ESP_LOGE(TAG, "Invalid IPv6 address: %s", clean_address.c_str());
    return false;
  }
  
  // Send packet
  ssize_t sent = sendto(this->sock_, &packet, sizeof(UdpPacket), 0,
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  
  if (sent < 0) {
    ESP_LOGE(TAG, "sendto failed: errno %d", errno);
    return false;
  }
  
  if (sent != sizeof(UdpPacket)) {
    ESP_LOGW(TAG, "Partial send: %d/%d bytes", sent, sizeof(UdpPacket));
    return false;
  }
  
  ESP_LOGI(TAG, "Sent packet: type=%d, seq=%d to [%s]:%d", 
           packet.type, packet.seq, clean_address.c_str(), port);
  
  return true;
  
#else
  ESP_LOGE(TAG, "UDP send only supported on ESP32");
  return false;
#endif
}

bool UDPClientComponent::post(const UdpPacket &packet) {
  if (this->server_address_.empty()) {
    ESP_LOGE(TAG, "No server address configured");
    return false;
  }
  
  return this->post(this->server_address_, this->server_port_, packet);
}

void UDPClientComponent::process_received_data_(const uint8_t *data, size_t len) {
  // Validate packet size
  if (len != sizeof(UdpPacket)) {
    ESP_LOGW(TAG, "Invalid packet size: expected %d, got %d", sizeof(UdpPacket), len);
    return;
  }
  
  // Copy received data to internal structure
  memcpy(&this->last_received_, data, sizeof(UdpPacket));
  
  ESP_LOGI(TAG, "Parsed packet: id=%d, type=%d, seq=%d, data=[%d, %d, %d, %d]",
		   this->last_received_.id,
           this->last_received_.type,
           this->last_received_.seq,
           this->last_received_.data1,
           this->last_received_.data2,
           this->last_received_.data3,
           this->last_received_.data4);
  
  // Update timestamp and trigger sensor
  this->sensor_active_ = true;
  
  // Immediately update binary sensor
  if (this->response_sensor_ != nullptr) {
    this->response_sensor_->publish_state(true);
  }
}

void UDPClientComponent::reset_sensor() {
  if (!this->sensor_active_ || this->response_sensor_ == nullptr) {
    return;
  }
  this->sensor_active_ = false;
  this->response_sensor_->publish_state(false);
}

std::string UDPClientComponent::strip_ipv6_brackets_(const std::string &address) {
  std::string result = address;
  
  // Remove leading bracket
  if (!result.empty() && result[0] == '[') {
    result = result.substr(1);
  }
  
  // Remove trailing bracket
  if (!result.empty() && result[result.length() - 1] == ']') {
    result = result.substr(0, result.length() - 1);
  }
  
  return result;
}

}  // namespace udp_client
}  // namespace esphome
