import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # the host tests cover the watchdog, which the config of a device compiles in
        cg.add_define("USE_TUYA_MCU_WATCHDOG")

    manifest.to_code = to_code_testing
