# udp_client
ESPHOME external_component esp32h2 Thread Network raw udp transmit


# ESPHome UDP Client Component for ESP32-H2 (Thread Network)

A lightweight, custom ESPHome external component designed for the **ESP32-H2** to enable ultra-fast, low-power data transmission over an **IPv6-only OpenThread network** using raw UDP packets.

This component acts as a highly efficient alternative to the standard ESPHome Native API for battery-powered, deep-sleeping devices.

---

## 💡 The Problem & The Solution

### The Limitations of the ESPHome Native API over Thread

While ESPHome's native API is excellent for always-on Wi-Fi devices, it falls short for battery-powered Thread end-devices ($MTD$) utilizing short wake cycles and aggressive Deep Sleep loops for two primary reasons:

1. **Inverted Architecture (Server vs. Client):** In the native API design, the ESPHome device acts as the *server* and Home Assistant acts as the *client*. When a device wakes up from Deep Sleep, it must wait for Home Assistant to notice it is online and initiate a connection. This polling interval is unpredictable and introduces massive latency.
2. **Connection Overhead:** The TCP handshake and encryption setup of the Native API keep the device awake much longer than necessary, draining critical battery life.

### The UDP Alternative

This component replaces the Native API's push/pull pattern with an instant **UDP Push** model.

* As soon as the ESP32-H2 attaches to the Thread partition (typically within **~5 seconds** from booting), it can immediately blast packed UDP binary telemetry to a central server.
* The server side (e.g., **Node-RED** parsing raw UDP sockets) processes the incoming buffer and forwards the data directly to Home Assistant via MQTT.
* This cuts down active uptime significantly, allowing the device to return to Deep Sleep almost instantaneously.

---

## 🛠 Features

* **IPv6 Native:** Operates entirely within the Thread Mesh Local routing plane (`AF_INET6`).
* **Optimized Data Structure:** Sends packed C-structs (`UdpPacket`, 11 bytes) containing device IDs, sequential counts, custom command types, and flexible data slots to keep payloads micro-sized.
* **Bi-directional Feedback:** Includes an integrated ESPHome `binary_sensor` that trips to `True` the exact moment a matching UDP packet response is parsed from the server, functioning as an instant confirmation toggle.
* **Deep Sleep Coordination:** Works hand-in-hand with custom external deep-sleep routines to strictly maximize sleep intervals.

---

## ⚠️ Crucial Configuration Insights

### 1. The `safe_mode` Trap for Deep Sleep Devices

By default, ESPHome enables `safe_mode`. If a device boots up repeatedly and enters deep sleep with an uptime of less than 1 minute, ESPHome flags this as a crash-loop and forces the device into Safe Mode for 5 minutes.

For an agile battery device meant to wake, transmit in 5 seconds, and sleep, **this will brick your battery loop**. You must explicitly turn this off:

```yaml
safe_mode:
  disabled: True

```

### 2. Observing the Native API "Unpredictability"

In the provided example, the `api` and `logger` blocks are left active but are completely optional. Keeping them enabled during initial testing serves a valuable purpose: you will visually observe through the `USB_SERIAL_JTAG` logs how immediately the UDP client transmits data upon the Thread state transitioning to `child`, compared to the highly erratic and delayed behavior of the Native API connection.

---

## 📋 Configuration Example

To integrate this into your project, place the `udp_client` files inside your local `external_components` directory alongside the [wacsy ESP32-H2 Deep Sleep component](https://www.google.com/search?q=https://github.com/wacsy/esphh2/tree/h2-external-comps-deepsleep).

Below is the essential structure based on `udp-client-example.yaml`:

```yaml
substitutions:
  name: udp-client-example
  deviceId: 69
  udpServer: "fded:fd60:cdf3:44f7:f461:67c8:d602:df97" # Your Thread Border Router IPv6
  udpPort: 5683
  localPort: 5688

esphome:
  name: ${name}
  on_boot:
    priority: -100
    then:
      - h2_deep_sleep.prevent: deep_sleep_control
      - script.execute: restart_sleep_timer

esp32:
  board: esp32-h2-devkitm-1
  framework:
    type: esp-idf
    sdkconfig_options:    
      CONFIG_LWIP_MAX_SOCKETS: "16"
      CONFIG_LWIP_SO_RCVBUF: "y"
      CONFIG_LWIP_SO_REUSE: "y"

external_components:
  - source:
      type: local
      path: external_components
    components: [h2_deep_sleep, udp_client]

network:
  enable_ipv6: True
  
openthread:
  device_type: MTD
  force_dataset: True

safe_mode:
  disabled: True

udp_client:
  id: my_udp_client
  server_address: ${udpServer}
  server_port: ${udpPort}
  local_port: ${localPort}

binary_sensor:
  - platform: udp_client
    udp_client_id: my_udp_client
    id: udp_response
    device_class: connectivity
    on_press:
      then:
        - lambda: |-
            id(my_data)[0] = id(my_udp_client).get_last_type();
            // Map other data fields...
        - script.execute: process_data

```

---

## 📦 Packet Structure Reference (C++ & Node-RED)

The data payload passing over the wire maps directly to an 11-byte packet defined in `udp_client.h`:

```cpp
struct UdpPacket {
  uint8_t id;         // Device identifier (1 byte)
  uint8_t type;       // Packet type identifier (1 byte)
  uint16_t seq;       // Sequence number (2 bytes)
  uint16_t data1;      // Data field 1 (2 bytes)
  uint16_t data2;      // Data field 2 (2 bytes)
  uint16_t data3;      // Data field 3 (2 bytes)
  uint16_t data4;      // Data field 4 (2 bytes)
} __attribute__((packed));

```

When building your parsing logic in **Node-RED** via a standard UDP input node, make sure to read the incoming buffer using the corresponding byte indices (respecting the 16-bit unsigned integers for the sequence and data fields) before pushing the final payload object over to your MQTT broker.

---

## 🧑‍💻 Disclaimer & Contribution

> [!NOTE]
> **A Hobbyist Project:** I am not a professional software developer! I built this component purely out of personal passion for optimizing custom home automation gear. The code was heavily designed and shaped with the support of modern AI tools to bypass the current limitations of ESPHome's core architecture on low-power Thread hardware.

Feel free to copy, tear apart, fork, modify, or do whatever you want with this component to fit your own smart home creations! Pull requests or enhancements are always welcome.
