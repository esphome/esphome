from collections.abc import Callable

from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import CONF_CHARGE_CURRENT, CONF_ENABLED
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID, CONF_MODE, CONF_VOLTAGE
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DEPENDENCIES = ["i2c"]
DOMAIN = "npm1300"
MULTI_CONF = True

CONF_NPM1300_ID = "npm1300_id"
CONF_VBUS_CURRENT_LIMIT = "vbus_current_limit"
CONF_THERMISTOR_RESISTANCE = "thermistor_resistance"
CONF_THERMISTOR_BETA = "thermistor_beta"
CONF_CHARGER = "charger"
CONF_TERMINATION_VOLTAGE = "termination_voltage"
CONF_WARM_TERMINATION_VOLTAGE = "warm_termination_voltage"
CONF_TERMINATION_CURRENT = "termination_current"
CONF_TRICKLE_VOLTAGE = "trickle_voltage"
CONF_DISCHARGE_CURRENT_LIMIT = "discharge_current_limit"
CONF_RECHARGE = "recharge"
CONF_BUCK1 = "buck1"
CONF_BUCK2 = "buck2"
CONF_LDO1 = "ldo1"
CONF_LDO2 = "ldo2"

npm1300_ns = cg.esphome_ns.namespace("npm1300")
NPM1300Component = npm1300_ns.class_(
    "NPM1300Component", cg.PollingComponent, i2c.I2CDevice
)
NPM1300Output = npm1300_ns.enum("NPM1300Output", is_class=True)
NPM1300OutputConfig = npm1300_ns.struct("NPM1300OutputConfig")
NPM1300ChargerConfig = npm1300_ns.struct("NPM1300ChargerConfig")

NPM1300_CHILD_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_NPM1300_ID): cv.use_id(NPM1300Component)}
)

# Config key of each regulator and its NPM1300Output enumerator
OUTPUTS = {
    CONF_BUCK1: NPM1300Output.NPM1300_OUTPUT_BUCK1,
    CONF_BUCK2: NPM1300Output.NPM1300_OUTPUT_BUCK2,
    CONF_LDO1: NPM1300Output.NPM1300_OUTPUT_LDO1,
    CONF_LDO2: NPM1300Output.NPM1300_OUTPUT_LDO2,
}

# ADCNTCRSEL codes
THERMISTOR_RESISTANCES = {10000: 1, 47000: 2, 100000: 3}
LDO_MODES = {"load_switch": 0, "ldo": 1}
TERMINATION_CURRENTS = {"10%": 0, "20%": 1}
# BCHGVTRICKLESEL, by millivolts
TRICKLE_VOLTAGES = {2900: 0, 2500: 1}
# BCHGISETDISCHARGE codes, by milliamperes
DISCHARGE_CURRENT_LIMITS = {200: 84, 1000: 415}
# BCHGVTERM: 3.50 V to 3.65 V, then 4.00 V to 4.45 V, both in 50 mV steps
TERMINATION_VOLTAGES = [3500 + 50 * i for i in range(4)] + [
    4000 + 50 * i for i in range(10)
]
# The wake-up timer counts 16 ms ticks in a 24-bit register
HIBERNATE_TICK_MS = 16
HIBERNATE_MAX_TICKS = 0xFFFFFF


def _millis(value: float) -> int:
    return round(value * 1000)


def _one_of_units(
    validator: Callable[[object], float], allowed: list[int], unit: str
) -> Callable[[object], float]:
    """Validate a quantity with `validator` and accept only the listed values, given in milli-units."""

    def validate(value: object) -> float:
        value = validator(value)
        if _millis(value) not in allowed:
            options = ", ".join(f"{v / 1000:g}{unit}" for v in allowed)
            raise cv.Invalid(f"Must be one of {options}")
        return value

    return validate


def _stepped(
    validator: Callable[[object], float],
    minimum: int,
    maximum: int,
    step: int,
    unit: str,
) -> Callable[[object], float]:
    """Validate a quantity in a range of milli-units, in steps of `step`."""
    allowed = list(range(minimum, maximum + 1, step))
    in_range = cv.All(validator, cv.Range(min=minimum / 1000, max=maximum / 1000))

    def validate(value: object) -> float:
        value = in_range(value)
        if _millis(value) not in allowed:
            raise cv.Invalid(f"Must be a multiple of {step}m{unit}")
        return value

    return validate


def _thermistor_resistance(value: object) -> float:
    value = cv.resistance(value)
    if round(value) not in THERMISTOR_RESISTANCES:
        raise cv.Invalid("Must be one of 10kOhm, 47kOhm, 100kOhm")
    return value


OUTPUT_VOLTAGE = _stepped(cv.voltage, 1000, 3300, 100, "V")

BUCK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): OUTPUT_VOLTAGE,
        cv.Optional(CONF_ENABLED): cv.boolean,
    }
)
LDO_SCHEMA = BUCK_SCHEMA.extend(
    {
        cv.Optional(CONF_MODE): cv.enum(LDO_MODES, lower=True),
    }
)

CHARGER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CHARGE_CURRENT): _stepped(cv.current, 32, 800, 2, "A"),
        cv.Optional(CONF_TERMINATION_VOLTAGE, default="4.2V"): _one_of_units(
            cv.voltage, TERMINATION_VOLTAGES, "V"
        ),
        cv.Optional(CONF_WARM_TERMINATION_VOLTAGE): _one_of_units(
            cv.voltage, TERMINATION_VOLTAGES, "V"
        ),
        cv.Optional(CONF_TERMINATION_CURRENT, default="10%"): cv.enum(
            TERMINATION_CURRENTS
        ),
        cv.Optional(CONF_TRICKLE_VOLTAGE, default="2.9V"): _one_of_units(
            cv.voltage, list(TRICKLE_VOLTAGES), "V"
        ),
        cv.Optional(CONF_DISCHARGE_CURRENT_LIMIT, default="1000mA"): _one_of_units(
            cv.current, list(DISCHARGE_CURRENT_LIMITS), "A"
        ),
        cv.Optional(CONF_RECHARGE, default=True): cv.boolean,
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
    }
)


def _vbus_current_limit(value: object) -> float:
    value = cv.current(value)
    milliamps = _millis(value)
    if milliamps != 100 and (milliamps not in range(500, 1501, 100)):
        raise cv.Invalid("Must be 100mA, or 500mA to 1500mA in 100mA steps")
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NPM1300Component),
            cv.Optional(CONF_VBUS_CURRENT_LIMIT): _vbus_current_limit,
            cv.Optional(CONF_THERMISTOR_RESISTANCE): _thermistor_resistance,
            cv.Optional(CONF_THERMISTOR_BETA, default=3380): cv.int_range(
                min=1, max=65535
            ),
            cv.Optional(CONF_CHARGER): CHARGER_SCHEMA,
            cv.Optional(CONF_BUCK1): BUCK_SCHEMA,
            cv.Optional(CONF_BUCK2): BUCK_SCHEMA,
            cv.Optional(CONF_LDO1): LDO_SCHEMA,
            cv.Optional(CONF_LDO2): LDO_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)


def _termination_voltage_code(volts: float) -> int:
    return TERMINATION_VOLTAGES.index(_millis(volts))


def _charger_config(config: ConfigType) -> cg.StructInitializer:
    termination = _termination_voltage_code(config[CONF_TERMINATION_VOLTAGE])
    warm = config.get(CONF_WARM_TERMINATION_VOLTAGE)
    return cg.StructInitializer(
        NPM1300ChargerConfig,
        ("current", _millis(config[CONF_CHARGE_CURRENT]) // 2),
        (
            "discharge_limit",
            DISCHARGE_CURRENT_LIMITS[_millis(config[CONF_DISCHARGE_CURRENT_LIMIT])],
        ),
        ("termination_voltage", termination),
        (
            "warm_termination_voltage",
            termination if warm is None else _termination_voltage_code(warm),
        ),
        ("trickle_voltage", TRICKLE_VOLTAGES[_millis(config[CONF_TRICKLE_VOLTAGE])]),
        ("termination_current", TERMINATION_CURRENTS[config[CONF_TERMINATION_CURRENT]]),
        ("recharge", config[CONF_RECHARGE]),
        ("enabled", config[CONF_ENABLED]),
    )


def _output_config(config: ConfigType) -> cg.StructInitializer:
    voltage = config.get(CONF_VOLTAGE)
    mode = config.get(CONF_MODE)
    enabled = config.get(CONF_ENABLED)
    return cg.StructInitializer(
        NPM1300OutputConfig,
        ("voltage", None if voltage is None else (_millis(voltage) - 1000) // 100),
        ("ldo_mode", None if mode is None else LDO_MODES[mode]),
        ("enabled", None if enabled is None else int(enabled)),
    )


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    if (limit := config.get(CONF_VBUS_CURRENT_LIMIT)) is not None:
        cg.add(var.set_vbus_current_limit(_millis(limit) // 100))
    if (resistance := config.get(CONF_THERMISTOR_RESISTANCE)) is not None:
        cg.add(
            var.set_thermistor(
                THERMISTOR_RESISTANCES[round(resistance)], config[CONF_THERMISTOR_BETA]
            )
        )
    if (charger := config.get(CONF_CHARGER)) is not None:
        cg.add(var.set_charger_config(_charger_config(charger)))
    for key, output in OUTPUTS.items():
        if (output_config := config.get(key)) is not None:
            cg.add(var.set_output_config(output, _output_config(output_config)))


automation.register_apply_action(
    "npm1300.enter_ship_mode",
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(NPM1300Component)}),
    automation.ApplyCall("enter_ship_mode()"),
)

automation.register_apply_action(
    "npm1300.hibernate",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(NPM1300Component),
            cv.Required(CONF_DURATION): cv.templatable(
                cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(
                        min=cv.TimePeriod(milliseconds=HIBERNATE_TICK_MS),
                        max=cv.TimePeriod(
                            milliseconds=HIBERNATE_TICK_MS * HIBERNATE_MAX_TICKS
                        ),
                    ),
                )
            ),
        }
    ),
    automation.ApplyField(CONF_DURATION, "hibernate", cg.uint32),
)
