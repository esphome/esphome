import esphome.codegen as cg

CODEOWNERS = ["@Passific", "@koosoli", "@limengdu"]
DOMAIN = "it8951"

register_enable_pin = cg.slot_list(
    "IT8951_ENABLE_PIN_COUNT", "register_enable_pin", DOMAIN
)
