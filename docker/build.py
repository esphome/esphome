#!/usr/bin/env python3
from dataclasses import dataclass
import subprocess
import argparse
import platform
import shlex
import re
import sys


CHANNEL_DEV = 'dev'
CHANNEL_BETA = 'beta'
CHANNEL_RELEASE = 'release'
CHANNELS = [CHANNEL_DEV, CHANNEL_BETA, CHANNEL_RELEASE]

ARCH_AMD64 = 'amd64'
ARCH_ARMV7 = 'armv7'
ARCH_AARCH64 = 'aarch64'
ARCHS = [ARCH_AMD64, ARCH_ARMV7, ARCH_AARCH64]

TYPE_DOCKER = 'docker'
TYPE_HA_ADDON = 'ha-addon'
TYPE_LINT = 'lint'
TYPES = [TYPE_DOCKER, TYPE_HA_ADDON, TYPE_LINT]


BASE_VERSION = "3.5.0"


parser = argparse.ArgumentParser()
parser.add_argument("--tag", type=str, required=True, help="The main docker tag to push to. If a version number also adds latest and/or beta tag")
parser.add_argument("--arch", choices=ARCHS, required=False, help="The architecture to build for")
parser.add_argument("--build-type", choices=TYPES, required=True, help="The type of build to run")
parser.add_argument("--dry-run", action="store_true", help="Don't run any commands, just print them")
subparsers = parser.add_subparsers(help="Action to perform", dest="command", required=True)
build_parser = subparsers.add_parser("build", help="Build the image")
push_parser = subparsers.add_parser("push", help="Tag the already built image and push it to docker hub")
manifest_parser = subparsers.add_parser("manifest", help="Create a manifest from already pushed images")



# only lists some possibilities, doesn't have to be perfect
# https://stackoverflow.com/a/45125525
UNAME_TO_ARCH = {
    "x86_64": ARCH_AMD64,
    "aarch64": ARCH_AARCH64,
    "aarch64_be": ARCH_AARCH64,
    "arm": ARCH_ARMV7,
}


@dataclass(frozen=True)
class DockerParams:
    build_from: str
    build_to: str
    manifest_to: str
    dockerfile: str

    @classmethod
    def for_type_arch(cls, build_type, arch):
        prefix = {
            TYPE_DOCKER: "esphome/esphome",
            TYPE_HA_ADDON: "esphome/esphome-hassio",
            TYPE_LINT: "esphome/esphome-lint"
        }[build_type]
        build_from = f"{prefix}-base-{arch}:{BASE_VERSION}"
        build_to = f"{prefix}-{arch}"
        dockerfile = {
            TYPE_DOCKER: "docker/Dockerfile",
            TYPE_HA_ADDON: "docker/Dockerfile.hassio",
            TYPE_LINT: "docker/Dockerfile.lint",
        }[build_type]
        return cls(
            build_from=build_from,
            build_to=build_to,
            manifest_to=prefix,
            dockerfile=dockerfile
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
    match = re.match(r'\d+\.\d+(?:\.\d+)?(b\d+)?', args.tag)
    if match is None:
        channel = CHANNEL_DEV
    elif match.group(1) is None:
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

    if args.command == "build":
        # 1. pull cache image
        params = DockerParams.for_type_arch(args.build_type, args.arch)
        cache_tag = {
            CHANNEL_DEV: "dev",
            CHANNEL_BETA: "beta",
            CHANNEL_RELEASE: "latest",
        }[channel]
        cache_img = f"ghcr.io/{params.build_to}:{cache_tag}"
        run_command("docker", "pull", cache_img, ignore_error=True)

        # 2. register QEMU binfmt (if not host arch)
        is_native = UNAME_TO_ARCH.get(platform.machine()) == args.arch
        if not is_native:
            run_command(
                "docker", "run", "--rm", "--privileged", "multiarch/qemu-user-static:5.2.0-2",
                "--reset", "-p", "yes"
            )

        # 3. build
        run_command(
            "docker", "build",
            "--build-arg", f"BUILD_FROM={params.build_from}",
            "--build-arg", f"BUILD_VERSION={args.tag}",
            "--tag", f"{params.build_to}:{args.tag}",
            "--cache-from", cache_img,
            "--file", params.dockerfile,
            "."
        )
    elif args.command == "push":
        params = DockerParams.for_type_arch(args.build_type, args.arch)
        imgs = [f"{params.build_to}:{tag}" for tag in tags_to_push]
        imgs += [f"ghcr.io/{params.build_to}:{tag}" for tag in tags_to_push]
        src = imgs[0]
        # 1. tag images
        for img in imgs[1:]:
            run_command(
                "docker", "tag", src, img
            )
        # 2. push images
        for img in imgs:
            run_command(
                "docker", "push", img
            )
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
            run_command(
                "docker", "manifest", "push", target
            )


if __name__ == "__main__":
    main()
