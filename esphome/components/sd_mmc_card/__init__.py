from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = []

# Mark that this component requires VFS directory support
# This must be set early so ESP32 component can check it during its own to_code()
esp32.require_vfs_dir()

sd_mmc_card_ns = cg.esphome_ns.namespace("sd_mmc_card")
SdMmc = sd_mmc_card_ns.class_("SdMmc", cg.Component)

CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_POWER_CTRL_PIN = "power_ctrl_pin"
CONF_SLOT = "slot"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdMmc),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
        cv.Optional(CONF_POWER_CTRL_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_SLOT, default=0): cv.int_range(min=0, max=1),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set mode and slot first
    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))
    cg.add(var.set_slot(config[CONF_SLOT]))

    # Set pins
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    # Only set data pins if not in 1-bit mode
    if not config[CONF_MODE_1BIT]:
        if CONF_DATA1_PIN in config:
            cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        if CONF_DATA2_PIN in config:
            cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        if CONF_DATA3_PIN in config:
            cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    if CONF_POWER_CTRL_PIN in config:
        cg.add(var.set_power_ctrl_pin(config[CONF_POWER_CTRL_PIN]))

    # Store device reference in CORE.data for storage_host to access
    # This allows storage_host to register callbacks with SD MMC card
    from esphome.core import CORE

    if not hasattr(CORE, "data"):
        CORE.data = {}
    if "sd_mmc_devices" not in CORE.data:
        CORE.data["sd_mmc_devices"] = []
    CORE.data["sd_mmc_devices"].append(var)
