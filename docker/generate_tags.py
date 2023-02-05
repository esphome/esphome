#!/usr/bin/env python3
import re
import os
import argparse
import json

CHANNEL_DEV = "dev"
CHANNEL_BETA = "beta"
CHANNEL_RELEASE = "release"

parser = argparse.ArgumentParser()
parser.add_argument(
    "--tag",
    type=str,
    required=True,
    help="The main docker tag to push to. If a version number also adds latest and/or beta tag",
)
parser.add_argument(
    "--suffix",
    type=str,
    required=True,
    help="The suffix of the tag.",
)


def main():
    args = parser.parse_args()

    # detect channel from tag
    match = re.match(r"^(\d+\.\d+)(?:\.\d+)?(b\d+)?$", args.tag)
    major_minor_version = None
    if match is None:
        channel = CHANNEL_DEV
    elif match.group(2) is None:
        major_minor_version = match.group(1)
        channel = CHANNEL_RELEASE
    else:
        channel = CHANNEL_BETA

    tags_to_push = [args.tag]
    if channel == CHANNEL_DEV:
        tags_to_push.append("dev")
    elif channel == CHANNEL_BETA:
        tags_to_push.append("beta")
    elif channel == CHANNEL_RELEASE:
        # Additionally push to beta
        tags_to_push.append("beta")
        tags_to_push.append("latest")

        if major_minor_version:
            tags_to_push.append("stable")
            tags_to_push.append(major_minor_version)

    suffix = f"-{args.suffix}" if args.suffix else ""

    with open(os.environ["GITHUB_OUTPUT"], "w") as f:
        print(f"channel={channel}", file=f)
        print(f"image=esphome/esphome{suffix}", file=f)
        full_tags = []

        for tag in tags_to_push:
            full_tags += [f"ghcr.io/esphome/esphome{suffix}:{tag}"]
            full_tags += [f"esphome/esphome{suffix}:{tag}"]
        print(f"tags={','.join(full_tags)}", file=f)


if __name__ == "__main__":
    main()
