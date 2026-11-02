from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Turns on USE_NUMBER so the hub's number forwarding code is in the host test build
    async def to_code_testing(config: ConfigType) -> None:
        CORE.testing_ensure_platform_registered("number")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["number"]
