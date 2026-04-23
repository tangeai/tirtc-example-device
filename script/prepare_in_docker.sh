#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

. "$script_dir/docker_common.sh"

print_usage() {
  cat <<'USAGE'
Usage: ./script/prepare_in_docker.sh

Behavior:
  1. Ensure the local Docker image is ready
  2. Run ./script/prepare.sh inside the Docker environment

Environment overrides:
  DEVICE_DEMO_DOCKER_IMAGE
  DEVICE_DEMO_DOCKER_PLATFORM
USAGE
}

case "${1:-}" in
  --help)
    print_usage
    exit 0
    ;;
  '')
    ;;
  *)
    printf '[demo] unknown argument: %s\n' "$1" >&2
    exit 1
    ;;
esac

demo_build_docker_image 0
workspace_mount=$(demo_resolve_workspace_mount)
demo_configure_tty

printf '[demo] docker_image=%s\n' "$docker_image"
printf '[demo] running prepare.sh inside Docker\n'

if [ -n "$tty_flag" ]; then
  exec docker run --rm -i -t --platform "$docker_platform" \
    -v "$workspace_mount:/workspace" \
    -w /workspace \
    "$docker_image" \
    ./script/prepare.sh
fi

exec docker run --rm -i --platform "$docker_platform" \
  -v "$workspace_mount:/workspace" \
  -w /workspace \
  "$docker_image" \
  ./script/prepare.sh
