CONF_DEVICE_ID = "device_id"
CONF_REGISTER_ID = "register_id"
CONF_DEVICES = "devices"
CONF_REGISTERS = "registers"
CONF_READ_DELAY = "read_delay"
CONF_MESSAGE = "message"
CONF_ON_SETUP_ID = "on_setup_id"
CONF_ON_SETUP = "on_setup"
CONF_MESSAGE_BUILDER_ID = "message_builder_id"
CONF_MESSAGE_BUILDER_KEY = "message_builder_key"
CONF_CACHE = "cache"
CONF_PIN_BANKS = "pin_banks"
# TODO: consider changing to custom_i2c_pin_bank_id since this globally pollutes the `pin: ...` configuration option on
# any components that use GPIO pins.
# (That's annoyingly long, though. Alternative: leave as is, and if/when someone else comes along and wants to make a
# component that follows this "pin/pin bank" model, turn it into a registry so that anyone can register a pin bank type
# and refer to pins in that pin bank using the `pin_bank_id` config key. Actually I think I prefer this option.)
# TODO: Also decide if we want to rename this to `pin_bank` to follow the pattern that pin keys don't include `_id` in
# their names (which per https://discord.com/channels/429907082951524364/1204312566231334992/1204545049908477972 is
# inconsistent with the rest of esphome, but seems to be a consistent inconsistency across components that provide GPIO
# pins).
CONF_PIN_BANK_ID = "pin_bank_id"

KEY_CUSTOM_I2C = "custom_i2c"
KEY_PIN_BANK_BYTE_COUNTS = "pin_bank_byte_counts"
