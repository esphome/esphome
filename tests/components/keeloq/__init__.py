from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    from esphome.components.remote_base import request_protocol

    async def to_code_testing(config: ConfigType) -> None:
        request_protocol("keeloq")

    manifest.to_code = to_code_testing
