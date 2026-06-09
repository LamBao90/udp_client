"""
ESPHome UDP Client Component for ESP32-H2 Thread Network
Provides IPv6-only UDP communication with binary sensor feedback
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_PORT,
)
from esphome import automation

# Define the component namespace
udp_client_ns = cg.esphome_ns.namespace("udp_client")
UDPClientComponent = udp_client_ns.class_("UDPClientComponent", cg.Component)

# Define the send action
UDPSendAction = udp_client_ns.class_("UDPSendAction", automation.Action)

# Configuration keys
CONF_SERVER_ADDRESS = "server_address"
CONF_SERVER_PORT = "server_port"
CONF_LOCAL_PORT = "local_port"
#CONF_RESPONSE_TIMEOUT = "response_timeout"

# Dependencies - requires ESP-IDF for ESP32-H2
DEPENDENCIES = ["esp32"]

# Auto-load binary_sensor for response detection
AUTO_LOAD = ["binary_sensor"]

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UDPClientComponent),
        cv.Optional(CONF_SERVER_ADDRESS): cv.string,
        cv.Optional(CONF_SERVER_PORT, default=5683): cv.port,
        cv.Optional(CONF_LOCAL_PORT, default=0): cv.port,
        #cv.Optional(CONF_RESPONSE_TIMEOUT, default="2s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate C++ code from YAML configuration"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set server address if provided
    if CONF_SERVER_ADDRESS in config:
        cg.add(var.set_server_address(config[CONF_SERVER_ADDRESS]))
    
    # Set server port
    cg.add(var.set_server_port(config[CONF_SERVER_PORT]))
    
    # Set local port
    cg.add(var.set_local_port(config[CONF_LOCAL_PORT]))
    
    # Set response timeout
    #cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))

    # Add required ESP-IDF components
    #cg.add_platformio_option("board_build.partitions", "partitions.csv")
    
    # Add build flags for ESP32-H2 Thread support
    #cg.add_build_flag("-DCONFIG_OPENTHREAD_ENABLED=1")
    #cg.add_build_flag("-DCONFIG_LWIP_IPV6=1")
    #cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG=1")
