#!/usr/bin/env python3

import argparse
import re
import sys


def sub(path, pattern, repl, expected_count=1):
    with open(path) as fh:
        content = fh.read()
    content, count = re.subn(pattern, repl, content, flags=re.MULTILINE)
    if expected_count is not None:
        assert count == expected_count, f"Pattern {pattern} replacement failed!"
    with open(path, "wt") as fh:
        fh.write(content)


def write_version(version: str):
    for p in [
            ".github/workflows/ci-docker.yml",
            ".github/workflows/release-dev.yml",
            ".github/workflows/release.yml"
    ]:
        sub(
            p,
            r'base_version=".*"',
            f'base_version="{version}"'
        )

    sub(
        "docker/Dockerfile",
        r"ARG BUILD_FROM=esphome/esphome-base-amd64:.*",
        f"ARG BUILD_FROM=esphome/esphome-base-amd64:{version}"
    )
    sub(
        "docker/Dockerfile.dev",
        r"FROM esphome/esphome-base-amd64:.*",
        f"FROM esphome/esphome-base-amd64:{version}"
    )
    sub(
        "docker/Dockerfile.lint",
        r"FROM esphome/esphome-lint-base:.*",
        f"FROM esphome/esphome-lint-base:{version}"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('new_version', type=str)
    args = parser.parse_args()

    version = args.new_version
    print(f"Bumping to {version}")
    write_version(version)
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
