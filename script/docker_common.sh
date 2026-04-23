#!/bin/sh
set -eu

default_docker_image='tirtc-device-example-env:latest'
default_docker_platform='linux/amd64'
required_image_tools='curl unzip python3 make gcc'

docker_image=${DEVICE_DEMO_DOCKER_IMAGE:-$default_docker_image}
docker_platform=${DEVICE_DEMO_DOCKER_PLATFORM:-$default_docker_platform}
tty_flag=''

demo_require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf '[demo] missing required command: %s\n' "$1" >&2
    exit 1
  fi
}

demo_require_docker() {
  demo_require_command docker
  if ! docker info >/dev/null 2>&1; then
    printf '%s\n' '[demo] docker daemon is not reachable; start Docker Desktop or another Docker engine and retry.' >&2
    exit 1
  fi
}

demo_configure_tty() {
  tty_flag=''
  if [ -t 0 ] && [ -t 1 ]; then
    tty_flag='-t'
  fi
}

demo_resolve_workspace_mount() {
  case "$(uname -s)" in
    CYGWIN*|MINGW*|MSYS*)
      if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$repo_root"
        return 0
      fi
      ;;
  esac

  printf '%s' "$repo_root"
}

demo_build_docker_image() {
  rebuild=${1:-0}

  demo_require_docker
  if [ "$rebuild" != '1' ] && docker image inspect "$docker_image" >/dev/null 2>&1; then
    return 0
  fi

  if [ "$rebuild" = '1' ]; then
    printf '[demo] rebuilding docker image %s from %s\n' "$docker_image" "$repo_root/Dockerfile"
  else
    printf '[demo] building docker image %s from %s\n' "$docker_image" "$repo_root/Dockerfile"
  fi

  docker build --platform "$docker_platform" -t "$docker_image" -f "$repo_root/Dockerfile" "$repo_root"
  demo_verify_image_tools
}

demo_verify_image_tools() {
  missing_tools=$(docker run --rm --platform "$docker_platform" "$docker_image" sh -lc '
    missing=""
    for tool in curl unzip python3 make gcc; do
      if ! command -v "$tool" >/dev/null 2>&1; then
        missing="$missing $tool"
      fi
    done
    printf "%s" "$missing"
  ')

  if [ -n "$missing_tools" ]; then
    printf '[demo] docker image %s is missing required tools:%s\n' "$docker_image" "$missing_tools" >&2
    exit 1
  fi

  printf '[demo] docker image toolchain verified:%s\n' " $required_image_tools"
}

demo_cleanup_container() {
  container_name=$1
  label_key=$2
  label_value=$3

  existing_ids=$(docker ps -aq \
    --filter "label=${label_key}=${label_value}" \
    --filter "name=${container_name}")

  if [ -z "$existing_ids" ]; then
    return 0
  fi

  for existing_id in $existing_ids; do
    printf '[demo] removing stale container %s\n' "$existing_id"
    docker rm -f "$existing_id" >/dev/null
  done
}
