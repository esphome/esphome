import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    manifest.enable_codegen()
    to_code = manifest.to_code

    async def to_code_testing(config: ConfigType) -> None:
        # the host tests cover the default of set_tx_break()
        cg.add_define("USE_UART_TX_BREAK")
        await to_code(config)

    manifest.to_code = to_code_testing
