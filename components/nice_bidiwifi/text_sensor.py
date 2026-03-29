import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        cv.Optional("product"): text_sensor.text_sensor_schema(),
        cv.Optional("firmware_version"): text_sensor.text_sensor_schema(),
        cv.Optional("hardware_version"): text_sensor.text_sensor_schema(),
        cv.Optional("gate_status"): text_sensor.text_sensor_schema(),
        cv.Optional("manufacturer"): text_sensor.text_sensor_schema(),
        cv.Optional("oxi_event"): text_sensor.text_sensor_schema(),
        cv.Optional("last_stop_reason"): text_sensor.text_sensor_schema(),
        cv.Optional("diagnostic_parameters"): text_sensor.text_sensor_schema(),
    }
).extend(cv.COMPONENT_SCHEMA)


TEXT_SENSOR_KEYS = {
    "product": "set_product_text_sensor",
    "firmware_version": "set_firmware_version_text_sensor",
    "hardware_version": "set_hardware_version_text_sensor",
    "gate_status": "set_gate_status_text_sensor",
    "manufacturer": "set_manufacturer_text_sensor",
    "oxi_event": "set_oxi_event_text_sensor",
    "last_stop_reason": "set_last_stop_reason_text_sensor",
    "diagnostic_parameters": "set_diag_par_text_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])

    for key, setter in TEXT_SENSOR_KEYS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
