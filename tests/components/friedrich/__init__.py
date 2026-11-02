from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    from esphome.components.remote_base import request_protocol

    # The harness walks the component manifest, not the climate platform, so declare its auto-load here
    manifest.auto_load = ["climate_ir", "climate"]

    async def to_code_testing(config: ConfigType) -> None:
        # The AEHA source is compiled only behind its define; climate.py requests it in the real build.
        request_protocol("aeha")

    manifest.to_code = to_code_testing
