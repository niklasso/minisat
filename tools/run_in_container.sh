#!/usr/bin/env bash
#
# Copyright Norbert Manthey, 2019

# Run commands inside container

set -ex
DOCKERFILE="$1"  # Dockerfile
shift

USER_FLAGS="-e USER="$(id -u)" -u=$(id -u)"

# disable getting the source again
if [ -z "$1" ] || [ "$1" = "sudo" ]
then
	USER_FLAGS=""
	shift
fi

if [ ! -r "$DOCKERFILE" ]
then
	echo "cannot find $DOCKERFILE (in $(readlink -e .)), abort"
	exit 1
fi

DOCKERFILE_DIR=$(dirname "$DOCKERFILE")
CONTAINER="${CONTAINER:-}"
[ -n "$CONTAINER" ] || CONTAINER=$(docker build -q -f "$DOCKERFILE" "$DOCKERFILE_DIR")

echo "running in container: $CONTAINER"

docker run \
  -it \
  $USER_FLAGS \
  -v $HOME:$HOME \
  -v /tmp/build_output:/tmp/build_output \
  -w $(pwd) \
  ${DOCKER_EXTRA_ARGS} \
  "$CONTAINER" "$@"
