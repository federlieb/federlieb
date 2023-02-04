FROM mcr.microsoft.com/devcontainers/cpp:0-ubuntu-22.04

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends python3-pip

RUN pip3 install conan 
