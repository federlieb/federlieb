#!/bin/bash

# conan 1.x
# conan profile new default --detect

conan profile detect
build_dir="$(realpath "$(dirname "$BASH_SOURCE")")"/../build
mkdir -p "$build_dir"
cd "$build_dir" && conan install -s compiler.libcxx=libstdc++11 ../conanfile.txt
