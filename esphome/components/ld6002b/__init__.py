from collections.abc import Awaitable, Callable

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WAKEUP_PIN
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, Expression

from .const import CONF_AUTO_WAKE, CONF_WAKEUP_PULSE

CODEOWNERS = ["@hepter"]
DEPENDENCIES = ["uart"]
DOMAIN = "ld6002b"
MULTI_CONF = True

ld6002b_ns = cg.esphome_ns.namespace("ld6002b")
LD6002BComponent = ld6002b_ns.class_("LD6002BComponent", cg.Component, uart.UARTDevice)


def _validate_wakeup_options(config: ConfigType) -> ConfigType:
    """Reject wake options that would silently do nothing.

    Runs before the schema so the defaults for the keys below have not been
    filled in yet and an explicit user value is still distinguishable from one.
    """
    if not isinstance(config, dict):
        return config
    if CONF_WAKEUP_PIN in config:
        return config
    for key in (CONF_AUTO_WAKE, CONF_WAKEUP_PULSE):
        if key in config:
            raise cv.Invalid(
                f"'{key}' requires '{CONF_WAKEUP_PIN}' to be configured", path=[key]
            )
    return config


CONFIG_SCHEMA = cv.All(
    _validate_wakeup_options,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD6002BComponent),
            cv.Optional(CONF_WAKEUP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_WAKEUP_PULSE, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_WAKE, default=True): cv.boolean,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld6002b",
    baud_rate=115200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if wakeup_pin_config := config.get(CONF_WAKEUP_PIN):
        pin = await cg.gpio_pin_expression(wakeup_pin_config)
        cg.add(var.set_wakeup_pin(pin))

    cg.add(var.set_wakeup_pulse_ms(config[CONF_WAKEUP_PULSE].total_milliseconds))

    cg.add(var.set_auto_wake(config[CONF_AUTO_WAKE]))


# (type, entity config, creates the entity from its template arguments, hub setter or None)
DispatchEntity = tuple[
    MockObj,
    ConfigType,
    Callable[[cg.TemplateArguments], Awaitable[MockObj]],
    Callable[[MockObj], Expression] | None,
]


async def build_type_dispatch(
    hub: MockObj,
    params: str,
    call: str,
    entities: list[DispatchEntity],
    publish_before: str | None = None,
    publish_after: str | None = None,
) -> None:
    """Give all of a platform's entities one generated function that finds the type by pointer.

    One class per domain instead of one per entity; ``call`` names the type as ``type``.
    """
    if not entities:
        return
    fn = f"esphome__{entities[0][1][CONF_ID].id}__fn"
    # Declared first: the entities' types name it and its body names the entities.
    cg.add_global(cg.RawStatement(f"static void {fn}({params});"))
    template = cg.TemplateArguments(cg.RawExpression(fn))
    created: list[tuple[MockObj, MockObj]] = []
    for type_, _, create, setter in entities:
        var = await create(template)
        if setter is not None:
            cg.add(setter(var))
        created.append((type_, var))
    *head, (last_type, _) = created
    chain = "".join(f"entity == ::{var} ? {type_} : " for type_, var in head)
    body = [f"const auto type = {chain}{last_type};", f"::{hub}->{call};"]
    if publish_before:
        body.insert(0, publish_before)
    if publish_after:
        body.append(publish_after)
    lines = "\n  ".join(body)
    cg.add_global(cg.RawStatement(f"static void {fn}({params}) {{\n  {lines}\n}}"))
