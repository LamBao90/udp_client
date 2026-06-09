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
* **Optimized Data Structure:** Sends packed C-structs (`UdpPacket`, 12 bytes) containing device IDs, sequential counts, custom command types, and flexible data slots to keep payloads micro-sized.
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

To integrate this into your project, place the `udp_client` files inside your local `external_components` directory alongside the [wacsy ESP32-H2 Deep Sleep component](https://github.com/wacsy/esphh2/tree/h2-external-comps-deepsleep).

Below is the essential structure based on `udp-client-example.yaml`:

```yaml
substitutions:
  name: udp-client-example
  deviceId: 69
  udpServer: "fded:fd60:cdf3:44f7:f461:67c8:d602:df97" # Your Thread Border Router IPv6 (MeshLocalAddress in OTBR webpage)
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

The data payload passing over the wire maps directly to an 12-byte packet defined in `udp_client.h`:

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

## Here's an example Node-red flow
### 1. Udp handle
![UDP receive and send](/images/node-udp-handle.jpg)

udp in with port above
```yaml
  udpPort: 5683
  #localPort: 5688 #udp out ip and port will define in payload message
```
in packet parser
```cpp
const buf = msg.payload;
if (buf.length < 12) return null;

const pkt = {
  id:    buf.readUInt8(0),
  type:  buf.readUInt8(1),
  seq:   buf.readUInt16LE(2),
  data1: buf.readUInt16LE(4),
  data2: buf.readUInt16LE(6),
  data3: buf.readUInt16LE(8),
  data4: buf.readUInt16LE(10),
  ip:    msg.ip,
  port:  msg.port,
  ts:    Date.now()
};

msg.payload = pkt;
return msg;
```
in queue push
```cpp
let queue = flow.get("udpQueue") || [];
queue.push(msg);
flow.set("udpQueue", queue);

if (!flow.get("udpSending")) {
    flow.set("udpSending", true);
    node.send({ topic: "kick" });
}

return null;
```
in queue pop
```cpp
let queue = flow.get("udpQueue") || [];

if (queue.length === 0) {
    flow.set("udpSending", false);
    return null;
}

let out = queue.shift();
flow.set("udpQueue", queue);

return out;
```
### 2. Splitter
Handle multiple device with `deviceId:`
![](/images/splitter.jpg)

### 3. Main flow
udp respones and send mqtt discovery to Home assistant
![](/images/flow.jpg)
in device manager
```cpp
const pkt = msg.payload;
//const id = pkt.id.toString();

let devices = flow.get("devices") || {};

// init device if first seen
if (!devices) {
    devices = {
        id: pkt.id,
        last_seq: pkt.seq,
        last_seen: pkt.ts,
        ip: pkt.ip,
        port: pkt.port,
        online: true
    };
}
const lastState = devices.online;

// heartbeat handling
if (pkt.type === 255) {
    devices.id        = pkt.id
    devices.last_seen = pkt.ts;
    devices.last_seq  = pkt.seq;
    devices.ip        = pkt.ip;
    devices.port      = pkt.port;
    devices.online    = (pkt.data3 === 1)?true:false;
}

// special message for ota
let isOta = flow.get("is_ota");
if (isOta === undefined) {
    isOta = false;
    flow.set("is_ota", false);
}

if (pkt.type === 254) {
    flow.set("is_ota", (pkt.data1===0)?false:true);
}

// save registry
flow.set("devices", devices);

// set
//flow.set("is_ota", true);

// get
//const isOta = flow.get("is_ota") || false;

// pass through for further logic
msg.payload = pkt;

if (!lastState && devices.online) {
    const isOta = flow.get("is_ota") || false;
    let msg1 = { payload: isOta };
    return [msg, msg, msg1];
}
else{
    return [null, msg, null];
}
```
in mqtt discovery
```cpp
const id = msg.payload.id;
if (id == null) {
    node.error("Missing msg.id");
    return null;
}

msg.topic = `homeassistant/device/remote_${id}/config`;

msg.payload = {
  dev: {
    ids: id,
    name: `Remote ${id}`,
    mf: "DIY",
    mdl: "ESP32-H2 Remote",
    sw: "1.0",
    sn: id,
    hw: "rev1"
  },

  o: {
    name: "udp2mqtt",
    sw: "0.1",
    url: "http://local-node-red"
  },

  state_topic: `remote/${id}/state`,
  qos: 1,

  cmps: {
    ota: {
      p: "switch",
      name: "OTA",
      unique_id: `remote_${id}_ota`,
      command_topic: `remote/${id}/cmd/ota`,
      value_template: "{{ value_json.ota }}",
      payload_on: "true",
      payload_off: "false"
    },

    battery: {
      p: "sensor",
      name: "Battery",
      device_class: "battery",
      unit_of_measurement: "%",
      value_template: "{{ value_json.battery }}",
      unique_id: `remote_${id}_battery`
    },

    charging: {
      p: "binary_sensor",
      name: "Charging",
      device_class: "battery_charging",
      value_template: "{{ value_json.charging }}",
      payload_on: "true",
      payload_off: "false",
      unique_id: `remote_${id}_charging`
    },
    wake: {
      p: "binary_sensor",
      name: "wake",
      value_template: "{{ value_json.wake }}",
      payload_on: "true",
      payload_off: "false",
      unique_id: `remote_${id}_wake`
    },
    ip: {
      p: "sensor",
      name: "IP Address",
      value_template: "{{ value_json.ip }}",
      unique_id: `remote_${id}_ip`
    },

    port: {
      p: "sensor",
      name: "Port",
      value_template: "{{ value_json.port }}",
      unique_id: `remote_${id}_port`
    },
    last_seen: {
      p: "sensor",
      name: "Last Seen",
      value_template: "{{ value_json.lastseen }}",
      unique_id: `remote_${id}_lastseen`
    }
  }
};

return msg;
```
in cmd parser
```cpp
const buf = msg.payload;
if (buf.type === 255){//heart beat message
    const isOta = flow.get("is_ota") || false;
    const remote = {
        id: buf.id,
        ota: (isOta)?"true":"false",
        charging: (buf.data2===0)?"false":"true",
        wake: (buf.data3===0)?"false":"true",
        bat: buf.data4,
        ip: buf.ip,
        port:   buf.port,
        ts:     buf.ts
    };
    msg.payload = remote;
    return [msg,null,null,null,null];
}

else if (buf.type === 11){
    const light = [
        (buf.data1===0)?false:true,
        (buf.data2===0)?false:true,
        (buf.data3===0)?false:true,
        (buf.data4===0)?false:true
    ];
    msg.payload = light;
    return [null,msg,null,null,null];
}
else if (buf.type === 12){
    const ac = {
        mode: buf.data1,
        temp: buf.data2,
        fan_mode: buf.data3,
        swing: buf.data4
    };
    msg.payload = ac;
    return [null,null,msg,null,null];
}
else if (buf.type === 13){
    const fan = {
        power: (buf.data1===0)?false:true,
        speed: buf.data2,
        swing: (buf.data3===0)?false:true,
    };
    msg.payload = fan;
    return [null,null,null,msg,null];
}
else if (buf.type === 14){
    const lamp = [
        buf.data1,
        buf.data2,
        buf.data3,
        buf.data4
    ];
    msg.payload = lamp;
    return [null,null,null,null,msg];
}
return null;
```
in remote infor
```cpp
const pkt = msg.payload;
const id = pkt.id;

const local = new Date(pkt.ts).toLocaleString("vi-VN", { //change to your local
    hour12: false
});

msg.topic = `remote/${id}/state`;
msg.payload = {
  battery: pkt.bat,
  charging: pkt.charging,
  ip: pkt.ip,
  port: pkt.port,
  wake: pkt.wake,
  lastseen: local,
  ota: pkt.ota
};
return msg;
```
ha cmd mqtt topic `remote/69/cmd/ota`

in form OTA cmd
```cpp
const pkt = {
  type:  253, //Set OTA command
  data1: (msg.payload)?1:0,
  data2: 0,
  data3: 0,
  data4: 0,
};
msg.payload = pkt;
return msg;
```
in create udp packet
```cpp
const devices = flow.get("devices");
const dev_id = devices.id;
const pkt = msg.payload;

let counter = flow.get("counter");
if (counter === undefined) {
    counter = 0;
    flow.set("counter", 0);
}

if (!devices || !devices.online) return null;

// build binary packet
const buf = Buffer.alloc(12);
buf.writeUInt8(dev_id, 0);
buf.writeUInt8(pkt.type, 1);
buf.writeUInt16LE(counter & 0xffff, 2);
buf.writeUInt16LE(pkt.data1, 4);
buf.writeUInt16LE(pkt.data2, 6);
buf.writeUInt16LE(pkt.data3, 8);
buf.writeUInt16LE(pkt.data4, 10);

counter++;
flow.set("counter", counter);

msg.payload = buf;
msg.ip = devices.ip;
msg.port = devices.port;
return msg;
```
check heart beat to set device offline (in case miss the last messge/ power lost)
```cpp
const devices = flow.get("devices") || {};
const now = Date.now();
const TIMEOUT = 15000; // 15s = 3 heartbeat

let offline = [];

    if (devices.online &&
        now - devices.last_seen > TIMEOUT) {
        devices.online = false;
    }


flow.set("devices", devices);
return null;
```

after successfull boot, your device will show in mqtt device automatically.
![](/images/mqtt%20device%20dashboard.jpg)




---

## 🧑‍💻 Disclaimer & Contribution

> [!NOTE]
> **A Hobbyist Project:** I am not a professional software developer! I built this component purely out of personal passion for optimizing custom home automation gear. The code was heavily designed and shaped with the support of modern AI tools to bypass the current limitations of ESPHome's core architecture on low-power Thread hardware.

Feel free to copy, tear apart, fork, modify, or do whatever you want with this component to fit your own smart home creations! Pull requests or enhancements are always welcome.
