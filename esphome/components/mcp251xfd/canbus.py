from esphome import pins
import esphome.codegen as cg
from esphome.components import canbus, spi
from esphome.components.canbus import CONF_BIT_RATE, CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_DATA_RATE, CONF_ID, CONF_INTERRUPT_PIN, CONF_MODE
from esphome.core import HexInt
from esphome.types import ConfigType

CODEOWNERS = ["@jc2a"]
DEPENDENCIES = ["spi"]

CONF_CLOCK = "clock"
CONF_DATA_BIT_RATE = "data_bit_rate"

mcp251xfd_ns = cg.esphome_ns.namespace("mcp251xfd")
mcp251xfd = mcp251xfd_ns.class_("MCP251XFD", CanbusComponent, spi.SPIDevice)
CanClock = mcp251xfd_ns.enum("CanClock", is_class=True)
CanMode = mcp251xfd_ns.enum("CanMode", is_class=True)

# The 4 MHz oscillator uses the PLL to produce a 40 MHz system clock.
SYSTEM_CLOCK = {"4MHZ": 40_000_000, "20MHZ": 20_000_000, "40MHZ": 40_000_000}
CAN_CLOCK = CanClock.options(*SYSTEM_CLOCK, prefix="CAN_CLOCK_")
# CiDBTCFG and CiTDC reset values (500 kbps / 2 Mbps at 40 MHz), written unchanged in CLASSIC mode
DATA_BIT_TIMING_RESET = 0x000E0303
TDC_RESET = 0x00021000

MCP_MODE = CanMode.options(
    "NORMAL", "CLASSIC", "LOOPBACK", "LISTENONLY", prefix="CAN_MODE_"
)


def _exact_rate(value: str) -> float:
    return {"33KBPS": 100_000 / 3, "83K3BPS": 250_000 / 3}.get(
        value, canbus.get_rate(value)
    )


def bit_timings(
    clock: str, bit_rate: str, data_bit_rate: str | None
) -> tuple[int, int, int]:
    """Choose the phase registers with a common prescaler, plus the CiTDC register.

    Without a data_bit_rate only the nominal phase is searched and the data phase keeps its reset values.
    """
    system_clock = SYSTEM_CLOCK[clock]
    phases = [(_exact_rate(bit_rate), 256, 128, 4)]
    if data_bit_rate is not None:
        phases.append((_exact_rate(data_bit_rate), 32, 16, 3))
        if phases[1][0] < phases[0][0]:
            raise cv.Invalid("data_bit_rate must be greater than or equal to bit_rate")

    best: tuple[tuple[float, float, float, int], tuple[int, int, int]] | None = None
    # Microchip DS20005678B, section 3.4.7 recommends equal NBRP and DBRP.
    for prescaler in range(1, 257):
        timings = []
        errors = []
        sample_errors = []
        total_quanta = 0
        offset = 0
        for rate, max_segment1, max_segment2, min_quanta in phases:
            quanta = round(system_clock / (rate * prescaler))
            if not min_quanta <= quanta <= 1 + max_segment1 + max_segment2:
                break
            segment2 = max(
                1, min(max_segment2, round(quanta * 0.2)), quanta - 1 - max_segment1
            )
            segment1 = quanta - segment2 - 1
            if segment1 < min_quanta - 2:
                break
            error = abs(system_clock / (prescaler * quanta) / rate - 1)
            if error > 0.005:
                break
            errors.append(error)
            sample_errors.append(abs((1 + segment1) / quanta - 0.8))
            total_quanta += quanta
            timings.append(
                ((prescaler - 1) << 24)
                | ((segment1 - 1) << 16)
                | ((segment2 - 1) << 8)
                | (min(segment1, segment2) - 1)
            )
            offset = prescaler * (1 + segment1)
        if len(timings) != len(phases):
            continue
        score = (max(errors), sum(errors), sum(sample_errors), -total_quanta)
        if best is None or score < best[0]:
            if data_bit_rate is None:
                result = (timings[0], DATA_BIT_TIMING_RESET, TDC_RESET)
            else:
                # Automatic TDC is recommended at 1 Mbps and above. TDCO is in SYSCLK ticks.
                tdc = (2 << 16) | (offset << 8) if phases[1][0] >= 1_000_000 else 0
                result = (timings[0], timings[1], tdc)
            best = (score, result)
    if best is None:
        if data_bit_rate is None:
            raise cv.Invalid(
                f"Cannot achieve bit_rate {bit_rate} with a {clock} oscillator within 0.5%"
            )
        raise cv.Invalid(
            f"Cannot achieve bit_rate {bit_rate} and data_bit_rate {data_bit_rate} "
            f"with a common prescaler and a {clock} oscillator within 0.5%"
        )
    return best[1]


def validate_bit_timing(config: ConfigType) -> ConfigType:
    bit_timings(
        config[CONF_CLOCK], config[CONF_BIT_RATE], config.get(CONF_DATA_BIT_RATE)
    )
    return config


def validate_data_rate(config: ConfigType) -> ConfigType:
    """Limit SPI SCK to 0.85 * SYSCLK / 2 per Microchip DS20006027B, section 4.0.

    Rounded down to whole MHz, as hosts round to their own divisors: ESP32 turns the
    8.5 MHz limit of a 20 MHz oscillator into about 8.9 MHz.
    """
    maximum = SYSTEM_CLOCK[config[CONF_CLOCK]] * 85 // 200 // 1_000_000 * 1_000_000
    # 8 MHz is the SPIDevice template default in mcp251xfd.h
    if config.get(CONF_DATA_RATE, 8_000_000) > maximum:
        raise cv.Invalid(
            f"data_rate must not exceed {maximum} Hz with a {config[CONF_CLOCK]} oscillator",
            path=[CONF_DATA_RATE],
        )
    return config


_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(mcp251xfd),
        cv.Optional(CONF_CLOCK, default="40MHZ"): cv.enum(CAN_CLOCK, upper=True),
        cv.Optional(CONF_BIT_RATE, default="500KBPS"): cv.enum(
            canbus.CAN_SPEEDS, upper=True
        ),
        cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
    }
).extend(spi.spi_device_schema(True))

_FD_SCHEMA = canbus.CANBUS_FD_SCHEMA.extend(
    _DEVICE_SCHEMA,
    {
        cv.Optional(CONF_DATA_BIT_RATE, default="2000KBPS"): cv.enum(
            canbus.CAN_DATA_SPEEDS, upper=True
        ),
    },
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            # NORMAL sends CAN FD frames by default; the test modes default to classic frames.
            "NORMAL": _FD_SCHEMA.extend(
                {cv.Optional(canbus.CONF_DEFAULT_FD, default=True): cv.boolean}
            ),
            "CLASSIC": canbus.CANBUS_SCHEMA.extend(_DEVICE_SCHEMA),
            "LOOPBACK": _FD_SCHEMA,
            "LISTENONLY": _FD_SCHEMA,
        },
        key=CONF_MODE,
        default_type="NORMAL",
        enum=MCP_MODE,
        upper=True,
    ),
    validate_data_rate,
    validate_bit_timing,
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "mcp251xfd", require_miso=True, require_mosi=True
)


async def to_code(config: ConfigType) -> None:
    nominal, data, tdc = bit_timings(
        config[CONF_CLOCK], config[CONF_BIT_RATE], config.get(CONF_DATA_BIT_RATE)
    )
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_CLOCK],
        config[CONF_MODE],
        HexInt(nominal),
        HexInt(data),
        HexInt(tdc),
    )
    await canbus.register_canbus(var, config, reports_status=True)
    await spi.register_spi_device(var, config)
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
