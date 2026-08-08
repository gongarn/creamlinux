FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update \
    && apt upgrade -y \
    && apt install -y --no-install-recommends \
    build-essential gcc-multilib g++-multilib cmake git zip

WORKDIR /usr/src/creamlinux