import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins

CODEOWNERS = ["@ESPHome_BidiWiFi"]
MULTI_CONF = False

CONF_NICE_BIDIWIFI_ID = "nice_bidiwifi_id"
CONF_UART_RX_PIN = "uart_rx_pin"
CONF_UART_TX_PIN = "uart_tx_pin"
CONF_HUB_ADDRESS = "hub_address"
CONF_LED1_RED_PIN = "led1_red_pin"
CONF_LED1_GREEN_PIN = "led1_green_pin"
CONF_LED2_RED_PIN = "led2_red_pin"
CONF_LED2_GREEN_PIN = "led2_green_pin"
CONF_LED3_PIN = "led3_pin"

nice_bidiwifi_ns = cg.esphome_ns.namespace("nice_bidiwifi")
NiceBidiWiFi = nice_bidiwifi_ns.class_("NiceBidiWiFi", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NiceBidiWiFi),
        cv.Optional(CONF_UART_RX_PIN, default=18): pins.internal_gpio_input_pin_number,
        cv.Optional(CONF_UART_TX_PIN, default=21): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_HUB_ADDRESS, default=0x5090): cv.hex_uint16_t,
        cv.Optional(CONF_LED1_RED_PIN, default=25): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_LED1_GREEN_PIN, default=2): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_LED2_RED_PIN, default=27): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_LED2_GREEN_PIN, default=26): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_LED3_PIN, default=22): pins.internal_gpio_output_pin_number,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_uart_rx_pin(config[CONF_UART_RX_PIN]))
    cg.add(var.set_uart_tx_pin(config[CONF_UART_TX_PIN]))
    cg.add(var.set_hub_address(config[CONF_HUB_ADDRESS]))
    cg.add(var.set_led1_red_pin(config[CONF_LED1_RED_PIN]))
    cg.add(var.set_led1_green_pin(config[CONF_LED1_GREEN_PIN]))
    cg.add(var.set_led2_red_pin(config[CONF_LED2_RED_PIN]))
    cg.add(var.set_led2_green_pin(config[CONF_LED2_GREEN_PIN]))
    cg.add(var.set_led3_pin(config[CONF_LED3_PIN]))
