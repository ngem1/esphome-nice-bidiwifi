import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import nice_bidiwifi_ns, NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

NiceSwitch = nice_bidiwifi_ns.class_("NiceSwitch", switch.Switch, cg.Component)

SWITCH_KEYS = {
    "auto_close": 0x80,
    "photo_close": 0x84,
    "always_close": 0x88,
    "standby": 0x8C,
    "pre_flash": 0x94,
    "key_lock": 0x9C,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        cv.Optional("auto_close"): switch.switch_schema(NiceSwitch),
        cv.Optional("photo_close"): switch.switch_schema(NiceSwitch),
        cv.Optional("always_close"): switch.switch_schema(NiceSwitch),
        cv.Optional("standby"): switch.switch_schema(NiceSwitch),
        cv.Optional("pre_flash"): switch.switch_schema(NiceSwitch),
        cv.Optional("key_lock"): switch.switch_schema(NiceSwitch),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])

    for key, register_id in SWITCH_KEYS.items():
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await cg.register_component(var, conf)
            await switch.register_switch(var, conf)
            cg.add(var.set_hub(hub))
            cg.add(var.set_register_id(register_id))
