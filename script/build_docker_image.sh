#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

. "$script_dir/docker_common.sh"

rebuild='0'
dry_run='0'

print_usage() {
  cat <<'USAGE'
Usage: ./script/build_docker_image.sh [--rebuild] [--dry-run]

Behavior:
  Build the local Docker image used by prepare/build/run in-docker helpers.

Environment overrides:
  DEVICE_DEMO_DOCKER_IMAGE
  DEVICE_DEMO_DOCKER_PLATFORM
USAGE
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --rebuild)
      rebuild='1'
      shift
      ;;
    --dry-run)
      dry_run='1'
      shift
      ;;
    --help)
      print_usage
      exit 0
      ;;
    *)
      printf '[demo] unknown argument: %s\n' "$1" >&2
      exit 1
      ;;
  esac
done

demo_require_docker
printf '[demo] docker_image=%s\n' "$docker_image"
printf '[demo] docker_platform=%s\n' "$docker_platform"

if [ "$dry_run" = '1' ]; then
  printf '%s\n' '[demo] dry-run only; docker image not built'
  exit 0
fi

demo_build_docker_image "$rebuild"
printf '[demo] docker image ready: %s\n' "$docker_image"
