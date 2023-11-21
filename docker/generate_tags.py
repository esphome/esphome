#!/usr/bin/env python3
import re
import argparse

CHANNEL_DEV = "dev"
CHANNEL_BETA = "beta"
CHANNEL_RELEASE = "release"

GHCR = "ghcr"
DOCKERHUB = "dockerhub"

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
parser.add_argument(
    "--registry",
    type=str,
    choices=[GHCR, DOCKERHUB],
    required=False,
    action="append",
    help="The registry to build tags for.",
)


def main():
    args = parser.parse_args()

    # detect channel from tag
    match = re.match(r"^(\d+\.\d+)(?:\.\d+)(?:(b\d+)|(-dev\d+))?$", args.tag)
    major_minor_version = None
    if match is None:  # eg 2023.12.0-dev20231109-testbranch
        channel = None  # Ran with custom tag for a branch etc
    elif match.group(3) is not None:  # eg 2023.12.0-dev20231109
        channel = CHANNEL_DEV
    elif match.group(2) is not None:  # eg 2023.12.0b1
        channel = CHANNEL_BETA
    else:  # eg 2023.12.0
        major_minor_version = match.group(1)
        channel = CHANNEL_RELEASE

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

    image_name = f"esphome/esphome{suffix}"

    print(f"channel={channel}")

    if args.registry is None:
        args.registry = [GHCR, DOCKERHUB]
    elif len(args.registry) == 1:
        if GHCR in args.registry:
            print(f"image=ghcr.io/{image_name}")
        if DOCKERHUB in args.registry:
            print(f"image=docker.io/{image_name}")

    print(f"image_name={image_name}")

    full_tags = []

    for tag in tags_to_push:
        if GHCR in args.registry:
            full_tags += [f"ghcr.io/{image_name}:{tag}"]
        if DOCKERHUB in args.registry:
            full_tags += [f"docker.io/{image_name}:{tag}"]
    print(f"tags={','.join(full_tags)}")


if __name__ == "__main__":
    main()
