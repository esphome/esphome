import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The frame tests drive TuyaUART with the low_power protocol compiled in
        cg.add_define("USE_TUYA_LOW_POWER")

    manifest.to_code = to_code_testing
    manifest.dependencies = [*manifest.dependencies, "uart"]
