from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real to_code runs for the instance in benchmark.yaml: it adds the dsmr_parser
    # library and, on the host, the system mbedtls
    manifest.enable_codegen()
