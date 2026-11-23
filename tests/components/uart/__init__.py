import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    manifest.enable_codegen()
    uart_to_code = manifest.to_code

    async def to_code_testing(config: ConfigType) -> None:
        # The virtual UART reports to a debugger only when one is configured.
        cg.add_define("USE_UART_DEBUGGER")
        await uart_to_code(config)

    manifest.to_code = to_code_testing
