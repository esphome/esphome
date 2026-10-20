import esphome.codegen as cg
from esphome.components import canbus, spi
from esphome.components.canbus import CONF_BIT_RATE, CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_DATA_RATE, CONF_ID, CONF_MODE
from esphome.core import HexInt
from esphome.types import ConfigType

CODEOWNERS = ["@jc2a"]
DEPENDENCIES = ["spi"]

CONF_CLOCK = "clock"
CONF_DATA_BIT_RATE = "data_bit_rate"
CONF_SPI_MODE = spi.CONF_SPI_MODE

mcp251xfd_ns = cg.esphome_ns.namespace("mcp251xfd")
mcp251xfd = mcp251xfd_ns.class_("MCP251XFD", CanbusComponent, spi.SPIDevice)
CanClock = mcp251xfd_ns.enum("CanClock", is_class=True)
CanMode = mcp251xfd_ns.enum("CanMode", is_class=True)

CAN_CLOCK = {
    "4MHZ": CanClock.CAN_CLOCK_4MHZ,
    "20MHZ": CanClock.CAN_CLOCK_20MHZ,
    "40MHZ": CanClock.CAN_CLOCK_40MHZ,
}

MCP_MODE = {
    "NORMAL": CanMode.CAN_MODE_NORMAL,
    "CLASSIC": CanMode.CAN_MODE_CLASSIC,
    "LOOPBACK": CanMode.CAN_MODE_LOOPBACK,
    "LISTENONLY": CanMode.CAN_MODE_LISTEN_ONLY,
}


def nominal_bit_timing(clock: str, bit_rate: str) -> int:
    """Choose nominal segments near an 80% sample point, with at most 0.5% rate error."""
    system_clock = 20_000_000 if clock == "20MHZ" else 40_000_000
    rate = {"33KBPS": 100_000 / 3, "83K3BPS": 250_000 / 3}.get(
        bit_rate, canbus.get_rate(bit_rate)
    )
    best: tuple[float, float, int] | None = None
    timing = 0
    for prescaler in range(1, 257):
        quanta = round(system_clock / (rate * prescaler))
        if not 4 <= quanta <= 385:
            continue
        segment2 = max(1, min(128, round(quanta * 0.2)))
        segment1 = quanta - segment2 - 1
        if not 2 <= segment1 <= 256:
            continue
        error = abs(system_clock / (prescaler * quanta) / rate - 1)
        score = (error, abs((1 + segment1) / quanta - 0.8), -quanta)
        if best is None or score < best:
            best = score
            timing = (
                ((prescaler - 1) << 24)
                | ((segment1 - 1) << 16)
                | ((segment2 - 1) << 8)
                | (segment2 - 1)
            )
    if best is None or best[0] > 0.005:
        raise cv.Invalid(
            f"Cannot achieve {bit_rate} with a {clock} oscillator within 0.5%"
        )
    return timing


def bit_timings(clock: str, bit_rate: str, data_bit_rate: str) -> tuple[int, int, int]:
    """Choose both phase registers with a common prescaler, plus the CiTDC register."""
    system_clock = 20_000_000 if clock == "20MHZ" else 40_000_000
    rates = [
        {"33KBPS": 100_000 / 3, "83K3BPS": 250_000 / 3}.get(
            value, canbus.get_rate(value)
        )
        for value in (bit_rate, data_bit_rate)
    ]
    if rates[1] < rates[0]:
        raise cv.Invalid("data_bit_rate must be greater than or equal to bit_rate")

    best: tuple[float, float, float, int] | None = None
    result = (0, 0, 0)
    # Microchip DS20005678B, section 3.4.7 recommends equal NBRP and DBRP.
    for prescaler in range(1, 257):
        timings = []
        errors = []
        sample_errors = []
        total_quanta = 0
        offset = 0
        for rate, max_segment1, max_segment2, min_quanta in (
            (rates[0], 256, 128, 4),
            (rates[1], 32, 16, 3),
        ):
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
        if len(timings) != 2:
            continue
        score = (max(errors), sum(errors), sum(sample_errors), -total_quanta)
        if best is None or score < best:
            best = score
            # Automatic TDC is recommended at 1 Mbps and above. TDCO is in SYSCLK ticks.
            tdc = (2 << 16) | (offset << 8) if rates[1] >= 1_000_000 else 0
            result = (timings[0], timings[1], tdc)
    if best is None:
        raise cv.Invalid(
            f"Cannot achieve bit_rate {bit_rate} and data_bit_rate {data_bit_rate} "
            f"with a common prescaler and a {clock} oscillator within 0.5%"
        )
    return result


def validate_bit_timing(config: ConfigType) -> ConfigType:
    bit_timings(config[CONF_CLOCK], config[CONF_BIT_RATE], config[CONF_DATA_BIT_RATE])
    return config


def validate_mode_defaults(config: ConfigType) -> ConfigType:
    """Classic-only mode cannot enable CAN FD send defaults."""
    if config[CONF_MODE] == "CLASSIC" and (
        config[canbus.CONF_DEFAULT_FD] or config[canbus.CONF_DEFAULT_BIT_RATE_SWITCH]
    ):
        raise cv.Invalid(
            "default_fd and default_bit_rate_switch must be false in CLASSIC mode"
        )
    return config


CONFIG_SCHEMA = cv.All(
    canbus.CANBUS_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(mcp251xfd),
            cv.Optional(CONF_CLOCK, default="40MHZ"): cv.enum(CAN_CLOCK, upper=True),
            cv.Optional(CONF_BIT_RATE, default="500KBPS"): cv.enum(
                canbus.CAN_SPEEDS, upper=True
            ),
            cv.Optional(CONF_DATA_BIT_RATE, default="2000KBPS"): cv.enum(
                canbus.CAN_DATA_SPEEDS, upper=True
            ),
            cv.Optional(CONF_MODE, default="NORMAL"): cv.enum(MCP_MODE, upper=True),
            cv.Optional(canbus.CONF_DEFAULT_FD, default=False): cv.boolean,
            cv.Optional(canbus.CONF_DEFAULT_BIT_RATE_SWITCH, default=False): cv.boolean,
        }
    )
    .extend(spi.spi_device_schema(True, "8MHz", "MODE0"))
    .extend(
        {
            # SPI must stay below 0.85 * SYSCLK / 2. The slowest running SYSCLK is 20 MHz.
            cv.Optional(CONF_DATA_RATE, default="8MHz"): cv.All(
                spi.SPI_DATA_RATE_SCHEMA, cv.Range(max=8_000_000)
            ),
            cv.Optional(CONF_SPI_MODE, default="MODE0"): cv.enum(
                {key: spi.SPI_MODE_OPTIONS[key] for key in ("MODE0", "MODE3")},
                upper=True,
            ),
        }
    ),
    validate_mode_defaults,
    validate_bit_timing,
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "mcp251xfd", require_miso=True, require_mosi=True
)


async def to_code(config: ConfigType) -> None:
    nominal, data, tdc = bit_timings(
        config[CONF_CLOCK], config[CONF_BIT_RATE], config[CONF_DATA_BIT_RATE]
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
