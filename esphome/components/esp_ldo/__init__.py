from esphome.automation import Action, register_action
import esphome.codegen as cg
from esphome.components.esp32 import VARIANT_ESP32P4, only_on_variant
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_VOLTAGE
from esphome.final_validate import full_config

CODEOWNERS = ["@clydebarrow"]

DOMAIN = "esp_ldo"

esp_ldo_ns = cg.esphome_ns.namespace("esp_ldo")
EspLdo = esp_ldo_ns.class_("EspLdo", cg.Component)
AdjustAction = esp_ldo_ns.class_("AdjustAction", Action)

CHANNELS = (1, 2, 3, 4)
CHANNELS_INTERNAL = (1, 2)
CONF_ADJUSTABLE = "adjustable"
CONF_ALLOW_INTERNAL_CHANNEL = "allow_internal_channel"
CONF_PASSTHROUGH = "passthrough"

adjusted_ids = set()


def validate_ldo_voltage(value):
    if isinstance(value, str) and value.lower() == CONF_PASSTHROUGH:
        return CONF_PASSTHROUGH
    value = cv.voltage(value)
    if 0.5 <= value <= 2.7:
        return value
    raise cv.Invalid(
        f"LDO voltage must be in range 0.5V-2.7V or 'passthrough' (bypass mode), got {value}V"
    )


def validate_ldo_config(config):
    channel = config[CONF_CHANNEL]
    allow_internal = config[CONF_ALLOW_INTERNAL_CHANNEL]
    if allow_internal and channel not in CHANNELS_INTERNAL:
        raise cv.Invalid(
            f"'{CONF_ALLOW_INTERNAL_CHANNEL}' is only valid for internal channels (1, 2). "
            f"Channel {channel} is a user-configurable channel — its usage depends on your board schematic.",
            path=[CONF_ALLOW_INTERNAL_CHANNEL],
        )
    if channel in CHANNELS_INTERNAL and not allow_internal:
        raise cv.Invalid(
            f"LDO channel {channel} is normally used internally by the chip (flash/PSRAM). "
            f"Set '{CONF_ALLOW_INTERNAL_CHANNEL}: true' to confirm you know what you are doing.",
            path=[CONF_CHANNEL],
        )
    if config[CONF_VOLTAGE] == CONF_PASSTHROUGH and config[CONF_ADJUSTABLE]:
        raise cv.Invalid(
            "Passthrough mode passes the supply voltage directly to the output and does not support "
            "runtime voltage adjustment.",
            path=[CONF_ADJUSTABLE],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.All(
            cv.COMPONENT_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(EspLdo),
                    cv.Required(CONF_VOLTAGE): validate_ldo_voltage,
                    cv.Required(CONF_CHANNEL): cv.one_of(*CHANNELS, int=True),
                    cv.Optional(CONF_ADJUSTABLE, default=False): cv.boolean,
                    cv.Optional(CONF_ALLOW_INTERNAL_CHANNEL, default=False): cv.boolean,
                }
            ),
            validate_ldo_config,
        )
    ),
    cv.only_on_esp32,
    only_on_variant(supported=[VARIANT_ESP32P4]),
)


async def to_code(configs):
    for config in configs:
        var = cg.new_Pvariable(config[CONF_ID], config[CONF_CHANNEL])
        await cg.register_component(var, config)
        voltage = config[CONF_VOLTAGE]
        if voltage == CONF_PASSTHROUGH:
            cg.add(var.set_voltage(3300))
        else:
            cg.add(var.set_voltage(int(round(voltage * 1000))))
        cg.add(var.set_adjustable(config[CONF_ADJUSTABLE]))


def final_validate(configs):
    for channel in CHANNELS:
        used = [config for config in configs if config[CONF_CHANNEL] == channel]
        if len(used) > 1:
            raise cv.Invalid(
                f"Multiple LDOs configured for channel {channel}. Each channel must be unique.",
                path=[CONF_CHANNEL, channel],
            )

    global_config = full_config.get()
    for w in adjusted_ids:
        path = global_config.get_path_for_id(w)
        ldo_conf = global_config.get_config_for_path(path[:-1])
        if not ldo_conf[CONF_ADJUSTABLE]:
            raise cv.Invalid(
                "A non adjustable LDO may not be adjusted.",
                path,
            )


FINAL_VALIDATE_SCHEMA = final_validate


def adjusted_ldo_id(value):
    value = cv.use_id(EspLdo)(value)
    adjusted_ids.add(value)
    return value


@register_action(
    "esp_ldo.voltage.adjust",
    AdjustAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): adjusted_ldo_id,
            cv.Required(CONF_VOLTAGE): cv.templatable(
                cv.All(cv.voltage, cv.float_range(min=0.5, max=2.7))
            ),
        }
    ),
    synchronous=True,
)
async def ldo_voltage_adjust_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_VOLTAGE], args, cg.float_)
    cg.add(var.set_voltage(template_))
    return var
