import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID
from . import nice_bidiwifi_ns, NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

NiceButton = nice_bidiwifi_ns.class_("NiceButton", button.Button, cg.Component)
CONF_COMMAND = "command"

BUTTON_COMMANDS = {
    "sbs": 0x01, "stop": 0x02, "open": 0x03, "close": 0x04,
    "partial_open_1": 0x05, "partial_open_2": 0x06, "partial_open_3": 0x07,
    "partial_open_4": 0x0B, "partial_open_5": 0x0C, "partial_open_6": 0x0D,
    "close_and_lock": 0x0E, "lock": 0x0F, "unlock": 0x10,
    "light_timer": 0x11, "light_toggle": 0x12,
    "master_sbs": 0x13, "master_open": 0x14, "master_close": 0x15,
    "slave_sbs": 0x16, "slave_open": 0x17, "slave_close": 0x18,
    "unlock_and_open": 0x19, "unlock_and_close": 0x1A,
    "auto_open_on": 0x1B, "auto_open_off": 0x1C,
}

CONFIG_SCHEMA = (
    button.button_schema(NiceButton)
    .extend({
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        cv.Required(CONF_COMMAND): cv.enum(BUTTON_COMMANDS, lower=True),
    })
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_command(config[CONF_COMMAND]))
