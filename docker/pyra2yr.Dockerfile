FROM ubuntu:23.04 as python
RUN apt-get update && \
    apt-get install -y \
    ca-certificates \
    python3 \
    python3-pip \
    python3-venv \
    python3-poetry \
    && apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*
