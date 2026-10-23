from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # teleinfo.h includes uart.h; the hub's own to_code needs a configured UART bus.
    async def to_code_testing(config: ConfigType) -> None:
        pass

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["uart"]
