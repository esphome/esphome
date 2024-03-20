import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_TRIGGER_ID,
    CONF_NUMBER,
    CONF_INVERTED,
    CONF_MODE,
    CONF_BYTES,
)
from esphome import core, automation, pins

from .constants import (
    CONF_DEVICE_ID,
    CONF_REGISTER_ID,
    CONF_DEVICES,
    CONF_REGISTERS,
    CONF_READ_DELAY,
    CONF_MESSAGE,
    CONF_ON_SETUP_ID,
    CONF_ON_SETUP,
    CONF_MESSAGE_BUILDER_ID,
    CONF_MESSAGE_BUILDER_KEY,
    CONF_CACHE,
    CONF_PIN_BANKS,
    CONF_PIN_BANK_ID,
    KEY_CUSTOM_I2C,
    KEY_PIN_BANK_BYTE_COUNTS,
)

CODEOWNERS = ["@javawizard"]
DEPENDENCIES = ["i2c"]

custom_i2c_ns = cg.esphome_ns.namespace("custom_i2c")
CustomI2COnSetupComponent = custom_i2c_ns.class_(
    "CustomI2COnSetupComponent", cg.Component
)
CustomI2CDevice = custom_i2c_ns.class_("CustomI2CDevice", cg.Component)
CustomI2CRegister = custom_i2c_ns.class_("CustomI2CRegister", cg.Component)
WriteAction = custom_i2c_ns.class_("WriteAction", automation.Action)
MessageBuilder = custom_i2c_ns.class_("MessageBuilder")
ValueMessageBuilder = custom_i2c_ns.class_("ValueMessageBuilder", MessageBuilder)
BytesMessageBuilder = custom_i2c_ns.class_("BytesMessageBuilder", MessageBuilder)
VectorMessageBuilder = custom_i2c_ns.class_("VectorMessageBuilder", MessageBuilder)
CompositeMessageBuilder = custom_i2c_ns.class_(
    "CompositeMessageBuilder", MessageBuilder
)
CustomI2CPinBank = custom_i2c_ns.class_("CustomI2CPinBank", cg.Component)
CustomI2CPin = custom_i2c_ns.class_("CustomI2CPin", cg.GPIOPin)

PIN_BANK_REGISTER_TYPES = [
    "read_input_states",
    "write_output_states",
    "write_ones_to_set_outputs_high",
    "write_ones_to_set_outputs_low",
    "write_io_directions",
    "write_inverted_io_directions",
    "write_ones_to_set_to_inputs",
    "write_ones_to_set_to_outputs",
    "write_pull_resistors_enabled",
    "write_inverted_pull_resistors_enabled",
    "write_ones_to_enable_pull_resistors",
    "write_ones_to_disable_pull_resistors",
    "write_pull_directions",
    "write_inverted_pull_directions",
    "write_ones_to_enable_pullups",
    "write_ones_to_enable_pulldowns",
]

MESSAGE_BUILDER_KEYS = {
    "uint8_t": (cv.templatable(cv.hex_uint8_t), ValueMessageBuilder, cg.uint8),
    "uint16_t": (cv.templatable(cv.hex_uint16_t), ValueMessageBuilder, cg.uint16),
    "uint32_t": (cv.templatable(cv.hex_uint32_t), ValueMessageBuilder, cg.uint32),
    "uint64_t": (cv.templatable(cv.hex_uint64_t), ValueMessageBuilder, cg.uint64),
    "bool": (cv.templatable(cv.boolean), ValueMessageBuilder, cg.bool_),
    "vector": (cv.returning_lambda, VectorMessageBuilder, None),
    "message": (
        # pylint: disable=unnecessary-lambda
        cv.ensure_list(lambda obj: MESSAGE_BUILDER_SCHEMA(obj)),
        CompositeMessageBuilder,
        None,
    ),
}


def build_message_builder_schema(parent_schema):
    def validate(obj):
        if isinstance(obj, int):  # TODO: also check for lambdas
            obj = {"uint8_t": obj}

        obj = cv.has_exactly_one_key(*MESSAGE_BUILDER_KEYS.keys())(obj)
        key = [k for k in MESSAGE_BUILDER_KEYS if k in obj][0]

        child_value = obj.pop(key)
        child_validator, child_class, _ = MESSAGE_BUILDER_KEYS[key]

        obj = parent_schema(obj)

        obj[key] = child_validator(child_value)
        obj[CONF_MESSAGE_BUILDER_KEY] = key  # for lookup during codegen

        # Optimization for the common case of arrays of bytes
        if child_class.inherits_from(CompositeMessageBuilder) and all(
            "uint8_t" in m and isinstance(m["uint8_t"], int) for m in obj[key]
        ):
            child_class = BytesMessageBuilder

        obj = cv.Schema(
            {cv.GenerateID(CONF_MESSAGE_BUILDER_ID): cv.declare_id(child_class)},
            extra=cv.ALLOW_EXTRA,
        )(obj)

        return obj

    return validate


MESSAGE_BUILDER_SCHEMA = build_message_builder_schema(cv.Schema({}))

# TODO: consider merging with SINGLE_MESSAGE_SCHEMA into a single type that denotes a sequence of bytes that form all
# or some portion of a message
REGISTER_ADDRESS_SCHEMA = cv.ensure_list(cv.hex_uint8_t)

# TODO: make these templatable, probably (or have separate schemas for templatable messages)
SINGLE_MESSAGE_SCHEMA = cv.ensure_list(cv.hex_uint8_t)
MESSAGE_LIST_SCHEMA = [SINGLE_MESSAGE_SCHEMA]

REGISTER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomI2CRegister),
        cv.Required(CONF_ADDRESS): REGISTER_ADDRESS_SCHEMA,
        cv.Optional(CONF_READ_DELAY): cv.positive_time_period_microseconds,
    }
)


PIN_BANK_REGISTER_SCHEMA = automation.maybe_conf(
    CONF_REGISTER_ID,
    cv.Schema(
        {
            cv.Required(CONF_REGISTER_ID): cv.use_id(CustomI2CRegister),
            cv.Optional(CONF_CACHE, False): cv.boolean,
        }
    ),
)

PIN_BANK_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomI2CPinBank),
        cv.Required(CONF_BYTES): cv.uint8_t,
        **{cv.Optional(t): PIN_BANK_REGISTER_SCHEMA for t in PIN_BANK_REGISTER_TYPES},
    }
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DEVICES, []): cv.ensure_list(
            i2c.i2c_device_schema(None).extend(
                {
                    cv.GenerateID(): cv.declare_id(CustomI2CDevice),
                    cv.Optional(
                        CONF_READ_DELAY, cv.TimePeriodMicroseconds()
                    ): cv.positive_time_period_microseconds,
                    cv.Optional(CONF_REGISTERS, []): cv.ensure_list(REGISTER_SCHEMA),
                }
            )
        ),
        cv.Optional(CONF_PIN_BANKS, []): cv.ensure_list(PIN_BANK_SCHEMA),
        # CONF_ON_SETUP_ID is not meant to be directly used; it's just here to get esphome to generate an ID for use to
        # use. (TODO: see if there's a way to do this that doesn't involve an otherwise unused config key)
        cv.GenerateID(CONF_ON_SETUP_ID): cv.declare_id(CustomI2COnSetupComponent),
        cv.Optional(CONF_ON_SETUP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template()
                ),
            }
        ),
    }
)


WRITE_ACTION_SCHEMA = automation.maybe_conf(
    CONF_MESSAGE,  # treat raw values or lists as messages to the (presumably) only existing device
    build_message_builder_schema(
        cv.Schema(
            {
                cv.Optional(CONF_DEVICE_ID): cv.use_id(CustomI2CDevice),
                cv.Optional(CONF_REGISTER_ID): cv.use_id(CustomI2CRegister),
            }
        )
    ),
    # Allow either device_id or register_id to be specified, and if neither is, insert a device_id entry that will
    # cause the automatic ID lookup logic to populate it with the sole device that exists (or throw an exception if
    # the user declared more than one).
    # There's got to be a better way to do this...
    cv.has_at_most_one_key(CONF_DEVICE_ID, CONF_REGISTER_ID),
    cv.Any(
        cv.has_exactly_one_key(CONF_DEVICE_ID, CONF_REGISTER_ID),
        lambda obj: obj | {CONF_DEVICE_ID: core.ID(None, type=CustomI2CDevice)},
    ),
)


async def build_message_builder(config: dict, lambda_template_args, lambda_args):
    message_builder_id = config[CONF_MESSAGE_BUILDER_ID]
    message_builder_key = config[CONF_MESSAGE_BUILDER_KEY]
    _, _, value_type = MESSAGE_BUILDER_KEYS[message_builder_key]

    if message_builder_id.type.inherits_from(CompositeMessageBuilder):
        message = config[CONF_MESSAGE]

        child_builders = [
            await build_message_builder(m, lambda_template_args, lambda_args)
            for m in message
        ]

        return cg.new_Pvariable(
            message_builder_id,
            cg.TemplateArguments(len(child_builders), *lambda_template_args),
            cg.ArrayInitializer(*child_builders),
        )

    # Optimization for the common case of arrays of bytes
    if message_builder_id.type.inherits_from(BytesMessageBuilder):
        message = config[CONF_MESSAGE]

        return cg.new_Pvariable(
            message_builder_id,
            cg.TemplateArguments(len(message), *lambda_template_args),
            cg.ArrayInitializer(*[m["uint8_t"] for m in message]),
        )

    if message_builder_id.type.inherits_from(ValueMessageBuilder):
        builder = cg.new_Pvariable(
            message_builder_id,
            cg.TemplateArguments(value_type, *lambda_template_args),
        )

        template = await cg.templatable(
            config[message_builder_key], lambda_args, value_type
        )
        cg.add(builder.set_value(template))

        return builder

    if message_builder_id.type.inherits_from(VectorMessageBuilder):
        builder = cg.new_Pvariable(message_builder_id, lambda_template_args)

        template = await cg.templatable(
            config[message_builder_key], lambda_args, cg.std_vector.template(cg.uint8)
        )
        cg.add(builder.set_vector(template))

        return builder

    raise AssertionError("unreachable")


async def build_pin_bank(config):
    pin_bank_id = config[CONF_ID]
    byte_count = config[CONF_BYTES]

    pin_bank_var = cg.new_Pvariable(config[CONF_ID], cg.TemplateArguments(byte_count))
    await cg.register_component(pin_bank_var, config)

    core.CORE.data.setdefault(KEY_CUSTOM_I2C, {}).setdefault(
        KEY_PIN_BANK_BYTE_COUNTS, {}
    )[pin_bank_id] = byte_count

    for register_type in PIN_BANK_REGISTER_TYPES:
        if register_type not in config:
            continue

        register_config = config[register_type]

        # should move this to validation
        if register_type == "read_input_states" and register_config[CONF_CACHE]:
            raise core.EsphomeError(
                "The 'read_input_states' register cannot be cached. (It wouldn't do much good if it were!)"
            )

        cg.add(
            getattr(pin_bank_var, f"{register_type}_register").set_register(
                await cg.get_variable(register_config[CONF_REGISTER_ID])
            )
        )

        if register_config[CONF_CACHE]:
            cg.add(getattr(pin_bank_var, f"{register_type}_register").enable_cache())


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_PIN_BANK_ID,
    pins.gpio_base_schema(CustomI2CPin, cv.uint8_t).extend(
        {cv.Required(CONF_PIN_BANK_ID): cv.use_id(CustomI2CPinBank)}
    ),
    # TODO: until we support open drain pin modes, validate that the pin mode isn't open drain
)
async def pin_to_code(config):
    pin_bank_id = config[CONF_PIN_BANK_ID]
    pin_bank_var = await cg.get_variable(config[CONF_PIN_BANK_ID])
    pin_bank_byte_count = core.CORE.data[KEY_CUSTOM_I2C][KEY_PIN_BANK_BYTE_COUNTS][
        pin_bank_id
    ]

    pin_var = cg.new_Pvariable(
        config[CONF_ID], cg.TemplateArguments(pin_bank_byte_count)
    )

    cg.add(pin_var.set_pin_bank(pin_bank_var))
    cg.add(pin_var.set_pin(config[CONF_NUMBER]))
    cg.add(pin_var.set_inverted(config[CONF_INVERTED]))
    cg.add(pin_var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))

    return pin_var


# Build the specified triggers and their automations, then add them to the passed-in TriggerManager.
async def build_automation_triggers(trigger_confs: list, trigger_manager: cg.MockObj):
    if len(trigger_confs) > 0:
        cg.add(trigger_manager.reserve(len(trigger_confs)))
        for conf in trigger_confs:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [], conf)
            cg.add(trigger_manager.add(trigger))


async def to_code(config):
    for device_config in config[CONF_DEVICES]:
        # register_address_bytes = device_config[CONF_REGISTER_ADDRESS_BYTES]

        device_var = cg.new_Pvariable(device_config[CONF_ID])
        await cg.register_component(device_var, device_config)
        # Note that device_var is a CustomI2CDevice which does *not* inherit from I2CDevice, but it exposes the same two
        # methods that register_i2c_device uses to inject the bus and the address, so we can still use it.
        await i2c.register_i2c_device(device_var, device_config)

        cg.add(device_var.set_read_delay(device_config[CONF_READ_DELAY]))

        for register_config in device_config[CONF_REGISTERS]:
            register_address = register_config[CONF_ADDRESS]

            register_var = cg.new_Pvariable(register_config[CONF_ID])
            await cg.register_component(register_var, register_config)

            cg.add(register_var.set_custom_i2c_device(device_var))

            # Register addresses are specified as arrays of bytes - that way we don't have to have separate "address"
            # and "length of address" configuration fields, particularly considering that I want to support devices
            # that have registers that have logically different address sizes within the same device (for example
            # devices running Adafruit's seesaw firmware; to read the values of the device's GPIOs you read from
            # register [1, 4], but to set the PWM duty cycle for pin 12, you write to register [8, 1, 12]).
            cg.add(
                register_var.set_register_address(
                    cg.ArrayInitializer(*register_address)
                )
            )

            if CONF_READ_DELAY in register_config:
                cg.add(
                    register_var.set_read_delay(register_config.get(CONF_READ_DELAY))
                )

    for pin_bank_config in config[CONF_PIN_BANKS]:
        await build_pin_bank(pin_bank_config)

    # keep this last - we want to run any on_setup actions after all the other I2C stuff has been set up (but before
    # any other esphome setup happens)
    if len(on_setup_config := config.get(CONF_ON_SETUP, [])) > 0:
        on_setup_var = cg.new_Pvariable(config[CONF_ON_SETUP_ID])
        await cg.register_component(on_setup_var, on_setup_config)
        # TODO: check to see if this will cause the actions to run in parallel and wrap them so that they run serially if so
        await build_automation_triggers(on_setup_config, on_setup_var.on_setup_triggers)


@automation.register_action("custom_i2c.write", WriteAction, WRITE_ACTION_SCHEMA)
async def write_action_to_code(config, action_id, template_arg, args):
    action = cg.new_Pvariable(action_id, template_arg)

    cg.add(
        action.set_message_builder(
            await build_message_builder(config, template_arg, args)
        )
    )

    if CONF_DEVICE_ID in config:
        cg.add(action.set_device(await cg.get_variable(config[CONF_DEVICE_ID])))
    elif CONF_REGISTER_ID in config:
        cg.add(action.set_register(await cg.get_variable(config[CONF_REGISTER_ID])))
    else:
        raise AssertionError("unreachable")

    return action
