import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID
from . import nice_bidiwifi_ns, NiceBidiWiFi, CONF_NICE_BIDIWIFI_ID

DEPENDENCIES = ["nice_bidiwifi"]

NiceCover = nice_bidiwifi_ns.class_("NiceCover", cover.Cover, cg.Component)

CONF_INVERT_OPEN_CLOSE = "invert_open_close"
CONF_SUPPORTS_POSITION = "supports_position"

CONFIG_SCHEMA = (
    cover.cover_schema(NiceCover)
    .extend({
        cv.GenerateID(CONF_NICE_BIDIWIFI_ID): cv.use_id(NiceBidiWiFi),
        cv.Optional(CONF_INVERT_OPEN_CLOSE, default=False): cv.boolean,
        cv.Optional(CONF_SUPPORTS_POSITION, default=True): cv.boolean,
    })
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    hub = await cg.get_variable(config[CONF_NICE_BIDIWIFI_ID])
    cg.add(hub.register_cover(var))
    cg.add(var.set_hub(hub))
    cg.add(var.set_invert_open_close(config[CONF_INVERT_OPEN_CLOSE]))
    cg.add(var.set_supports_position(config[CONF_SUPPORTS_POSITION]))
