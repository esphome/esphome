#!/usr/bin/env bash
# Based on Home Assistant's docker builder
######################
# Hass.io Build-env
######################
set -e

echo -- "$@"

#### Variable ####

DOCKER_TIMEOUT=20
DOCKER_PID=-1
DOCKER_HUB=""
DOCKER_CACHE="true"
DOCKER_LOCAL="false"
TARGET=""
IMAGE=""
BUILD_LIST=()
BUILD_TASKS=()

#### Misc functions ####

function print_help() {
    cat << EOF
Hass.io build-env for ecosystem:
docker run --rm homeassistant/{arch}-builder:latest [options]

Options:
  -h, --help
        Display this help and exit.

  Repository / Data
    -t, --target <PATH_TO_BUILD>
        Set local folder or path inside repository for build.

  Version/Image handling
    -i, --image <IMAGE_NAME>
        Overwrite image name of build / support {arch}

  Architecture
    --armhf
        Build for arm.
    --amd64
        Build for intel/amd 64bit.
    --aarch64
        Build for arm 64bit.
    --i386
        Build for intel/amd 32bit.
    --all
        Build all architecture.

  Build handling
    --no-cache
       Disable cache for the build (from latest).
    -d, --docker-hub <DOCKER_REPOSITORY>
       Set or overwrite the docker repository.

    Use the host docker socket if mapped into container:
       /var/run/docker.sock

EOF

    exit 1
}

#### Docker functions ####

function start_docker() {
    local starttime
    local endtime

    if [ -S "/var/run/docker.sock" ]; then
        echo "[INFO] Use host docker setup with '/var/run/docker.sock'"
        DOCKER_LOCAL="true"
        return 0
    fi

    echo "[INFO] Starting docker."
    dockerd 2> /dev/null &
    DOCKER_PID=$!

    echo "[INFO] Waiting for docker to initialize..."
    starttime="$(date +%s)"
    endtime="$(date +%s)"
    until docker info >/dev/null 2>&1; do
        if [ $((endtime - starttime)) -le ${DOCKER_TIMEOUT} ]; then
            sleep 1
            endtime=$(date +%s)
        else
            echo "[ERROR] Timeout while waiting for docker to come up"
            exit 1
        fi
    done
    echo "[INFO] Docker was initialized"
}


function stop_docker() {
    local starttime
    local endtime

    if [ "$DOCKER_LOCAL" == "true" ]; then
        return 0
    fi

    echo "[INFO] Stopping in container docker..."
    if [ "$DOCKER_PID" -gt 0 ] && kill -0 "$DOCKER_PID" 2> /dev/null; then
        starttime="$(date +%s)"
        endtime="$(date +%s)"

        # Now wait for it to die
        kill "$DOCKER_PID"
        while kill -0 "$DOCKER_PID" 2> /dev/null; do
            if [ $((endtime - starttime)) -le ${DOCKER_TIMEOUT} ]; then
                sleep 1
                endtime=$(date +%s)
            else
                echo "[ERROR] Timeout while waiting for container docker to die"
                exit 1
            fi
        done
    else
        echo "[WARN] Your host might have been left with unreleased resources"
    fi
}

function run_build() {
    local build_dir=$1
    local repository=$2
    local image=$3
    local version=$4
    local build_arch=$5
    local docker_cli=("${!6}")

    local push_images=()

    # Overwrites
    if [ ! -z "$DOCKER_HUB" ]; then repository="$DOCKER_HUB"; fi
    if [ ! -z "$IMAGE" ]; then image="$IMAGE"; fi

    # Init Cache
    if [ "$DOCKER_CACHE" == "true" ]; then
        echo "[INFO] Init cache for $repository/$image:$version"
        if docker pull "$repository/$image:latest" > /dev/null 2>&1; then
            docker_cli+=("--cache-from" "$repository/$image:latest")
        else
            docker_cli+=("--no-cache")
            echo "[WARN] No cache image found. Cache is disabled for build"
        fi
    else
        docker_cli+=("--no-cache")
    fi

    # Build image
    echo "[INFO] Run build for $repository/$image:$version"
    docker build --pull -t "$repository/$image:$version" \
        --label "io.hass.version=$version" \
        --label "io.hass.arch=$build_arch" \
        -f "$TARGET/docker/Dockerfile.$build_arch" \
        "${docker_cli[@]}" \
        "$build_dir"

    echo "[INFO] Finish build for $repository/$image:$version"
    docker tag "$repository/$image:$version" "$repository/$image:dev"
}


#### HassIO functions ####

function build_addon() {
    local build_arch=$1

    local docker_cli=()
    local image=""
    local repository=""
    local raw_image=""
    local name=""
    local description=""
    local url=""
    local args=""

    # Read addon config.json
    name="$(jq --raw-output '.name // empty' "$TARGET/esphomeyaml/config.json" | sed "s/'//g")"
    description="$(jq --raw-output '.description // empty' "$TARGET/esphomeyaml/config.json" | sed "s/'//g")"
    url="$(jq --raw-output '.url // empty' "$TARGET/esphomeyaml/config.json")"
    version="$(jq --raw-output '.version' "$TARGET/esphomeyaml/config.json")"
    raw_image="$(jq --raw-output '.image // empty' "$TARGET/esphomeyaml/config.json")"

    # Read data from image
    if [ ! -z "$raw_image" ]; then
        repository="$(echo "$raw_image" | cut -f 1 -d '/')"
        image="$(echo "$raw_image" | cut -f 2 -d '/')"
    fi

    # Set additional labels
    docker_cli+=("--label" "io.hass.name=$name")
    docker_cli+=("--label" "io.hass.description=$description")
    docker_cli+=("--label" "io.hass.type=addon")

    if [ ! -z "$url" ]; then
        docker_cli+=("--label" "io.hass.url=$url")
    fi

    # Start build
    run_build "$TARGET" "$repository" "$image" "$version" \
        "$build_arch" docker_cli[@]
}

#### initialized cross-build ####

function init_crosscompile() {
    echo "[INFO] Setup crosscompiling feature"
    (
        mount binfmt_misc -t binfmt_misc /proc/sys/fs/binfmt_misc
        update-binfmts --enable qemu-arm
        update-binfmts --enable qemu-aarch64
    ) > /dev/null 2>&1 || echo "[WARN] Can't enable crosscompiling feature"
}


function clean_crosscompile() {
    echo "[INFO] Clean crosscompiling feature"
    if [ -f /proc/sys/fs/binfmt_misc ]; then
        umount /proc/sys/fs/binfmt_misc || true
    fi

    (
        update-binfmts --disable qemu-arm
        update-binfmts --disable qemu-aarch64
    ) > /dev/null 2>&1 || echo "[WARN] No crosscompiling feature found for cleanup"
}

#### Error handling ####

function error_handling() {
    stop_docker
    clean_crosscompile

    exit 1
}
trap 'error_handling' SIGINT SIGTERM

#### Parse arguments ####

while [[ $# -gt 0 ]]; do
    key=$1
    case ${key} in
        -h|--help)
            print_help
            ;;
        -t|--target)
            TARGET=$2
            shift
            ;;
        -i|--image)
            IMAGE=$2
            shift
            ;;
        --no-cache)
            DOCKER_CACHE="false"
            ;;
        -d|--docker-hub)
            DOCKER_HUB=$2
            shift
            ;;
        --armhf)
            BUILD_LIST+=("armhf")
            ;;
        --amd64)
            BUILD_LIST+=("amd64")
            ;;
        --i386)
            BUILD_LIST+=("i386")
            ;;
        --aarch64)
            BUILD_LIST+=("aarch64")
            ;;
        --all)
            BUILD_LIST=("armhf" "amd64" "i386" "aarch64")
            ;;

        *)
            echo "[WARN] $0 : Argument '$1' unknown will be Ignoring"
            ;;
    esac
    shift
done

# Check if an architecture is available
if [ "${#BUILD_LIST[@]}" -eq 0 ]; then
    echo "[ERROR] You need select an architecture for build!"
    exit 1
fi


#### Main ####

mkdir -p /data

# Setup docker env
init_crosscompile
start_docker

# Select arch build
for arch in "${BUILD_LIST[@]}"; do
    (build_addon "$arch") &
    BUILD_TASKS+=($!)
done

# Wait until all build jobs are done
wait "${BUILD_TASKS[@]}"

# Cleanup docker env
clean_crosscompile
stop_docker

exit 0
