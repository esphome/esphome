import logging

from voluptuous import Schemable

from esphome import automation, core, pins
import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
from esphome.components.const import CONF_ON_RECEIVE
from esphome.components.opentherm import CONF_IN_PIN, CONF_OUT_PIN, schema
from esphome.components.opentherm.generate import TYPE_MAP, MessageId, OpenthermData
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_ID,
    CONF_LAMBDA,
    CONF_SENSOR,
    CONF_TRIGGER_ID,
    CONF_VALUE,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.cpp_generator import StringLiteral
from esphome.types import ConfigType

CODEOWNERS = ["@bootc"]
MULTI_CONF = True

AUTO_LOAD = ["binary_sensor", "opentherm", "sensor"]

CONF_BEFORE_TRANSMIT = "before_transmit"
CONF_OPENTHERM_BOILER_ID = "opentherm_boiler_id"

opentherm_boiler_ns = cg.esphome_ns.namespace("opentherm_boiler")

Boiler = opentherm_boiler_ns.class_("Boiler", cg.Component)
RequestProcessor = opentherm_boiler_ns.class_("RequestProcessor")
BinarySensorValue = opentherm_boiler_ns.class_("BinarySensorValue", RequestProcessor)
ConstantValue = opentherm_boiler_ns.class_("ConstantValue", RequestProcessor)
LambdaValue = opentherm_boiler_ns.class_("LambdaValue", RequestProcessor)
SensorValue = opentherm_boiler_ns.class_("SensorValue", RequestProcessor)

# Triggers
OnReceiveTrigger = opentherm_boiler_ns.class_(
    "OnReceiveTrigger",
    automation.Trigger.template(OpenthermData.operator("ref")),
)
BeforeTransmitTrigger = opentherm_boiler_ns.class_(
    "BeforeTransmitTrigger",
    automation.Trigger.template(OpenthermData.operator("ref")),
)

_LOGGER = logging.getLogger(__name__)


def get_value_schema(entity: schema.EntitySchema) -> Schemable:
    if (
        entity.message_data.startswith("u8_")
        or entity.message_data.startswith("s8_")
        or entity.message_data in {"u16", "s16"}
    ):
        return cv.int_

    if entity.message_data == "f88":
        return cv.float_

    raise cv.Invalid(f"Unsupported data type {entity.message_data} for entity")


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Boiler),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnReceiveTrigger),
                }
            ),
            cv.Optional(CONF_BEFORE_TRANSMIT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BeforeTransmitTrigger
                    ),
                }
            ),
        }
    )
    .extend(
        cv.Schema(
            {
                cv.Optional(key): cv.All(
                    cv.Schema(
                        {
                            cv.GenerateID(CONF_ID): cv.declare_id(RequestProcessor),
                            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
                            cv.Optional(CONF_VALUE): get_value_schema(schema),
                            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
                        }
                    ),
                    cv.has_exactly_one_key(CONF_SENSOR, CONF_VALUE, CONF_LAMBDA),
                )
                for key, schema in schema.SENSORS.items()
            }
        )
    )
    .extend(
        cv.Schema(
            {
                cv.Optional(key): cv.All(
                    cv.Schema(
                        {
                            cv.GenerateID(CONF_ID): cv.declare_id(RequestProcessor),
                            cv.Optional(CONF_BINARY_SENSOR): cv.use_id(
                                binary_sensor.BinarySensor
                            ),
                            cv.Optional(CONF_VALUE): cv.boolean,
                            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
                        }
                    ),
                    cv.has_exactly_one_key(CONF_BINARY_SENSOR, CONF_VALUE, CONF_LAMBDA),
                )
                for key, schema in schema.BINARY_SENSORS.items()
            }
        )
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266]),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set pins
    in_pin = await cg.gpio_pin_expression(config[CONF_IN_PIN])
    cg.add(var.set_in_pin(in_pin))

    out_pin = await cg.gpio_pin_expression(config[CONF_OUT_PIN])
    cg.add(var.set_out_pin(out_pin))

    for conf in config.get(CONF_ON_RECEIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(OpenthermData.operator("ref"), "data")], conf
        )

    for conf in config.get(CONF_BEFORE_TRANSMIT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(OpenthermData.operator("ref"), "data")], conf
        )

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue

        if key in schema.SENSORS or key in schema.BINARY_SENSORS:
            schema_ = schema.SENSORS.get(key) or schema.BINARY_SENSORS.get(key)
            if schema_ is None:
                # this should never happen due to earlier checks
                raise cv.Invalid(f"Unknown entity key {key}")

            accessor = TYPE_MAP[schema_.message_data]
            message_id = getattr(MessageId, schema_.message)
            id: core.ID = conf.get(CONF_ID)
            value = None

            if CONF_SENSOR in conf:
                id.type = SensorValue.template(accessor)
                sensor_ = await cg.get_variable(conf[CONF_SENSOR])
                value = cg.new_Pvariable(id, sensor_)

            elif CONF_BINARY_SENSOR in conf:
                id.type = BinarySensorValue.template(accessor)
                binary_sensor_ = await cg.get_variable(conf[CONF_BINARY_SENSOR])
                value = cg.new_Pvariable(id, binary_sensor_)

            elif CONF_VALUE in conf:
                id.type = ConstantValue.template(accessor)
                value = cg.new_Pvariable(id, conf[CONF_VALUE])

            elif CONF_LAMBDA in conf:
                id.type = LambdaValue.template(accessor)
                lambda_ = await cg.process_lambda(
                    conf[CONF_LAMBDA],
                    [(OpenthermData.operator("const").operator("ref"), "data")],
                    return_type=cg.optional.template(accessor.class_("ValueType")),
                )
                value = cg.new_Pvariable(id, lambda_)

            else:
                # Should never happen due to cv.has_exactly_one_key()
                raise cv.Invalid(f"No valid rp configuration for key {key}")

            cg.add(value.set_id(StringLiteral(f"{key} ({id})")))
            cg.add(var.register_request_processor(message_id, value))
