#!/bin/bash
conan profile update settings.compiler.libcxx=libstdc++11 default
build_dir="$(realpath "$(dirname "$BASH_SOURCE")")"/../build
mkdir -p "$build_dir"
cd "$build_dir" && conan install ../conanfile.txt
