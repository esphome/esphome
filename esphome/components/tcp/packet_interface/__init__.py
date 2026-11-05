import esphome.codegen as cg
from esphome.components import binary_sensor, packet_interface, socket
from esphome.components.const import (
    CONF_ALLOWED_IPS,
    CONF_CONNECTED,
    CONF_HOST,
    CONF_RECONNECT_INTERVAL,
    CONF_ROLE,
)
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    new_packet_interface,
    packet_interface_schema,
)
from esphome.components.packet_interface.layers import (
    CONF_ADDRESSING,
    CONF_RELIABILITY,
    CONF_SESSION,
)
from esphome.components.packet_interface.layers.basic_session import CONF_KEEP_ALIVE
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_TIMEOUT,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["network"]


def AUTO_LOAD(config: ConfigType | None = None) -> list[str]:
    """The binary_sensor component only for the connected sensor; without a config, as tooling calls it, both."""
    if not config or CONF_CONNECTED in config:
        return ["socket", "binary_sensor"]
    return ["socket"]


# Each packet and its 2 byte length must fit the socket link's 1024 byte send buffer
MAX_PACKET_SIZE = 1022

tcp_ns = cg.esphome_ns.namespace("tcp")
TcpPacketInterface = tcp_ns.class_(
    "TcpPacketInterface", packet_interface.PacketInterface
)


def _timeout(config: ConfigType) -> ConfigType:
    """Default the timeout to three keep-alive intervals, and require it to be longer than one."""
    keep_alive = config[CONF_KEEP_ALIVE].total_milliseconds
    if (timeout := config.get(CONF_TIMEOUT)) is None:
        config[CONF_TIMEOUT] = cv.positive_time_period_milliseconds(
            f"{keep_alive * 3}ms"
        )
    elif timeout.total_milliseconds <= keep_alive:
        raise cv.Invalid("Must be longer than keep_alive", path=[CONF_TIMEOUT])
    return config


BASE_SCHEMA = packet_interface_schema(
    TcpPacketInterface,
    MAX_PACKET_SIZE,
    MAX_PACKET_SIZE,
    # TCP resends what is lost, joins exactly two devices, and its connection state is the session
    provides=(CONF_ADDRESSING, CONF_RELIABILITY, CONF_SESSION),
).extend(
    {
        cv.Required(CONF_PORT): cv.port,
        cv.Optional(
            CONF_RECONNECT_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_KEEP_ALIVE, default="10s"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(hours=1)),
        ),
        # Nothing received for this long closes the connection; it must be longer than the peer's keep_alive
        cv.Optional(CONF_TIMEOUT): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(max=cv.TimePeriod(hours=3)),
        ),
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "client": cv.All(
                BASE_SCHEMA.extend({cv.Required(CONF_HOST): socket.ipv4_host}),
                _timeout,
            ),
            "server": cv.All(
                BASE_SCHEMA.extend(
                    {cv.Optional(CONF_ALLOWED_IPS): socket.IPV4_ALLOW_SCHEMA}
                ),
                _timeout,
            ),
        },
        key=CONF_ROLE,
        default_type="client",
    ),
    # A client uses one socket; a server one listener and one accepted socket
    socket.consume_role_sockets("tcp.packet_interface"),
)


async def to_code(config: ConfigType) -> None:
    # The receive buffer's size is known here, so it is a static array rather than allocated at run time
    rx_buffer = f"{config[CONF_ID].id}__rx_buffer"
    cg.add_global(
        cg.RawStatement(f"static uint8_t {rx_buffer}[{config[CONF_MAX_PACKET_SIZE]}];")
    )
    var = await new_packet_interface(config, cg.RawExpression(rx_buffer))
    if config[CONF_ROLE] == "server":
        socket.require_tcp_listener()
        cg.add(var.set_server(True))
        socket.add_ipv4_allow(
            var.set_allow, config.get(CONF_ALLOWED_IPS), config[CONF_ID]
        )
    else:
        socket.require_tcp_client_link()
        cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_keep_alive(config[CONF_KEEP_ALIVE]))
    cg.add(var.set_receive_timeout(config[CONF_TIMEOUT]))
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CONNECTED, var.set_connected_sensor)
