from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    from esphome.components.remote_base import request_protocol

    async def to_code_testing(config: ConfigType) -> None:
        # The AEHA source is compiled only behind its define; climate.py requests it in the real build.
        request_protocol("aeha")

    manifest.to_code = to_code_testing
    # AUTO_LOAD = ["climate_ir"] sits on the climate platform manifest, which the bare `friedrich`
    # domain never sees. climate_ir does not declare `climate`, so add both.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]
