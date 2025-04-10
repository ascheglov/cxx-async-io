#!/bin/bash
set -eu

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
shopt -s globstar # ‘**’ will match all files and zero or more directories and subdirectories
clang-format -i ./**/*.{cc,h}
