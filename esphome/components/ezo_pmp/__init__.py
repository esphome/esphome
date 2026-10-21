from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_COMMAND,
    CONF_DURATION,
    CONF_ID,
    CONF_VOLUME,
)

CODEOWNERS = ["@carlos-sarmiento"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_VOLUME_PER_MINUTE = "volume_per_minute"

ezo_pmp_ns = cg.esphome_ns.namespace("ezo_pmp")
EzoPMP = ezo_pmp_ns.class_("EzoPMP", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EzoPMP),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(103))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


EZO_PMP_NO_ARGS_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
    }
)

for _name, _method in (
    ("ezo_pmp.find", "find()"),
    ("ezo_pmp.dose_continuously", "dose_continuously()"),
    ("ezo_pmp.clear_total_volume_dosed", "clear_total_volume_dosed()"),
    ("ezo_pmp.clear_calibration", "clear_calibration()"),
    ("ezo_pmp.pause_dosing", "pause_dosing()"),
    ("ezo_pmp.stop_dosing", "stop_dosing()"),
):
    automation.register_apply_action(
        _name, EZO_PMP_NO_ARGS_ACTION_SCHEMA, automation.ApplyCall(_method)
    )


EZO_PMP_DOSE_VOLUME_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_VOLUME): cv.templatable(
            cv.float_range()
        ),  # Any way to represent as proper volume (vs. raw int)
    }
)

automation.register_apply_action(
    "ezo_pmp.dose_volume",
    EZO_PMP_DOSE_VOLUME_ACTION_SCHEMA,
    automation.ApplyField(CONF_VOLUME, "dose_volume", cg.double),
)


EZO_PMP_DOSE_VOLUME_OVER_TIME_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_VOLUME): cv.templatable(
            cv.float_range()
        ),  # Any way to represent as proper volume (vs. raw int)
        cv.Required(CONF_DURATION): cv.templatable(
            cv.int_range(1)
        ),  # Any way to represent it as minutes (vs. raw int)
    }
)

automation.register_apply_action(
    "ezo_pmp.dose_volume_over_time",
    EZO_PMP_DOSE_VOLUME_OVER_TIME_ACTION_SCHEMA,
    automation.ApplyCall(
        "dose_volume_over_time({}, {})",
        ((CONF_VOLUME, cg.double), (CONF_DURATION, cg.int_)),
    ),
)


EZO_PMP_DOSE_WITH_CONSTANT_FLOW_RATE_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_VOLUME_PER_MINUTE): cv.templatable(
            cv.float_range()
        ),  # Any way to represent as proper volume (vs. raw int)
        cv.Required(CONF_DURATION): cv.templatable(
            cv.int_range(1)
        ),  # Any way to represent it as minutes (vs. raw int)
    }
)

automation.register_apply_action(
    "ezo_pmp.dose_with_constant_flow_rate",
    EZO_PMP_DOSE_WITH_CONSTANT_FLOW_RATE_ACTION_SCHEMA,
    automation.ApplyCall(
        "dose_with_constant_flow_rate({}, {})",
        ((CONF_VOLUME_PER_MINUTE, cg.double), (CONF_DURATION, cg.int_)),
    ),
)


EZO_PMP_SET_CALIBRATION_VOLUME_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_VOLUME): cv.templatable(
            cv.float_range()
        ),  # Any way to represent as proper volume (vs. raw int)
    }
)

automation.register_apply_action(
    "ezo_pmp.set_calibration_volume",
    EZO_PMP_SET_CALIBRATION_VOLUME_ACTION_SCHEMA,
    automation.ApplyField(CONF_VOLUME, "set_calibration_volume", cg.double),
)


EZO_PMP_CHANGE_I2C_ADDRESS_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.int_range(min=1, max=127)),
    }
)

automation.register_apply_action(
    "ezo_pmp.change_i2c_address",
    EZO_PMP_CHANGE_I2C_ADDRESS_ACTION_SCHEMA,
    automation.ApplyField(CONF_ADDRESS, "change_i2c_address", cg.int_),
)


EZO_PMP_ARBITRARY_COMMAND_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string_strict),
    }
)

automation.register_apply_action(
    "ezo_pmp.arbitrary_command",
    EZO_PMP_ARBITRARY_COMMAND_ACTION_SCHEMA,
    automation.ApplyField(CONF_COMMAND, "exec_arbitrary_command", cg.std_string),
)
