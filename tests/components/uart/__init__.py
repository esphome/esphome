from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a uart packet_interface, whose to_code waits on the uart bus variable.
    # The decoder's auto baud path only compiles with the define auto_baud_sync_byte emits.
    add_defines_to_code(manifest, "USE_UART_AUTO_BAUD")
