from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Test the microphone helpers without the ESP32-specific driver.
    manifest.add_resources(
        FileResource("esphome.components.i2s_audio", "microphone/capture_time.h"),
        FileResource("esphome.components.i2s_audio", "microphone/dc_offset_filter.h"),
    )
    # For AudioStreamInfo, which the microphone platform would otherwise bring in
    manifest.dependencies = manifest.dependencies + ["audio"]
