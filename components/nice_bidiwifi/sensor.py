import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from . import NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        cv.Optional("position"): sensor.sensor_schema(
            unit_of_measurement="%",
            accuracy_decimals=1,
        ),
        cv.Optional("encoder_value"): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
        cv.Optional("maneuver_count"): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
        cv.Optional("maintenance_count"): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
        cv.Optional("maintenance_threshold"): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


SENSOR_KEYS = {
    "position": "set_position_sensor",
    "encoder_value": "set_encoder_sensor",
    "maneuver_count": "set_maneuver_count_sensor",
    "maintenance_count": "set_maintenance_count_sensor",
    "maintenance_threshold": "set_maintenance_threshold_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])

    for key, setter in SENSOR_KEYS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
