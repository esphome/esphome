"""Run the ESP32 lifecycle methods with failure-injecting driver stubs."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
COMPONENT = ROOT / "esphome/components/ethernet"


def extract_method(source: str, signature: str) -> str:
    """Extract a production method including its body."""
    start = source.index(signature)
    end = source.index("{", start)
    depth = 1
    while depth:
        end += 1
        depth += (source[end] == "{") - (source[end] == "}")
    return source[start : end + 1]


class EthernetLifecycleTest(unittest.TestCase):
    """Test driver errors and deferred event delivery without real hardware."""

    def test_driver_failures_and_stop_barrier(self) -> None:
        """A rejected or unacknowledged stop must never permit restart."""
        source = (COMPONENT / "ethernet_component_esp32.cpp").read_text()
        header = (COMPONENT / "ethernet_component.h").read_text()
        methods = "\n".join(
            extract_method(source, signature)
            for signature in (
                "void EthernetComponent::enable()",
                "void EthernetComponent::loop()",
                "void EthernetComponent::disable()",
                "void EthernetComponent::eth_event_handler(",
            )
        )
        accessors = "\n".join(
            extract_method(header, signature)
            for signature in (
                "bool is_driver_stopped()",
                "bool is_connected()",
            )
        )
        fixture = (Path(__file__).parent / "fixtures/lifecycle.cpp").read_text()
        fixture = fixture.replace("// PRODUCTION_METHODS", methods)
        fixture = fixture.replace("// PRODUCTION_ACCESSORS", accessors)
        with tempfile.TemporaryDirectory(prefix="esphome-ethernet-test-") as directory:
            cpp = Path(directory) / "test.cpp"
            binary = Path(directory) / "test"
            cpp.write_text(fixture)
            command = [
                os.environ.get("CXX", "c++"),
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-unused-parameter",
                "-Wno-unused-but-set-variable",
            ]
            if sys.platform == "darwin":
                sdk = subprocess.check_output(
                    ["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True
                ).strip()
                command.extend(["-isystem", str(Path(sdk) / "usr/include/c++/v1")])
            result = subprocess.run(
                [*command, str(cpp), "-o", str(binary)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(binary)], check=True, timeout=15)
