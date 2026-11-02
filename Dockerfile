# ==========================================
# OmniIPC Framework Container
# Linux Kernel POSIX Environment + Web Gateway
# ==========================================
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV PORT=8080

# Install C++20 build toolchain, Python 3, and POSIX IPC utilities
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    python3 \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy project files
COPY . /app

# Compile native Linux C++20 IPC engine
WORKDIR /app/backend
RUN make clean && make -j$(nproc) CXX=g++

WORKDIR /app

# Expose HTTP and SSE Telemetry port
EXPOSE 8080

# Run Telemetry Server
CMD ["python3", "backend/server.py"]
