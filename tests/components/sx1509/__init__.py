import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One empty config entry so to_code runs and compiles the LED channel path.
    manifest.multi_conf = False
    manifest.dependencies = manifest.dependencies + ["output"]

    async def to_code(config: ConfigType) -> None:
        cg.add_define("SX1509_LED_CHANNEL_COUNT", 3)
        cg.add_define("USE_OUTPUT_FLOAT_POWER_SCALING")

    manifest.to_code = to_code
