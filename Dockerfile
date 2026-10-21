FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    ELEKLOADER_CROSS=m68k-linux-gnu- \
    PATH=/opt/venv/bin:$PATH

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        binutils-m68k-linux-gnu \
        ca-certificates \
        gcc-m68k-linux-gnu \
        git \
        python3 \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

ARG ELEKLOADER_REF=kit-v0.5.0

RUN git clone --depth 1 --branch "$ELEKLOADER_REF" https://github.com/irpina/elekloader.git /opt/elekloader \
    && python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir /opt/elekloader

WORKDIR /workspace