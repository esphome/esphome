# This file's presence makes pytest treat this directory as a package named
# "uart"; required for cpp unit testing.
import esphome.codegen as cg
from esphome.components import uart
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One config entry, so the stub runs once: the gtests link the filtered virtual UART base.
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        uart.require_virtual_uart()
        # The virtual UART reports to a debugger only when one is configured.
        cg.add_define("USE_UART_DEBUGGER")

    manifest.to_code = to_code_testing
