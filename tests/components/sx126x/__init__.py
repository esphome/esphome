"""Enable real logger callbacks for radio reentrancy regression tests."""

from esphome.components.logger import request_log_listener
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config: ConfigType) -> None:
        request_log_listener()

    manifest.multi_conf = False
    manifest.to_code = to_code
