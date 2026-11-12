from esphome import codegen as cg
from esphome.core import HexInt
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    # The decoder's auto baud path only compiles with the defines auto_baud_sync_byte emits.
    async def to_code(config: ConfigType) -> None:
        await original_to_code(config)
        cg.add_define("USE_UART_AUTO_BAUD")
        cg.add_define("UART_AUTO_BAUD_SYNC_BYTE", HexInt(0x55))

    if hasattr(original_to_code, "priority"):
        to_code.priority = original_to_code.priority
    manifest.to_code = to_code
