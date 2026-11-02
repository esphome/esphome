from esphome.components.runtime_image import enable_format
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code is suppressed in benchmark builds; the JPEG decoder is normally
    # enabled by process_runtime_image_config(), so enable it here.
    async def to_code_benchmark(config: ConfigType) -> None:
        enable_format("JPEG")

    manifest.to_code = to_code_benchmark
