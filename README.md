# OmniIPC: High-Performance IPC Framework & Discord Telemetry Visualizer

An enterprise-grade systems engineering framework that abstracts, implements, and benchmarks four core Inter-Process Communication (IPC) primitives on POSIX systems (macOS and Linux), accompanied by a real-time visualization dashboard styled after Discord with dark and light themes.

---

## 1. Project Directory Layout

```text
omni-ipc/
├── backend/
│   ├── include/
│   │   ├── ipc_common.hpp       # Packed 36-byte framing header, CRC32, rusage helpers
│   │   ├── ipc_channel.hpp      # Standard polymorphic abstract C++ base class
│   │   ├── pipe_channel.hpp     # POSIX unidirectional & bidirectional pipe driver
│   │   ├── shm_channel.hpp      # POSIX Shared Memory (mmap, mutex, ring buffer)
│   │   ├── mq_channel.hpp       # System V / POSIX Message Queue driver
│   │   ├── socket_channel.hpp   # UNIX Domain Sockets (AF_UNIX stream socketpair)
│   │   └── benchmark_suite.hpp  # High-precision P50/P99 latency & throughput engine
│   ├── src/
│   │   ├── pipe_channel.cpp
│   │   ├── shm_channel.cpp
│   │   ├── mq_channel.cpp
│   │   ├── socket_channel.cpp
│   │   ├── benchmark_suite.cpp
│   │   └── main.cpp             # CLI test runner & streaming telemetry emitter
│   ├── Makefile                 # C++20 Clang/GCC build script
│   └── server.py                # Zero-dependency Python 3 SSE Telemetry Gateway
├── frontend/
│   ├── dist/
│   │   └── index.html           # Standalone production Discord visualizer (Zero npm install)
│   ├── src/
│   │   ├── App.tsx              # React + Tailwind Discord clone interface
│   │   ├── types.ts             # TypeScript IPC & telemetry interfaces
│   │   ├── index.css            # Custom Discord scrollbars and animations
│   │   └── main.tsx
│   ├── package.json             # React 18, Vite, Tailwind CSS, Lucide-React
│   ├── tailwind.config.js       # Discord exact color tokens (Dark & Light)
│   └── vite.config.ts           # Vite proxy & bundler config
├── scripts/
│   ├── build.sh                 # Builds C++ binary and validates frontend
│   └── run.sh                   # Launches gateway & opens visualizer
└── README.md
```

---

## 2. Core IPC Mechanisms Implemented

| Primitive | Transport Backing | Synchronization | Peak Bandwidth | Zero-Copy |
| :--- | :--- | :--- | :--- | :--- |
| **Shared Memory** | `shm_open`, `ftruncate`, `mmap` | `pthread_mutex` (`PTHREAD_PROCESS_SHARED`) + `pthread_cond` | **~8 - 22 GB/s** | **Yes** |
| **UNIX Sockets** | `AF_UNIX` (`SOCK_STREAM`) | Kernel socket buffer readiness (`poll`/`select`) | **~3 - 6 GB/s** | No (`sk_buff`) |
| **Pipes** | POSIX `pipe()` descriptors | OS VFS pipe buffer flow control | **~1 - 3 GB/s** | No (VFS buffer) |
| **Message Queues** | System V / POSIX `msgget` / `msgsnd` | Kernel message priority queue | **~200 - 800 MB/s** | No |

---

## 3. Discord UI Visualizer Mapping

The frontend interface translates systems concepts into Discord components:

- **Server Sidebar (Far Left):** Active System Nodes (Host OS Node, Linux Worker Cluster, Docker IPC Sandbox).
- **Channel List (Inner Left):** IPC Transports (`#shared-memory`, `#unix-domain-sockets`, `#pipes`, `#message-queues`), payload selection pills (`1KB`, `1MB`, `10MB`, `100MB`), and gateway heartbeat.
- **Main Chat Area (Center):** Real-time packet event stream formatted as Discord messages with:
  - Process Avatar & PID tags (`Process_10421 [PROD]`)
  - Timestamp with microsecond precision
  - Embedded frame card displaying Route, Payload Size, One-Way Latency, and CRC32 Checksum
  - Interactive bottom command bar to inject bursts or run live C++ benchmarks.
- **Members List (Right):** Running process threads (Producer, Consumer, Broker, Watcher) with live CPU%, Memory usage, and status badges (Online, Idle, High Load).
- **Theming:** Full Dark/Light mode toggle matching Discord colors:
  - Dark Mode: `#313338` (Chat), `#2B2D31` (Channels/Members), `#1E1F22` (Server Sidebar).
  - Light Mode: `#FFFFFF` (Chat), `#F2F3F5` (Channels/Members), `#E3E5E8` (Server Sidebar).
  - Accent: Discord Blurple `#5865F2` and Status Green `#57F287`.

---

## 4. Quick Start & Execution

### Option A: Complete Real-Time Visualizer
```bash
# Navigate to the project root
cd /Users/mayanksharma/.gemini/antigravity/scratch/omni-ipc

# Run the launch script (builds C++ engine and starts telemetry gateway)
./scripts/run.sh
```
Open **`http://localhost:8080`** in any web browser to view the real-time Discord visualizer!

### Option B: Standalone C++ Benchmark CLI
```bash
cd backend
make

# Benchmark Shared Memory with 1MB payload across 20 iterations
./omni_ipc_engine --mechanism shm --size 1MB --iterations 20

# Benchmark UNIX Domain Sockets with 10MB payload
./omni_ipc_engine --mechanism sockets --size 10MB --iterations 10

# Stream raw JSON telemetry lines directly
./omni_ipc_engine --mechanism shm --size 1MB --iterations 5 --stream-json
```

---

## 5. REST & Streaming API Endpoints

The embedded Python gateway (`backend/server.py`) provides:

- `GET /api/events`: Server-Sent Events (SSE) streaming live packet telemetry at up to 60Hz.
- `GET /api/status`: Health check returning active processes, CPU, memory, and channel metadata.
- `POST /api/run-benchmark`: Triggers the compiled C++ executable with JSON payload:
  ```json
  {
    "mechanism": "shm",
    "size": "10MB",
    "iterations": 20
  }
  ```
- `POST /api/toggle-streaming`: Pauses or resumes the live packet stream.
