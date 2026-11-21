import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # the host tests cover the watchdog, which the config of a device compiles in
        cg.add_define("USE_TUYA_MCU_WATCHDOG")
        cg.add_define("USE_TUYA_MCU_WATCHDOG_RESET")
        cg.add_define("USE_UART_TX_BREAK")

    manifest.to_code = to_code_testing
