from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        for platform in ("sensor", "binary_sensor"):
            CORE.testing_ensure_platform_registered(platform)

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["sensor", "binary_sensor"]
