import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ADDRESS, CONF_COMMAND, CONF_ID, CONF_DURATION
from esphome import automation
from esphome.automation import maybe_simple_id

CODEOWNERS = ["@carlos-sarmiento"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_VOLUME = "volume"
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

# Actions that do not require more arguments

EzoPMPFindAction = ezo_pmp_ns.class_("EzoPMPFindAction", automation.Action)
EzoPMPClearTotalVolumeDispensedAction = ezo_pmp_ns.class_(
    "EzoPMPClearTotalVolumeDispensedAction", automation.Action
)
EzoPMPClearCalibrationAction = ezo_pmp_ns.class_(
    "EzoPMPClearCalibrationAction", automation.Action
)
EzoPMPPauseDosingAction = ezo_pmp_ns.class_(
    "EzoPMPPauseDosingAction", automation.Action
)
EzoPMPStopDosingAction = ezo_pmp_ns.class_("EzoPMPStopDosingAction", automation.Action)
EzoPMPDoseContinuouslyAction = ezo_pmp_ns.class_(
    "EzoPMPDoseContinuouslyAction", automation.Action
)

# Actions that require more arguments
EzoPMPDoseVolumeAction = ezo_pmp_ns.class_("EzoPMPDoseVolumeAction", automation.Action)
EzoPMPDoseVolumeOverTimeAction = ezo_pmp_ns.class_(
    "EzoPMPDoseVolumeOverTimeAction", automation.Action
)
EzoPMPDoseWithConstantFlowRateAction = ezo_pmp_ns.class_(
    "EzoPMPDoseWithConstantFlowRateAction", automation.Action
)
EzoPMPSetCalibrationVolumeAction = ezo_pmp_ns.class_(
    "EzoPMPSetCalibrationVolumeAction", automation.Action
)
EzoPMPChangeI2CAddressAction = ezo_pmp_ns.class_(
    "EzoPMPChangeI2CAddressAction", automation.Action
)
EzoPMPArbitraryCommandAction = ezo_pmp_ns.class_(
    "EzoPMPArbitraryCommandAction", automation.Action
)


@automation.register_action(
    "ezo_pmp.find", EzoPMPFindAction, EZO_PMP_NO_ARGS_ACTION_SCHEMA
)
async def ezo_pmp_find_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "ezo_pmp.dose_continuously",
    EzoPMPDoseContinuouslyAction,
    EZO_PMP_NO_ARGS_ACTION_SCHEMA,
)
async def ezo_pmp_dose_continuously_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "ezo_pmp.clear_total_volume_dosed",
    EzoPMPClearTotalVolumeDispensedAction,
    EZO_PMP_NO_ARGS_ACTION_SCHEMA,
)
async def ezo_pmp_clear_total_volume_dosed_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "ezo_pmp.clear_calibration",
    EzoPMPClearCalibrationAction,
    EZO_PMP_NO_ARGS_ACTION_SCHEMA,
)
async def ezo_pmp_clear_calibration_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "ezo_pmp.pause_dosing", EzoPMPPauseDosingAction, EZO_PMP_NO_ARGS_ACTION_SCHEMA
)
async def ezo_pmp_pause_dosing_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "ezo_pmp.stop_dosing", EzoPMPStopDosingAction, EZO_PMP_NO_ARGS_ACTION_SCHEMA
)
async def ezo_pmp_stop_dosing_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# Actions that require Multiple Args

EZO_PMP_DOSE_VOLUME_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_VOLUME): cv.templatable(
            cv.float_range()
        ),  # Any way to represent as proper volume (vs. raw int)
    }
)


@automation.register_action(
    "ezo_pmp.dose_volume", EzoPMPDoseVolumeAction, EZO_PMP_DOSE_VOLUME_ACTION_SCHEMA
)
async def ezo_pmp_dose_volume_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_VOLUME], args, cg.double)
    cg.add(var.set_volume(template_))

    return var


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


@automation.register_action(
    "ezo_pmp.dose_volume_over_time",
    EzoPMPDoseVolumeOverTimeAction,
    EZO_PMP_DOSE_VOLUME_OVER_TIME_ACTION_SCHEMA,
)
async def ezo_pmp_dose_volume_over_time_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_VOLUME], args, cg.double)
    cg.add(var.set_volume(template_))

    template_ = await cg.templatable(config[CONF_DURATION], args, int)
    cg.add(var.set_duration(template_))

    return var


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


@automation.register_action(
    "ezo_pmp.dose_with_constant_flow_rate",
    EzoPMPDoseWithConstantFlowRateAction,
    EZO_PMP_DOSE_WITH_CONSTANT_FLOW_RATE_ACTION_SCHEMA,
)
async def ezo_pmp_dose_with_constant_flow_rate_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_VOLUME_PER_MINUTE], args, cg.double)
    cg.add(var.set_volume(template_))

    template_ = await cg.templatable(config[CONF_DURATION], args, int)
    cg.add(var.set_duration(template_))

    return var


EZO_PMP_SET_CALIBRATION_VOLUME_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_VOLUME): cv.templatable(
            cv.float_range()
        ),  # Any way to represent as proper volume (vs. raw int)
    }
)


@automation.register_action(
    "ezo_pmp.set_calibration_volume",
    EzoPMPSetCalibrationVolumeAction,
    EZO_PMP_SET_CALIBRATION_VOLUME_ACTION_SCHEMA,
)
async def ezo_pmp_set_calibration_volume_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_VOLUME], args, cg.double)
    cg.add(var.set_volume(template_))

    return var


EZO_PMP_CHANGE_I2C_ADDRESS_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.int_range(min=1, max=127)),
    }
)


@automation.register_action(
    "ezo_pmp.change_i2c_address",
    EzoPMPChangeI2CAddressAction,
    EZO_PMP_CHANGE_I2C_ADDRESS_ACTION_SCHEMA,
)
async def ezo_pmp_change_i2c_address_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.double)
    cg.add(var.set_address(template_))

    return var


EZO_PMP_ARBITRARY_COMMAND_ACTION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(EzoPMP),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "ezo_pmp.arbitrary_command",
    EzoPMPArbitraryCommandAction,
    EZO_PMP_ARBITRARY_COMMAND_ACTION_SCHEMA,
)
async def ezo_pmp_arbitrary_command_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(template_))

    return var
