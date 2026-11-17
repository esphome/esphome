from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The DAC is an audio_dac platform on I2C; the tests drive its power switch.
    manifest.dependencies = manifest.dependencies + ["audio_dac", "i2c", "switch"]
