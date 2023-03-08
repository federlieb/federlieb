#!/bin/bash

# conan 1.x
# conan profile new default --detect

conan profile detect
conan profile update settings.compiler.libcxx=libstdc++11 default
build_dir="$(realpath "$(dirname "$BASH_SOURCE")")"/../build
mkdir -p "$build_dir"
cd "$build_dir" && conan install ../conanfile.txt
