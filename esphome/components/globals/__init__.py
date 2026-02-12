import hashlib

from esphome import automation, codegen as cg, config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_RESTORE_VALUE,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
globals_ns = cg.esphome_ns.namespace("globals")
GlobalsComponent = globals_ns.class_("GlobalsComponent", cg.Component)
RTCGlobalsComponent = globals_ns.class_("RTCGlobalsComponent", cg.Component)
RestoringGlobalsComponent = globals_ns.class_(
    "RestoringGlobalsComponent", cg.PollingComponent
)
RestoringGlobalStringComponent = globals_ns.class_(
    "RestoringGlobalStringComponent", cg.PollingComponent
)
GlobalVarSetAction = globals_ns.class_("GlobalVarSetAction", automation.Action)

CONF_MAX_RESTORE_DATA_LENGTH = "max_restore_data_length"

RTC_RESTORE_MODE = "RTC"
PRIMITIVE_TYPES = {
    "bool",
    "char",
    "float",
    "double",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
}


def extract_primitive_type(type_str):
    # Extract the primitive type and length from stuff like uint8_t[10]
    if "[" not in type_str:
        return type_str, None
    primitive_type, length_str = type_str.split("[", 1)
    length = int(length_str.rstrip("]"))
    return primitive_type, length


# Base schema fields shared by both variants
_BASE_SCHEMA = {
    cv.Required(CONF_ID): cv.declare_id(GlobalsComponent),
    cv.Required(CONF_TYPE): cv.string_strict,
    cv.Optional(CONF_INITIAL_VALUE): cv.string_strict,
    cv.Optional(CONF_MAX_RESTORE_DATA_LENGTH): cv.int_range(0, 254),
}


def validate_restore_mode(value):
    value = cv.one_of(RTC_RESTORE_MODE, upper=True)(value)
    # Only RTC is available in ESP32
    if isinstance(value, str) and value == RTC_RESTORE_MODE and not CORE.is_esp32:
        raise cv.Invalid(
            f"RTC restore mode is only supported on ESP32 platforms, but current platform is {CORE.target_platform}"
        )
    return value


def validate_primitive_types(value):
    value = cv.string_strict(value)
    primitive_type, _ = extract_primitive_type(value)
    if primitive_type not in PRIMITIVE_TYPES:
        raise cv.Invalid(f"Type must be one of {PRIMITIVE_TYPES}, but got '{value}'")
    return value


# RTC-restoring globals: regular Component (no polling needed)
_RTC_RESTORING_SCHEMA = cv.Schema(
    {
        **_BASE_SCHEMA,
        cv.Required(CONF_TYPE): validate_primitive_types,
        cv.Optional(CONF_RESTORE_VALUE, default="RTC"): validate_restore_mode,
    }
).extend(cv.COMPONENT_SCHEMA)

# Non-restoring globals: regular Component (no polling needed)
_NON_RESTORING_SCHEMA = cv.Schema(
    {
        **_BASE_SCHEMA,
        cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

# Restoring globals: PollingComponent with configurable update_interval
_RESTORING_SCHEMA = cv.Schema(
    {
        **_BASE_SCHEMA,
        cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
    }
).extend(cv.polling_component_schema("1s"))


def _globals_schema(config: ConfigType) -> ConfigType:
    """Select schema based on restore_value setting."""
    restore_val = config.get(CONF_RESTORE_VALUE, False)
    if restore_val is True:
        return _RESTORING_SCHEMA(config)
    if restore_val == RTC_RESTORE_MODE:
        return _RTC_RESTORING_SCHEMA(config)
    return _NON_RESTORING_SCHEMA(config)


MULTI_CONF = True
CONFIG_SCHEMA = _globals_schema


# Run with low priority so that namespaces are registered first
@coroutine_with_priority(CoroPriority.LATE)
async def to_code(config):
    type_ = cg.RawExpression(config[CONF_TYPE])
    restore = config[CONF_RESTORE_VALUE]
    initial_value = None
    if CONF_INITIAL_VALUE in config:
        initial_value = cg.RawExpression(config[CONF_INITIAL_VALUE])

    if restore == RTC_RESTORE_MODE:
        # RTC restore mode uses a global variable to store the values
        rtc_var_id = f"{config[CONF_ID]}RTC"
        primitive_type, length = extract_primitive_type(str(type_))
        if length is None:
            rtc_var_exp = f"RTC_DATA_ATTR {primitive_type} {rtc_var_id}"
        else:
            rtc_var_exp = f"RTC_DATA_ATTR {primitive_type} {rtc_var_id}[{length}]"
        if initial_value:
            rtc_var_exp += f" = {initial_value}"
        cg.add_global(cg.RawExpression(rtc_var_exp))
        template_args = cg.TemplateArguments(type_)
        type = RTCGlobalsComponent
        res_type = type.template(template_args)
        rhs = type.new(template_args, cg.RawExpression(f"&{rtc_var_id}"))
    else:
        if str(type_) == "std::string" and restore:
            # Special casing the strings to their own class with a different save/restore mechanism
            template_args = cg.TemplateArguments(
                type_, config.get(CONF_MAX_RESTORE_DATA_LENGTH, 63) + 1
            )
            type = RestoringGlobalStringComponent
        else:
            template_args = cg.TemplateArguments(type_)
            type = RestoringGlobalsComponent if restore else GlobalsComponent
        res_type = type.template(template_args)
        rhs = type.new(template_args, initial_value)
    glob = cg.Pvariable(config[CONF_ID], rhs, res_type)
    await cg.register_component(glob, config)

    if restore is True:
        value = config[CONF_ID].id
        if isinstance(value, str):
            value = value.encode()
        hash_ = int(hashlib.md5(value).hexdigest()[:8], 16)
        cg.add(glob.set_name_hash(hash_))


@automation.register_action(
    "globals.set",
    GlobalVarSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GlobalsComponent),
            cv.Required(CONF_VALUE): cv.templatable(cv.string_strict),
        }
    ),
)
async def globals_set_to_code(config, action_id, template_arg, args):
    full_id, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    var = cg.new_Pvariable(action_id, template_arg, paren)
    templ = await cg.templatable(
        config[CONF_VALUE], args, None, to_exp=cg.RawExpression
    )
    cg.add(var.set_value(templ))
    return var
