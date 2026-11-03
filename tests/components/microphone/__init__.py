from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # MicrophoneSource needs esp-audio-libs, which only builds for the ESP32
    manifest.resources = [
        resource
        for resource in manifest.resources
        if resource.resource != "microphone_source.cpp"
    ]
