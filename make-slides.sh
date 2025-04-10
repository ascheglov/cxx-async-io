#!/bin/bash
set -eu

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"/slides

python3 ../patch-slides.py

docker run \
  -v "$PWD:/home/marp/app/" \
  --rm \
  ghcr.io/marp-team/marp-cli -I . --allow-local-files --pdf

xdg-open slides.pdf
