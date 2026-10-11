from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    from esphome.components.remote_base import request_protocol

    async def to_code_testing(config: ConfigType) -> None:
        # Protocol sources are compiled only behind their define; keep the ones under test.
        request_protocol("hob2hood")

    manifest.to_code = to_code_testing
    # AUTO_LOAD is not resolved by the unit test build.
    manifest.dependencies = manifest.dependencies + ["binary_sensor"]
