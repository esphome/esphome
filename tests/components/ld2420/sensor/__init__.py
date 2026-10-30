from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Turns on USE_SENSOR so the hub's sensor is in the host test build
    async def to_code_testing(config: ConfigType) -> None:
        CORE.testing_ensure_platform_registered("sensor")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["sensor"]
