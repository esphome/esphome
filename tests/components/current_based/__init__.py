import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        for platform in ("cover", "sensor"):
            CORE.testing_ensure_platform_registered(platform)
        cg.add_define("USE_CURRENT_BASED_COVER_LEARN_DURATIONS")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["cover", "sensor"]
