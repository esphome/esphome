import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # esp32_camera does not build on the host, and the cropping under test only
    # needs the camera interfaces and the decoder's luma weights.
    manifest.dependencies = ["camera", "camera_decoder"]

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_CAMERA")
        cg.add_define("USE_SSOCR_READER")
        cg.add_define("USE_SSOCR_READER_RGB888")

    manifest.to_code = to_code_testing
