#!/usr/bin/env python3
import argparse
from dataclasses import dataclass
import re
import shlex
import subprocess
import sys

CHANNEL_DEV = "dev"
CHANNEL_BETA = "beta"
CHANNEL_RELEASE = "release"
CHANNELS = [CHANNEL_DEV, CHANNEL_BETA, CHANNEL_RELEASE]

ARCH_AMD64 = "amd64"
ARCH_AARCH64 = "aarch64"
ARCHS = [ARCH_AMD64, ARCH_AARCH64]

TYPE_DOCKER = "docker"
TYPE_HA_ADDON = "ha-addon"
TYPE_LINT = "lint"
TYPES = [TYPE_DOCKER, TYPE_HA_ADDON, TYPE_LINT]


parser = argparse.ArgumentParser()
parser.add_argument(
    "--tag",
    type=str,
    required=True,
    help="The main docker tag to push to. If a version number also adds latest and/or beta tag",
)
parser.add_argument(
    "--arch", choices=ARCHS, required=False, help="The architecture to build for"
)
parser.add_argument(
    "--build-type", choices=TYPES, required=True, help="The type of build to run"
)
parser.add_argument(
    "--dry-run", action="store_true", help="Don't run any commands, just print them"
)
subparsers = parser.add_subparsers(
    help="Action to perform", dest="command", required=True
)
build_parser = subparsers.add_parser("build", help="Build the image")
build_parser.add_argument("--push", help="Also push the images", action="store_true")
build_parser.add_argument(
    "--load", help="Load the docker image locally", action="store_true"
)
manifest_parser = subparsers.add_parser(
    "manifest", help="Create a manifest from already pushed images"
)


@dataclass(frozen=True)
class DockerParams:
    build_to: str
    manifest_to: str
    baseimgtype: str
    platform: str
    target: str

    @classmethod
    def for_type_arch(cls, build_type, arch):
        prefix = {
            TYPE_DOCKER: "esphome/esphome",
            TYPE_HA_ADDON: "esphome/esphome-hassio",
            TYPE_LINT: "esphome/esphome-lint",
        }[build_type]
        build_to = f"{prefix}-{arch}"
        baseimgtype = {
            TYPE_DOCKER: "docker",
            TYPE_HA_ADDON: "hassio",
            TYPE_LINT: "docker",
        }[build_type]
        platform = {
            ARCH_AMD64: "linux/amd64",
            ARCH_AARCH64: "linux/arm64",
        }[arch]
        target = {
            TYPE_DOCKER: "docker",
            TYPE_HA_ADDON: "hassio",
            TYPE_LINT: "lint",
        }[build_type]
        return cls(
            build_to=build_to,
            manifest_to=prefix,
            baseimgtype=baseimgtype,
            platform=platform,
            target=target,
        )


def main():
    args = parser.parse_args()

    def run_command(*cmd, ignore_error: bool = False):
        print(f"$ {shlex.join(list(cmd))}")
        if not args.dry_run:
            rc = subprocess.call(list(cmd))
            if rc != 0 and not ignore_error:
                print("Command failed")
                sys.exit(1)

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

        # Compatibility with HA tags
        if major_minor_version:
            tags_to_push.append("stable")
            tags_to_push.append(major_minor_version)

    if args.command == "build":
        # 1. pull cache image
        params = DockerParams.for_type_arch(args.build_type, args.arch)
        cache_tag = {
            CHANNEL_DEV: "cache-dev",
            CHANNEL_BETA: "cache-beta",
            CHANNEL_RELEASE: "cache-latest",
        }[channel]
        cache_img = f"ghcr.io/{params.build_to}:{cache_tag}"

        imgs = [f"{params.build_to}:{tag}" for tag in tags_to_push]
        imgs += [f"ghcr.io/{params.build_to}:{tag}" for tag in tags_to_push]

        # 3. build
        cmd = [
            "docker",
            "buildx",
            "build",
            "--build-arg",
            f"BASEIMGTYPE={params.baseimgtype}",
            "--build-arg",
            f"BUILD_VERSION={args.tag}",
            "--cache-from",
            f"type=registry,ref={cache_img}",
            "--file",
            "docker/Dockerfile",
            "--platform",
            params.platform,
            "--target",
            params.target,
        ]
        for img in imgs:
            cmd += ["--tag", img]
        if args.push:
            cmd += ["--push", "--cache-to", f"type=registry,ref={cache_img},mode=max"]
        if args.load:
            cmd += ["--load"]

        run_command(*cmd, ".")
    elif args.command == "manifest":
        manifest = DockerParams.for_type_arch(args.build_type, ARCH_AMD64).manifest_to

        targets = [f"{manifest}:{tag}" for tag in tags_to_push]
        targets += [f"ghcr.io/{manifest}:{tag}" for tag in tags_to_push]
        # 1. Create manifests
        for target in targets:
            cmd = ["docker", "manifest", "create", target]
            for arch in ARCHS:
                src = f"{DockerParams.for_type_arch(args.build_type, arch).build_to}:{args.tag}"
                if target.startswith("ghcr.io"):
                    src = f"ghcr.io/{src}"
                cmd.append(src)
            run_command(*cmd)
        # 2. Push manifests
        for target in targets:
            run_command("docker", "manifest", "push", target)


if __name__ == "__main__":
    main()
