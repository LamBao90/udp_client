"""
Binary Sensor component for UDP Client
Provides feedback when UDP responses are received
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import udp_client_ns, UDPClientComponent

DEPENDENCIES = ["udp_client"]

CONF_UDP_CLIENT_ID = "udp_client_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_UDP_CLIENT_ID): cv.use_id(UDPClientComponent),
    }
)


async def to_code(config):
    """Generate binary sensor code"""
    # Get the parent UDP client component
    parent = await cg.get_variable(config[CONF_UDP_CLIENT_ID])
    
    # Create binary sensor
    var = await binary_sensor.new_binary_sensor(config)
    
    # Register sensor with UDP client component
    cg.add(parent.set_response_sensor(var))
