import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]

CONF_IMPLEMENTATION = "implementation"
IMPLEMENTATION_LWIP_TCP = "lwip_tcp"
IMPLEMENTATION_LWIP_SOCKETS = "lwip_sockets"
IMPLEMENTATION_BSD_SOCKETS = "bsd_sockets"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(
            CONF_IMPLEMENTATION,
            esp8266=IMPLEMENTATION_LWIP_TCP,
            esp32=IMPLEMENTATION_BSD_SOCKETS,
            rp2040=IMPLEMENTATION_LWIP_TCP,
            bk72xx=IMPLEMENTATION_LWIP_SOCKETS,
            ln882x=IMPLEMENTATION_LWIP_SOCKETS,
            rtl87xx=IMPLEMENTATION_LWIP_SOCKETS,
            host=IMPLEMENTATION_BSD_SOCKETS,
        ): cv.one_of(
            IMPLEMENTATION_LWIP_TCP,
            IMPLEMENTATION_LWIP_SOCKETS,
            IMPLEMENTATION_BSD_SOCKETS,
            lower=True,
            space="_",
        ),
    }
)


async def to_code(config):
    impl = config[CONF_IMPLEMENTATION]
    if impl == IMPLEMENTATION_LWIP_TCP:
        cg.add_define("USE_SOCKET_IMPL_LWIP_TCP")
    elif impl == IMPLEMENTATION_LWIP_SOCKETS:
        cg.add_define("USE_SOCKET_IMPL_LWIP_SOCKETS")
        cg.add_define("USE_SOCKET_SELECT_SUPPORT")
    elif impl == IMPLEMENTATION_BSD_SOCKETS:
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")
        cg.add_define("USE_SOCKET_SELECT_SUPPORT")


def FILTER_SOURCE_FILES() -> list[str]:
    """Return list of socket implementation files that aren't selected by the user."""
    impl = CORE.config["socket"][CONF_IMPLEMENTATION]

    # Build list of files to exclude based on selected implementation
    excluded = []
    if impl != IMPLEMENTATION_LWIP_TCP:
        excluded.append("lwip_raw_tcp_impl.cpp")
    if impl != IMPLEMENTATION_BSD_SOCKETS:
        excluded.append("bsd_sockets_impl.cpp")
    if impl != IMPLEMENTATION_LWIP_SOCKETS:
        excluded.append("lwip_sockets_impl.cpp")
    return excluded
