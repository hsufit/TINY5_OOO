#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
image_name="${TINY5_DOCKER_IMAGE:-tiny5-dev}"

if ! command -v docker >/dev/null 2>&1; then
    printf 'Docker is required to run the development container.\n' >&2
    exit 127
fi

# Report daemon/permission errors before deciding whether an image is missing.
docker info >/dev/null
if ! docker image inspect "$image_name" >/dev/null 2>&1; then
    printf 'Building Docker image: %s\n' "$image_name" >&2
    docker build --tag "$image_name" "$project_dir"
else
    printf 'Docker image %s already exists.\n' "$image_name" >&2
    if [[ $# -eq 0 && -t 0 && -t 1 ]]; then
        reply=""
        read -r -p 'Do you want to rebuild it? (y/N): ' reply || reply=""
        case "$reply" in
            [yY]|[yY][eE][sS])
                printf 'Rebuilding Docker image: %s\n' "$image_name" >&2
                docker build --tag "$image_name" "$project_dir"
                ;;
        esac
    fi
fi

printf 'Starting Docker container...\n' >&2
args=(run --rm --init --interactive
    --user "$(id -u):$(id -g)"
    --env HOME=/tmp
    --mount "type=bind,source=$project_dir,target=/workspace"
    --workdir /workspace)
if [[ -t 0 && -t 1 ]]; then
    args+=(--tty)
fi
if [[ $# -eq 0 ]]; then
    set -- /bin/bash
fi

exec docker "${args[@]}" "$image_name" "$@"
