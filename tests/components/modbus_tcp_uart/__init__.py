# This file's presence makes pytest treat this directory as a package named
# "modbus_tcp_uart"; required for cpp unit testing.
import esphome.codegen as cg
from esphome.components import socket as socket_component
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        # The gtests with several clients use the tcp_uart listener and connection pool.
        socket_component.require_tcp_listener()
        cg.add_define("USE_TCP_UART_MAX_CONNECTIONS")

    manifest.to_code = to_code_testing
