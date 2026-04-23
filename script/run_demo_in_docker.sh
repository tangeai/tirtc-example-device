#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

. "$script_dir/docker_common.sh"

runner_label_key='com.tirtc.example.device.runner'
runner_label_value='run_demo_in_docker.sh'
container_name=''

require_value() {
  if [ -z "$2" ]; then
    printf '[demo] missing required argument: %s\n' "$1" >&2
    exit 1
  fi
}

build_container_name() {
  sanitized_device_id=$(printf '%s' "$device_id" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9_.-' '-')
  container_name="tirtc-device-demo-${sanitized_device_id}"
}

print_usage() {
  cat <<'USAGE'
Usage: ./script/run_demo_in_docker.sh [--endpoint <url>] --device-id <id> --device-secret-key <key> [--dry-run]

Behavior:
  1. Ensure the local Docker image is ready
  2. Remove stale demo containers for the same device_id
  3. Run ./script/build.sh and ./script/run_demo.sh inside Docker

Environment overrides:
  DEVICE_DEMO_DOCKER_IMAGE
  DEVICE_DEMO_DOCKER_PLATFORM
USAGE
}

endpoint=''
device_id=''
device_secret_key=''
dry_run='0'

while [ "$#" -gt 0 ]; do
  case "$1" in
    --endpoint)
      [ "$#" -ge 2 ] || { printf '[demo] --endpoint requires a value\n' >&2; exit 1; }
      endpoint=$2
      shift 2
      ;;
    --device-id)
      [ "$#" -ge 2 ] || { printf '[demo] --device-id requires a value\n' >&2; exit 1; }
      device_id=$2
      shift 2
      ;;
    --device-secret-key)
      [ "$#" -ge 2 ] || { printf '[demo] --device-secret-key requires a value\n' >&2; exit 1; }
      device_secret_key=$2
      shift 2
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

require_value device_id "$device_id"
require_value device_secret_key "$device_secret_key"

build_container_name
demo_build_docker_image 0
workspace_mount=$(demo_resolve_workspace_mount)
demo_configure_tty

printf '[demo] docker_image=%s\n' "$docker_image"
printf '[demo] endpoint=%s\n' "$endpoint"
printf '[demo] device_id=%s\n' "$device_id"
printf '[demo] container_name=%s\n' "$container_name"

if [ "$dry_run" = '1' ]; then
  printf '%s\n' '[demo] dry-run only; docker command not executed'
  exit 0
fi

demo_cleanup_container "$container_name" "$runner_label_key" "$runner_label_value"

if [ -n "$tty_flag" ]; then
  exec docker run --rm -i -t --platform "$docker_platform" \
    --name "$container_name" \
    --label "$runner_label_key=$runner_label_value" \
    -v "$workspace_mount:/workspace" \
    -w /workspace \
    -e DEVICE_DEMO_ENDPOINT="$endpoint" \
    -e DEVICE_DEMO_DEVICE_ID="$device_id" \
    -e DEVICE_DEMO_DEVICE_SECRET_KEY="$device_secret_key" \
    "$docker_image" \
    sh -lc 'set -eu; ./script/build.sh; if [ -n "${DEVICE_DEMO_ENDPOINT:-}" ]; then exec ./script/run_demo.sh --endpoint "$DEVICE_DEMO_ENDPOINT" --device-id "$DEVICE_DEMO_DEVICE_ID" --device-secret-key "$DEVICE_DEMO_DEVICE_SECRET_KEY"; fi; exec ./script/run_demo.sh --device-id "$DEVICE_DEMO_DEVICE_ID" --device-secret-key "$DEVICE_DEMO_DEVICE_SECRET_KEY"'
  fi

exec docker run --rm -i --platform "$docker_platform" \
  --name "$container_name" \
  --label "$runner_label_key=$runner_label_value" \
  -v "$workspace_mount:/workspace" \
  -w /workspace \
  -e DEVICE_DEMO_ENDPOINT="$endpoint" \
  -e DEVICE_DEMO_DEVICE_ID="$device_id" \
  -e DEVICE_DEMO_DEVICE_SECRET_KEY="$device_secret_key" \
  "$docker_image" \
  sh -lc 'set -eu; ./script/build.sh; if [ -n "${DEVICE_DEMO_ENDPOINT:-}" ]; then exec ./script/run_demo.sh --endpoint "$DEVICE_DEMO_ENDPOINT" --device-id "$DEVICE_DEMO_DEVICE_ID" --device-secret-key "$DEVICE_DEMO_DEVICE_SECRET_KEY"; fi; exec ./script/run_demo.sh --device-id "$DEVICE_DEMO_DEVICE_ID" --device-secret-key "$DEVICE_DEMO_DEVICE_SECRET_KEY"'
