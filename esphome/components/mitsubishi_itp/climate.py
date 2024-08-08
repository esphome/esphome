import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    climate,
    uart,
    time,
    sensor,
    button,
    select,
)
from esphome.const import (
    CONF_CUSTOM_FAN_MODES,
    CONF_ID,
    CONF_SUPPORTED_FAN_MODES,
    CONF_SUPPORTED_MODES,
    CONF_TIME_ID,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_NONE,
)
from esphome.core import coroutine

CODEOWNERS = ["@Sammy1Am", "@KazWolfe"]

AUTO_LOAD = [
    "climate",
    "select",
    "sensor",
    "binary_sensor",
    "button",
    "text_sensor",
    "time",
]
DEPENDENCIES = [
    "uart",
]

CONF_UART_HEATPUMP = "uart_heatpump"
CONF_UART_THERMOSTAT = "uart_thermostat"

CONF_SELECTS = "selects"
CONF_TEMPERATURE_SOURCE_SELECT = "temperature_source_select"  # This is to create a Select object for selecting a source
CONF_VANE_POSITION_SELECT = "vane_position_select"
CONF_HORIZONTAL_VANE_POSITION_SELECT = "horizontal_vane_position_select"

CONF_BUTTONS = "buttons"
CONF_FILTER_RESET_BUTTON = "filter_reset_button"

CONF_TEMPERATURE_SOURCES = (
    "temperature_sources"  # This is for specifying additional sources
)

CONF_DISABLE_ACTIVE_MODE = "disable_active_mode"
CONF_ENHANCED_MHK_SUPPORT = (
    "enhanced_mhk"  # EXPERIMENTAL. Will be set to default eventually.
)

DEFAULT_POLLING_INTERVAL = "5s"

mitsubishi_itp_ns = cg.esphome_ns.namespace("mitsubishi_itp")
MitsubishiUART = mitsubishi_itp_ns.class_(
    "MitsubishiUART", cg.PollingComponent, climate.Climate
)
CONF_MITSUBISHI_IPT_ID = "mitsuibishi_itp_id"

TemperatureSourceSelect = mitsubishi_itp_ns.class_(
    "TemperatureSourceSelect", select.Select
)
VanePositionSelect = mitsubishi_itp_ns.class_("VanePositionSelect", select.Select)
HorizontalVanePositionSelect = mitsubishi_itp_ns.class_(
    "HorizontalVanePositionSelect", select.Select
)

FilterResetButton = mitsubishi_itp_ns.class_(
    "FilterResetButton", button.Button, cg.Component
)

DEFAULT_CLIMATE_MODES = ["OFF", "HEAT", "DRY", "COOL", "FAN_ONLY", "HEAT_COOL"]
DEFAULT_FAN_MODES = ["AUTO", "QUIET", "LOW", "MEDIUM", "HIGH"]
CUSTOM_FAN_MODES = {"VERYHIGH": mitsubishi_itp_ns.FAN_MODE_VERYHIGH}
VANE_POSITIONS = ["Auto", "1", "2", "3", "4", "5", "Swing"]
HORIZONTAL_VANE_POSITIONS = ["Auto", "<<", "<", "|", ">", ">>", "<>", "Swing"]

INTERNAL_TEMPERATURE_SOURCE_OPTIONS = [
    mitsubishi_itp_ns.TEMPERATURE_SOURCE_INTERNAL
]  # These will always be available

validate_custom_fan_modes = cv.enum(CUSTOM_FAN_MODES, upper=True)

BASE_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(MitsubishiUART),
        cv.Required(CONF_UART_HEATPUMP): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_UART_THERMOSTAT): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(
            CONF_SUPPORTED_MODES, default=DEFAULT_CLIMATE_MODES
        ): cv.ensure_list(climate.validate_climate_mode),
        cv.Optional(
            CONF_SUPPORTED_FAN_MODES, default=DEFAULT_FAN_MODES
        ): cv.ensure_list(climate.validate_climate_fan_mode),
        cv.Optional(CONF_CUSTOM_FAN_MODES, default=["VERYHIGH"]): cv.ensure_list(
            validate_custom_fan_modes
        ),
        cv.Optional(CONF_TEMPERATURE_SOURCES, default=[]): cv.ensure_list(
            cv.use_id(sensor.Sensor)
        ),
        cv.Optional(CONF_DISABLE_ACTIVE_MODE, default=False): cv.boolean,
        cv.Optional(CONF_ENHANCED_MHK_SUPPORT, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema(DEFAULT_POLLING_INTERVAL))

SELECTS = {
    CONF_TEMPERATURE_SOURCE_SELECT: (
        "Temperature Source",
        select.select_schema(
            TemperatureSourceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:thermometer-check",
        ),
        INTERNAL_TEMPERATURE_SOURCE_OPTIONS,
    ),
    CONF_VANE_POSITION_SELECT: (
        "Vane Position",
        select.select_schema(
            VanePositionSelect,
            entity_category=ENTITY_CATEGORY_NONE,
            icon="mdi:arrow-expand-vertical",
        ),
        VANE_POSITIONS,
    ),
    CONF_HORIZONTAL_VANE_POSITION_SELECT: (
        "Horizontal Vane Position",
        select.select_schema(
            HorizontalVanePositionSelect,
            entity_category=ENTITY_CATEGORY_NONE,
            icon="mdi:arrow-expand-horizontal",
        ),
        HORIZONTAL_VANE_POSITIONS,
    ),
}

SELECTS_SCHEMA = cv.All(
    {
        cv.Optional(
            select_designator, default={"name": f"{select_name}"}
        ): select_schema
        for select_designator, (
            select_name,
            select_schema,
            _,
        ) in SELECTS.items()
    }
)

BUTTONS = {
    CONF_FILTER_RESET_BUTTON: (
        "Filter Reset",
        button.button_schema(
            FilterResetButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:restore",
        ),
    )
}

BUTTONS_SCHEMA = cv.All(
    {
        cv.Optional(
            button_designator, default={"name": f"{button_name}"}
        ): button_schema
        for button_designator, (button_name, button_schema) in BUTTONS.items()
    }
)


CONFIG_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_SELECTS, default={}): SELECTS_SCHEMA,
        cv.Optional(CONF_BUTTONS, default={}): BUTTONS_SCHEMA,
    }
)


def final_validate(config):
    schema = uart.final_validate_device_schema(
        "mitsubishi_itp",
        uart_bus=CONF_UART_HEATPUMP,
        require_tx=True,
        require_rx=True,
        data_bits=8,
        parity="EVEN",
        stop_bits=1,
    )
    if CONF_UART_THERMOSTAT in config:
        schema = schema.extend(
            uart.final_validate_device_schema(
                "mitsubishi_itp",
                uart_bus=CONF_UART_THERMOSTAT,
                require_tx=True,
                require_rx=True,
                data_bits=8,
                parity="EVEN",
                stop_bits=1,
            )
        )
    schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


@coroutine
async def to_code(config):
    hp_uart_component = await cg.get_variable(config[CONF_UART_HEATPUMP])
    muart_component = cg.new_Pvariable(config[CONF_ID], hp_uart_component)

    await cg.register_component(muart_component, config)
    await climate.register_climate(muart_component, config)

    # If thermostat defined
    if CONF_UART_THERMOSTAT in config:
        # Register thermostat with MUART
        ts_uart_component = await cg.get_variable(config[CONF_UART_THERMOSTAT])
        cg.add(getattr(muart_component, "set_thermostat_uart")(ts_uart_component))
        # Add sensor as source
        SELECTS[CONF_TEMPERATURE_SOURCE_SELECT][2].append("Thermostat")

    # If RTC defined
    if CONF_TIME_ID in config:
        rtc_component = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(getattr(muart_component, "set_time_source")(rtc_component))
    elif CONF_UART_THERMOSTAT in config and config.get(CONF_ENHANCED_MHK_SUPPORT):
        raise cv.RequiredFieldInvalid(
            f"{CONF_TIME_ID} is required if {CONF_ENHANCED_MHK_SUPPORT} is set."
        )

    # Traits
    traits = muart_component.config_traits()

    if CONF_SUPPORTED_MODES in config:
        cg.add(traits.set_supported_modes(config[CONF_SUPPORTED_MODES]))

    if CONF_SUPPORTED_FAN_MODES in config:
        cg.add(traits.set_supported_fan_modes(config[CONF_SUPPORTED_FAN_MODES]))

    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(traits.set_supported_custom_fan_modes(config[CONF_CUSTOM_FAN_MODES]))

    # Selects

    # Add additional configured temperature sensors to the select menu
    for ts_id in config[CONF_TEMPERATURE_SOURCES]:
        ts = await cg.get_variable(ts_id)
        SELECTS[CONF_TEMPERATURE_SOURCE_SELECT][2].append(ts.get_name())
        cg.add(
            getattr(ts, "add_on_state_callback")(
                # TODO: Is there anyway to do this without a raw expression?
                cg.RawExpression(
                    f"[](float v){{{getattr(muart_component, 'temperature_source_report')}({ts.get_name()}, v);}}"
                )
            )
        )

    # Register selects
    for select_designator, (
        _,
        _,
        select_options,
    ) in SELECTS.items():
        select_conf = config[CONF_SELECTS][select_designator]
        select_component = cg.new_Pvariable(select_conf[CONF_ID])
        cg.add(getattr(muart_component, f"set_{select_designator}")(select_component))
        await cg.register_parented(select_component, muart_component)

        # For temperature source select, skip registration if there are less than two sources
        if select_designator == CONF_TEMPERATURE_SOURCE_SELECT:
            if len(SELECTS[CONF_TEMPERATURE_SOURCE_SELECT][2]) < 2:
                continue

        await select.register_select(
            select_component, select_conf, options=select_options
        )

    # Buttons
    for button_designator, (_, _) in BUTTONS.items():
        button_conf = config[CONF_BUTTONS][button_designator]
        button_component = await button.new_button(button_conf)
        await cg.register_component(button_component, button_conf)
        await cg.register_parented(button_component, muart_component)

    # Debug Settings
    if dam_conf := config.get(CONF_DISABLE_ACTIVE_MODE):
        cg.add(getattr(muart_component, "set_active_mode")(not dam_conf))

    if enhanced_mhk_protocol := config.get(CONF_ENHANCED_MHK_SUPPORT):
        cg.add(
            getattr(muart_component, "set_enhanced_mhk_support")(enhanced_mhk_protocol)
        )
