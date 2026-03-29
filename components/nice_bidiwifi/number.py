import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, UNIT_SECOND, UNIT_PERCENT
from . import nice_bidiwifi_ns, NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

NiceNumber = nice_bidiwifi_ns.class_("NiceNumber", number.Number, cg.Component)

# Min/max for force (0x4A/0x4B) and speed (0x42/0x43): typical Nice Bus-T4 / DMP usage is 1–100 (%).
# Individual boards (e.g. CL201) may clamp internally or NACK SET outside their range — see the drive
# programming manual; use send_raw_cmd from a template text entity to probe unsupported values.
NUMBER_KEYS = {
    "pause_time": {
        "register": 0x81,
        "min": 0,
        "max": 250,
        "step": 5,
        "unit": UNIT_SECOND,
    },
    "opening_force": {
        "register": 0x4A,
        "min": 1,
        "max": 100,
        "step": 1,
        "unit": UNIT_PERCENT,
    },
    "closing_force": {
        "register": 0x4B,
        "min": 1,
        "max": 100,
        "step": 1,
        "unit": UNIT_PERCENT,
    },
    "opening_speed": {
        "register": 0x42,
        "min": 1,
        "max": 100,
        "step": 1,
        "unit": UNIT_PERCENT,
    },
    "closing_speed": {
        "register": 0x43,
        "min": 1,
        "max": 100,
        "step": 1,
        "unit": UNIT_PERCENT,
    },
    "photo_close_time": {
        "register": 0x85,
        "min": 0,
        "max": 250,
        "step": 1,
        "unit": UNIT_SECOND,
    },
    "photo_close_mode": {
        "register": 0x86,
        "min": 0,
        "max": 15,
        "step": 1,
        "unit": None,
    },
    "always_close_time": {
        "register": 0x89,
        "min": 0,
        "max": 250,
        "step": 1,
        "unit": UNIT_SECOND,
    },
    "always_close_mode": {
        "register": 0x8A,
        "min": 0,
        "max": 15,
        "step": 1,
        "unit": None,
    },
}


def _number_schema(key_info):
    kwargs = {}
    if key_info["unit"] is not None:
        kwargs["unit_of_measurement"] = key_info["unit"]
    return number.number_schema(NiceNumber, **kwargs)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        **{
            cv.Optional(key): _number_schema(info)
            for key, info in NUMBER_KEYS.items()
        },
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])

    for key, info in NUMBER_KEYS.items():
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await cg.register_component(var, conf)
            await number.register_number(
                var,
                conf,
                min_value=info["min"],
                max_value=info["max"],
                step=info["step"],
            )
            cg.add(var.set_hub(hub))
            cg.add(var.set_register_id(info["register"]))
