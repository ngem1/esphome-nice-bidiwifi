import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        cv.Optional("photocell"): binary_sensor.binary_sensor_schema(),
        cv.Optional("limit_switch_open"): binary_sensor.binary_sensor_schema(),
        cv.Optional("limit_switch_close"): binary_sensor.binary_sensor_schema(),
        cv.Optional("obstacle"): binary_sensor.binary_sensor_schema(),
        cv.Optional("locked"): binary_sensor.binary_sensor_schema(),
    }
).extend(cv.COMPONENT_SCHEMA)

BINARY_SENSOR_KEYS = {
    "photocell": "set_photocell_sensor",
    "limit_switch_open": "set_limit_open_sensor",
    "limit_switch_close": "set_limit_close_sensor",
    "obstacle": "set_obstacle_sensor",
    "locked": "set_locked_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])

    for key, setter in BINARY_SENSOR_KEYS.items():
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
