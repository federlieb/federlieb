#!/bin/bash
python3 -m venv .venv
. .venv/bin/activate
pip3 install conan rich pytest pysqlite3-binary networkx
conan profile detect
build_dir="$(realpath "$(dirname "$BASH_SOURCE")")"/../build
mkdir -p "$build_dir"
conan install --output-folder "$build_dir" conanfile.txt
