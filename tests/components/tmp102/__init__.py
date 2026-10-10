from esphome.core import CORE
import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        CORE.testing_ensure_platform_registered("sensor")
        cg.add_define("USE_TMP102_CONFIGURE")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["i2c", "sensor"]
