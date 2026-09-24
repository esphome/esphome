from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_CYCLE,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INDEX,
    CONF_LAMBDA,
    CONF_MODE,
    CONF_MQTT_ID,
    CONF_ON_VALUE,
    CONF_OPERATION,
    CONF_OPTION,
    CONF_OPTIONS,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObjClass, TemplateArguments
from esphome.cpp_types import global_ns

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

select_ns = cg.esphome_ns.namespace("select")
Select = select_ns.class_("Select", cg.EntityBase)
SelectPtr = Select.operator("ptr")

# Triggers
SelectStateTrigger = select_ns.class_(
    "SelectStateTrigger",
    automation.Trigger.template(cg.StringRef, cg.size_t),
)

# Conditions
SelectIsCondition = select_ns.class_("SelectIsCondition", automation.Condition)

# Enums
SelectOperation = select_ns.enum("SelectOperation")
SELECT_OPERATION_OPTIONS = {
    "NEXT": SelectOperation.SELECT_OP_NEXT,
    "PREVIOUS": SelectOperation.SELECT_OP_PREVIOUS,
    "FIRST": SelectOperation.SELECT_OP_FIRST,
    "LAST": SelectOperation.SELECT_OP_LAST,
}


_SELECT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTSelectComponent),
            cv.GenerateID(): cv.declare_id(Select),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SelectStateTrigger),
                }
            ),
        }
    )
)


_SELECT_SCHEMA.add_extra(entity_duplicate_validator("select"))


def select_schema(
    class_: MockObjClass,
    *,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
):
    schema = {cv.GenerateID(): cv.declare_id(class_)}

    for key, default, validator in [
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _SELECT_SCHEMA.extend(schema)


@setup_entity("select")
async def setup_select_core_(var, config, *, options: list[str]):
    cg.add(var.traits.set_options(options))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.StringRef, "x"), (cg.size_t, "i")], conf
        )

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_select(var, config, *, options: list[str]):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("select", config)
    CORE.register_platform_component("select", var)
    await setup_select_core_(var, config, options=options)


async def new_select(config, *args, options: list[str]):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_select(var, config, options=options)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(select_ns.using)


OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Select),
    }
)


automation.register_apply_action(
    "select.set",
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_OPTION): cv.templatable(cv.string_strict),
        }
    ),
    automation.ApplyField(CONF_OPTION, "set_option", cg.std_string),
    call="make_call",
)

automation.register_apply_action(
    "select.set_index",
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_INDEX): cv.templatable(cv.positive_int),
        }
    ),
    automation.ApplyField(CONF_INDEX, "set_index", cg.size_t),
    call="make_call",
)


@automation.register_condition(
    "select.is",
    SelectIsCondition,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Optional(CONF_OPTIONS): cv.All(
                cv.ensure_list(cv.string_strict), cv.Length(min=1)
            ),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    ).add_extra(cv.has_exactly_one_key(CONF_OPTIONS, CONF_LAMBDA)),
)
async def select_is_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    if options := config.get(CONF_OPTIONS):
        # List of constant options
        # Create a constexpr and pass that with a template length
        arr_id = ID(
            f"{condition_id}_data",
            is_declaration=True,
            type=global_ns.namespace("constexpr char * const"),
        )
        arg = cg.static_const_array(arr_id, cg.ArrayInitializer(*options))
        template_arg = TemplateArguments(len(options), *template_arg)
    else:
        # Lambda
        arg = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(global_ns.namespace("StringRef &").operator("const"), "current")] + args,
            return_type=cg.bool_,
        )
        template_arg = TemplateArguments(0, *template_arg)
    return cg.new_Pvariable(condition_id, template_arg, paren, arg)


automation.register_apply_action(
    "select.operation",
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_OPERATION): cv.templatable(
                cv.enum(SELECT_OPERATION_OPTIONS, upper=True)
            ),
            cv.Optional(CONF_CYCLE, default=True): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyField(CONF_OPERATION, "with_operation", SelectOperation),
    automation.ApplyField(CONF_CYCLE, "with_cycle", cg.bool_),
    call="make_call",
)

# The operation is fixed by the action name; CONF_MODE only stays accepted in the config.
for _name, _mode, _cycle in (
    ("select.next", "NEXT", True),
    ("select.previous", "PREVIOUS", True),
    ("select.first", "FIRST", False),
    ("select.last", "LAST", False),
):
    _schema = {cv.Optional(CONF_MODE, default=_mode): cv.one_of(_mode, upper=True)}
    _fields = [
        automation.ApplyCall(f"with_operation({SELECT_OPERATION_OPTIONS[_mode]})")
    ]
    if _cycle:
        _schema[cv.Optional(CONF_CYCLE, default=True)] = cv.boolean
        _fields.append(automation.ApplyField(CONF_CYCLE, "with_cycle", cg.bool_))
    automation.register_apply_action(
        _name,
        automation.maybe_simple_id(OPERATION_BASE_SCHEMA.extend(_schema)),
        *_fields,
        call="make_call",
    )
