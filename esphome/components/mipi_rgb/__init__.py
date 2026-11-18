import esphome.codegen as cg

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "mipi_rgb"

register_enable_pin = cg.slot_list(
    "MIPI_RGB_ENABLE_PIN_COUNT", "register_enable_pin", DOMAIN
)
