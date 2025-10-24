import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_SERVERS,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE
from esphome.cpp_generator import ProgmemAssignmentExpression

DEPENDENCIES = ["network"]
sntp_ns = cg.esphome_ns.namespace("sntp")
SNTPComponent = sntp_ns.class_("SNTPComponent", time_.RealTimeClock)

DEFAULT_SERVERS = ["0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"]

CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SNTPComponent),
            cv.Optional(CONF_SERVERS, default=DEFAULT_SERVERS): cv.All(
                cv.ensure_list(cv.Any(cv.domain, cv.hostname)), cv.Length(min=1, max=3)
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_RP2040,
            PLATFORM_BK72XX,
            PLATFORM_LN882X,
            PLATFORM_RTL87XX,
        ]
    ),
)


async def to_code(config):
    servers = config[CONF_SERVERS]
    server_count = len(servers)

    # Define server count at compile time
    cg.add_define("SNTP_SERVER_COUNT", server_count)

    # Generate PROGMEM strings for ESP8266, regular strings for other platforms
    if CORE.is_esp8266:
        # On ESP8266, use PROGMEM to store strings in flash
        server_vars = []
        for i, server in enumerate(servers):
            var_name = f"{config[CONF_ID].id}_server_{i}"
            # Create PROGMEM string: static const char var_name[] PROGMEM = "server";
            assignment = ProgmemAssignmentExpression(
                "char", var_name, cg.safe_exp(server)
            )
            cg.add(assignment)
            server_vars.append(cg.RawExpression(var_name))
        # Pass PROGMEM string pointers to constructor using ArrayInitializer
        var = cg.new_Pvariable(config[CONF_ID], cg.ArrayInitializer(*server_vars))
    else:
        # On other platforms, pass regular string literals to constructor
        var = cg.new_Pvariable(
            config[CONF_ID], cg.ArrayInitializer(*[cg.safe_exp(s) for s in servers])
        )

    await cg.register_component(var, config)
    await time_.register_time(var, config)

    if CORE.is_esp8266 and len(servers) > 1:
        # We need LwIP features enabled to get 3 SNTP servers (not just one)
        cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY")
