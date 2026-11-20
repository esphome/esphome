from dataclasses import dataclass, field
import functools

from esphome import automation
import esphome.codegen as cg
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
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority, entity_helpers
from esphome.core.entity_helpers import (
    ApplyEntity,
    SubEntities,
    entity_duplicate_validator,
    mqtt_component_class,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObj, MockObjClass, TemplateArguments
from esphome.cpp_types import global_ns
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "select"
IS_PLATFORM_COMPONENT = True

select_ns = cg.esphome_ns.namespace("select")
Select = select_ns.class_("Select", cg.EntityBase)
SelectPtr = Select.operator("ptr")
ApplySelect = select_ns.class_("ApplySelect", Select)

# Conditions
SelectIsCondition = select_ns.class_("SelectIsCondition", automation.Condition)

# Enums
SelectOperation = select_ns.enum("SelectOperation")
SELECT_OPERATION_OPTIONS = SelectOperation.options(
    "NEXT", "PREVIOUS", "FIRST", "LAST", prefix="SELECT_OP_"
)


@dataclass
class SelectData:
    # Options of each select whose option list never changes at run time, by id
    fixed_options: dict[str, list[str]] = field(default_factory=dict)


def _get_data() -> SelectData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SelectData()
    return CORE.data[DOMAIN]


def _record_options(
    config: ConfigType, options: list[str], fixed_options: bool
) -> None:
    if fixed_options:
        _get_data().fixed_options[config[CONF_ID].id] = options


_SELECT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(entity_helpers.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt_component_class("MQTTSelectComponent")
            ),
            cv.GenerateID(): cv.declare_id(Select),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(),
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
    if options:
        table = cg.shared_progmem_array("select_options", cg.const_char_ptr, options)
        cg.add(var.traits.set_options_static(table, len(options)))

    for conf in config.get(CONF_ON_VALUE, []):
        # The callback carries the index; the automation gets the option text as well.
        parent = automation.parent_ref(var)
        index = cg.RawExpression("index")
        await automation.build_callback_automation(
            var,
            "add_on_state_callback",
            [(cg.StringRef, "x"), (cg.size_t, "i")],
            conf,
            params=[(cg.size_t, "index")],
            forward=[cg.StringRef(parent.option_at(index)), index],
        )

    await entity_helpers.setup_entity_integrations(var, config)


async def register_select(
    var, config, *, options: list[str], fixed_options: bool = False
):
    """``fixed_options=True`` when the options never change at run time, so ``select.set`` can use the index."""
    _record_options(config, options, fixed_options)
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("select", config)
    CORE.register_platform_component("select", var)
    await setup_select_core_(var, config, options=options)


async def new_select(config, *args, options: list[str], fixed_options: bool = False):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_select(var, config, options=options, fixed_options=fixed_options)
    return var


APPLY_ENTITY = ApplyEntity(
    ApplySelect,
    ((SelectPtr, "entity"), (cg.size_t, "index")),
    "entity->publish_state(index);",
    (("option", "const char *option = entity->option_at(index);"),),
)


def sub_selects(
    config: ConfigType, *, parent: MockObj | ID | None = None
) -> SubEntities:
    """Return a SubEntities bound to new_select; ``apply`` statements can use ``entity``, ``index`` and ``option``."""
    # Hub selects get their options from code generation only
    return SubEntities(
        functools.partial(new_select, fixed_options=True), config, parent, APPLY_ENTITY
    )


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(select_ns.using)


OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Select),
    }
)


_SELECT_SET_SCHEMA = OPERATION_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTION): cv.templatable(cv.string_strict),
    }
)


def _option_index(config: ConfigType, option: str) -> str:
    return str(_get_data().fixed_options[config[CONF_ID].id].index(option))


_set_option_builder = automation.apply_action_builder(
    "select.set",
    _SELECT_SET_SCHEMA,
    automation.ApplyField(
        CONF_OPTION,
        "set_option",
        cg.std_string,
        const_fn=automation.literal_with_length,
    ),
    call="make_call",
)
_set_index_builder = automation.apply_action_builder(
    "select.set",
    _SELECT_SET_SCHEMA,
    automation.ApplyField(CONF_OPTION, "set_index", cg.size_t, const_fn=_option_index),
    call="make_call",
)


@automation.register_action(
    "select.set", automation.ApplyAction, _SELECT_SET_SCHEMA, synchronous=True
)
async def select_set_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    # A constant fixed option becomes its index
    await cg.get_variable(config[CONF_ID])
    option = config[CONF_OPTION]
    if not cg.is_template(option) and option in _get_data().fixed_options.get(
        config[CONF_ID].id, ()
    ):
        return await _set_index_builder(config, action_id, template_arg, args)
    return await _set_option_builder(config, action_id, template_arg, args)


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
        # Shared flash table of option pointers, length passed as a template argument
        arg = cg.shared_progmem_array("select_is_options", cg.const_char_ptr, options)
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
