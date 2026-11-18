from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The ADC is an audio_adc platform on I2C; the tests drive its input select.
    manifest.dependencies = manifest.dependencies + ["audio_adc", "i2c", "select"]
