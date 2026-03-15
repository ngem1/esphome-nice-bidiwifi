# No custom output platform needed for the Nice BiDi-WiFi component.
#
# The signal, RX, and TX LEDs are standard GPIO outputs and can be
# configured directly in the user's ESPHome YAML using the built-in
# gpio output platform:
#
#   output:
#     - platform: gpio
#       pin: 25
#       id: signal_led
#     - platform: gpio
#       pin: 26
#       id: rx_led
#     - platform: gpio
#       pin: 27
#       id: tx_led
