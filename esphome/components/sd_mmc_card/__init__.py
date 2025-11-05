from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = []

# Mark that this component requires VFS directory support
# This must be set early so ESP32 component can check it during its own to_code()
esp32.require_vfs_dir()

sd_mmc_card_ns = cg.esphome_ns.namespace("sd_mmc_card")
SdMmc = sd_mmc_card_ns.class_("SdMmc", cg.Component)

# Automation classes
CardMountedTrigger = sd_mmc_card_ns.class_(
    "CardMountedTrigger", automation.Trigger.template(cg.std_string)
)
MountCardAction = sd_mmc_card_ns.class_("MountCardAction", automation.Action)
UnmountCardAction = sd_mmc_card_ns.class_("UnmountCardAction", automation.Action)
ListFilesAction = sd_mmc_card_ns.class_("ListFilesAction", automation.Action)
CardMountedCondition = sd_mmc_card_ns.class_(
    "CardMountedCondition", automation.Condition
)

CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_POWER_CTRL_PIN = "power_ctrl_pin"
CONF_SLOT = "slot"
CONF_ON_MOUNTED = "on_mounted"

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
        cv.Optional(CONF_ON_MOUNTED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardMountedTrigger),
            }
        ),
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

    # Register on_mounted trigger
    for conf in config.get(CONF_ON_MOUNTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "mount_path")], conf
        )


# Actions
SD_MMC_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SdMmc),
    }
)


@automation.register_action("sd_mmc_card.mount", MountCardAction, SD_MMC_ACTION_SCHEMA)
async def sd_mmc_mount_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sd_mmc_card.unmount", UnmountCardAction, SD_MMC_ACTION_SCHEMA
)
async def sd_mmc_unmount_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


LIST_FILES_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(SdMmc),
        cv.Optional("path", default=""): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "sd_mmc_card.list_files", ListFilesAction, LIST_FILES_SCHEMA
)
async def sd_mmc_list_files_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if "path" in config:
        template_ = await cg.templatable(config["path"], args, cg.std_string)
        cg.add(var.set_path(template_))
    return var


# Conditions
@automation.register_condition(
    "sd_mmc_card.is_mounted", CardMountedCondition, SD_MMC_ACTION_SCHEMA
)
async def sd_mmc_is_mounted_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
