from esphome.loader import FileResource
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Only the capture time helper is under test; the microphone platform is not in this build
    manifest.add_resources(
        FileResource("esphome.components.i2s_audio", "microphone/capture_time.h"),
    )
    # For AudioStreamInfo, which the microphone platform would otherwise bring in
    manifest.dependencies = manifest.dependencies + ["audio"]
