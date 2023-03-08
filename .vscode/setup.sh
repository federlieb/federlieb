#!/bin/bash

conan profile detect
build_dir="$(realpath "$(dirname "$BASH_SOURCE")")"/../build
mkdir -p "$build_dir"
conan install --output-folder "$build_dir" conanfile.txt
