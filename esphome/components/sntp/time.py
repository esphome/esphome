import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import time as time_
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PLATFORM,
    CONF_SERVERS,
    CONF_SERVICE,
    CONF_TIME,
    CONF_TIMEZONE,
    CONF_UPDATE_INTERVAL,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE, ID, EsphomeError
from esphome.cpp_generator import MockObj, TemplateArgsType
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["network"]

CONF_SNTP = "sntp"
CONF_SNTP_ID = "sntp_id"

sntp_ns = cg.esphome_ns.namespace("sntp")
SNTPComponent = sntp_ns.class_("SNTPComponent", time_.RealTimeClock)
SetTimezoneAction = sntp_ns.class_(
    "SetTimezoneAction", automation.Action, cg.Parented.template(SNTPComponent)
)

DEFAULT_SERVERS = ["0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"]

CONF_ZONE = "zone"
SERVICE_TIME_NOW = "time.now"
ZONE_IP = "ip"
# Must match SNTPComponent::MAX_ZONE_LENGTH
MAX_ZONE_LENGTH = 47


def validate_zone(value: object) -> str:
    """Validate a Region/City zone name, or the token 'ip'."""
    value = cv.string_strict(value)
    if value.lower() == ZONE_IP:
        return ZONE_IP
    if not time_.is_valid_iana_zone(value):
        raise cv.Invalid(
            f"Unknown zone '{value}'. Use '{ZONE_IP}' or a Region/City name from "
            "https://en.wikipedia.org/wiki/List_of_tz_database_time_zones"
        )
    if len(value) > MAX_ZONE_LENGTH:
        raise cv.Invalid(f"Zone names can be at most {MAX_ZONE_LENGTH} characters")
    return value


TIMEZONE_SERVICE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Optional(CONF_SERVICE, default=SERVICE_TIME_NOW): cv.one_of(
                SERVICE_TIME_NOW, lower=True
            ),
            cv.Required(CONF_ZONE): validate_zone,
            cv.Optional(CONF_UPDATE_INTERVAL, default="1h"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(minutes=1)),
            ),
        }
    ),
    # The platforms http_request supports
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_RP2]),
)


def validate_timezone(value: object) -> object:
    """A POSIX or Region/City timezone fixed at build time, or a service to fetch it from at runtime."""
    if isinstance(value, dict):
        return TIMEZONE_SERVICE_SCHEMA(value)
    return cv.All(
        cv.only_with_framework(["arduino", "esp-idf", "host"]), time_.validate_tz
    )(value)


def uses_timezone_service(config: ConfigType) -> bool:
    return isinstance(config.get(CONF_TIMEZONE), dict)


def _sntp_final_validate(config: ConfigType) -> None:
    """Merge multiple SNTP instances into one, similar to OTA merging behavior."""
    full_conf = fv.full_config.get()
    time_confs = full_conf.get(CONF_TIME, [])

    sntp_configs: list[ConfigType] = []
    other_time_configs: list[ConfigType] = []

    for time_conf in time_confs:
        if time_conf.get(CONF_PLATFORM) == CONF_SNTP:
            sntp_configs.append(time_conf)
        else:
            other_time_configs.append(time_conf)

    if len(sntp_configs) <= 1:
        return

    # Merge all SNTP configs into the first one
    merged = sntp_configs[0]
    for sntp_conf in sntp_configs[1:]:
        # Validate that IDs are consistent if manually specified
        if merged[CONF_ID].is_manual and sntp_conf[CONF_ID].is_manual:
            raise cv.Invalid(
                f"Found multiple SNTP configurations but {CONF_ID} is inconsistent"
            )
        merged = merge_config(merged, sntp_conf)

    # Deduplicate servers while preserving order
    servers = merged[CONF_SERVERS]
    unique_servers = list(dict.fromkeys(servers))

    # Warn if we're dropping servers due to 3-server limit
    if len(unique_servers) > 3:
        dropped = unique_servers[3:]
        unique_servers = unique_servers[:3]
        _LOGGER.warning(
            "SNTP supports maximum 3 servers. Dropped excess server(s): %s",
            dropped,
        )

    merged[CONF_SERVERS] = unique_servers

    _LOGGER.warning(
        "Found and merged %d SNTP time configurations into one instance",
        len(sntp_configs),
    )

    # Replace time configs with merged SNTP + other time platforms
    other_time_configs.append(merged)
    full_conf[CONF_TIME] = other_time_configs
    fv.full_config.set(full_conf)


CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SNTPComponent),
            cv.Optional(CONF_SERVERS, default=DEFAULT_SERVERS): cv.All(
                cv.ensure_list(cv.Any(cv.domain, cv.hostname)), cv.Length(min=1, max=3)
            ),
            cv.Optional(CONF_TIMEZONE): validate_timezone,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_RP2,
            PLATFORM_BK72XX,
            PLATFORM_LN882X,
            PLATFORM_RTL87XX,
        ]
    ),
)

FINAL_VALIDATE_SCHEMA = _sntp_final_validate


async def to_code(config: ConfigType) -> None:
    servers = config[CONF_SERVERS]

    # Define server count at compile time
    cg.add_define("SNTP_SERVER_COUNT", len(servers))

    # Pass string literals to constructor - stored in flash/rodata by compiler
    var = cg.new_Pvariable(config[CONF_ID], servers)

    await cg.register_component(var, config)

    if uses_timezone_service(config):
        tz_config = config[CONF_TIMEZONE]
        cg.add_define("USE_SNTP_TIMEZONE_SERVICE")
        # Needed by the runtime zone, even if no build time zone is available below
        cg.add_define("USE_TIME_TIMEZONE")
        http_request = await cg.get_variable(tz_config[CONF_HTTP_REQUEST_ID])
        zone = tz_config[CONF_ZONE]
        cg.add(
            var.set_timezone_service(
                http_request, zone, tz_config[CONF_UPDATE_INTERVAL]
            )
        )
        # Until the service answers, use the rules for the configured zone, or for the
        # zone of the build machine when looking it up by IP address
        if zone == ZONE_IP:
            try:
                initial_tz = time_.detect_tz() or ""
            except EsphomeError:
                initial_tz = ""
        else:
            initial_tz = time_.validate_tz(zone)
        config = {**config, CONF_TIMEZONE: initial_tz}

    await time_.register_time(var, config)

    if CORE.is_esp8266 and len(servers) > 1:
        # We need LwIP features enabled to get 3 SNTP servers (not just one)
        cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY")


@automation.register_action(
    "time.sntp.set_timezone",
    SetTimezoneAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(SNTPComponent),
            cv.Required(CONF_ZONE): cv.templatable(validate_zone),
        },
        key=CONF_ZONE,
    ),
    synchronous=True,
)
async def sntp_set_timezone_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    sntp_conf = next(
        (
            conf
            for conf in CORE.config.get(CONF_TIME, [])
            if conf.get(CONF_PLATFORM) == CONF_SNTP
            and conf[CONF_ID].id == config[CONF_ID].id
        ),
        None,
    )
    if sntp_conf is None or not uses_timezone_service(sntp_conf):
        raise EsphomeError(
            "time.sntp.set_timezone needs the sntp time 'timezone' option to be set "
            f"to a service, for example 'timezone: {{zone: {ZONE_IP}}}'"
        )
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    zone = await cg.templatable(config[CONF_ZONE], args, cg.std_string)
    cg.add(var.set_zone(zone))
    return var
