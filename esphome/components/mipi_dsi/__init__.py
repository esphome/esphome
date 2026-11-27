import esphome.codegen as cg

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "mipi_dsi"

register_enable_pin = cg.slot_list(
    "MIPI_DSI_ENABLE_PIN_COUNT", "register_enable_pin", DOMAIN
)

mipi_dsi_ns = cg.esphome_ns.namespace("mipi_dsi")
