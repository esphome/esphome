#!/usr/bin/env python3

import argparse
import re
import subprocess
from dataclasses import dataclass
import sys


@dataclass
class Version:
    major: int
    minor: int
    patch: int
    beta: int = 0
    dev: bool = False

    def __str__(self):
        return f'{self.major}.{self.minor}.{self.full_patch}'

    @property
    def full_patch(self):
        res = f'{self.patch}'
        if self.beta > 0:
            res += f'b{self.beta}'
        if self.dev:
            res += '-dev'
        return res

    @classmethod
    def parse(cls, value):
        match = re.match(r'(\d+).(\d+).(\d+)(b\d+)?(-dev)?', value)
        assert match is not None
        major = int(match[1])
        minor = int(match[2])
        patch = int(match[3])
        beta = int(match[4][1:]) if match[4] else 0
        dev = bool(match[5])
        return Version(
            major=major, minor=minor, patch=patch,
            beta=beta, dev=dev
        )


def sub(path, pattern, repl, expected_count=1):
    with open(path) as fh:
        content = fh.read()
    content, count = re.subn(pattern, repl, content, re.MULTILINE)
    if expected_count is not None:
        assert count == expected_count
    with open(path, "wt") as fh:
        fh.write(content)


def write_version(version: Version):
    sub(
        'esphome/const.py',
        r"^MAJOR_VERSION = \d+$",
        f"MAJOR_VERSION = {version.major}"
    )
    sub(
        'esphome/const.py',
        r"^MINOR_VERSION = \d+$",
        f"MINOR_VERSION = {version.minor}"
    )
    sub(
        'esphome/const.py',
        r"^PATCH_VERSION = .*$",
        f"PATCH_VERSION = {version.full_patch}"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('new_version', type=str)
    parser.add_argument('--commit', action='store_true')
    args = parser.parse_args()

    if args.commit and subprocess.call(["git", "diff", "--quiet"]) == 1:
        print("Cannot use --commit because git is dirty.")
        return 1

    version = Version.parse(args.new_version)
    print(f"Bumping to {version}")
    write_version(version)

    if args.commit:
        subprocess.check_call(["git", "commit", "-nam", f"Bump version to v{version}"])
    return 1


if __name__ == "__main__":
    sys.exit(main() or 0)
