#!/bin/bash

set -eu

DST=3rd
REPO_NAME=$(basename -s .git "$REPO")

[ ! -d "$DST" ] && mkdir "$DST"
cd "$DST"
[ ! -d "$REPO_NAME" ] && git clone "$REPO"
cd "$REPO_NAME"

[ -n "${COMMIT}" ] && git reset --hard "$COMMIT"
