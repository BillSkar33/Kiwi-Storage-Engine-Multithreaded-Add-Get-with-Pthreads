# Kiwi Storage Engine — Multithreaded Add/Get with Pthreads

> Implementation and performance study of thread-safe `add`/`get` operations on an LSM‑tree–based storage engine using POSIX Threads (pthreads).

## Abstract
This project extends the **Kiwi** storage engine with safe **multithreaded** execution of `add` and `get`. It introduces explicit synchronization, fair workload partitioning, high‑precision timing, and a benchmarking harness that reports **throughput (ops/s)** and **latency (sec/op)** per operation type. The design respects Kiwi’s background **compaction/merge** thread and avoids race conditions while preserving correctness.

## Why
Modern storage systems must handle concurrent reads and writes with predictable latency. The baseline single‑threaded path is insufficient for realistic workloads. This work adds a minimal, auditable synchronization layer and a reproducible benchmark to evaluate scaling behavior.

## Key features
- **Thread‑safe `add`/`get`** using pthreads primitives.
- **Reader‑writer locking (RWLocker)** that allows concurrent readers and exclusive writers.
- **Lock granularity** aligned with engine subsystems (e.g., memtable vs. SST) to enable parallelism without violating invariants.
- **Fair workload partitioning** across threads with deterministic distribution.
- **Monotonic timing** via `clock_gettime(CLOCK_MONOTONIC)` with per‑thread, per‑operation, and global timers.
- **Benchmarking CLI** with modes: `read`, `write`, `readwrite` and knobs for thread count, randomness, and read percentage.
- **Reproducible runs** and environment printout to ease comparisons.

## Repository layout (suggested)
```
.
├─ src/
│  ├─ engine/              # Kiwi engine code we extended (db.c/.h, memtable.c/.h, sst.c/.h, …)
│  ├─ bench/               # Benchmark harness: kiwi-bench (main, parse args, timers, results)
│  └─ sync/                # RWLocker (rwlocker.c/.h), error handling, timer helpers
├─ include/                # Public headers
├─ scripts/                # Convenience scripts for experiment runs and CSV exports
├─ report/                 # PDF report and figures (results, plots)
├─ Makefile                # Build targets for engine and bench
└─ README.md               # You are here
```


## Build

### Option A — Setup with VMware Kiwi VM (recommended)
1. **Download VMware VM files.**  
   Folder: https://drive.google.com/drive/folders/1PZfErGdxHmsBYaNRyp2jDGt1h26TjZt8

2. **Download the Kiwi VM image.**  
   Image: https://drive.google.com/file/d/1Fi1dLgQ8b090MdA2hVhgwKR4aMQtkSxl/view?usp=drive_link

3. **Import the Kiwi VM into VMware.**  
   Open VMware → *File* → *Open…* → select the downloaded Kiwi image → finish the import.

4. **Credentials.**  
   - For package installation: user **root**, password **myy601**.  
   - For normal usage: user **myy601**, password **myy601**.

5. **Transfer the project files into the VM.** Choose one of the following:
   - **Shared folder**: enable VMware shared folders and copy the repository contents into the VM.  
   - **SCP** (from host to VM):  
     ```bash
     scp -r /path/to/your/repo/ myy601@<vm_ip>:/home/myy601/
     ```

6. **Replace the engine files.**  
   Copy the relevant source files from your repository into the VM’s `kiwi-source/` tree, **replacing** the original files at the appropriate paths (as instructed by the assignment). Keep the directory structure intact.

7. **Install build tools (if missing).**
   ```bash
   # login as root for package installation
   apt update && apt install -y build-essential make gcc g++ git
   ```

8. **Compile Kiwi and the benchmark.**
   ```bash
   # switch to the working user if needed
   su - myy601
   cd ~/kiwi-source
   make clean && make all -j"$(nproc)"
   ```

9. **Verify artifacts.**
   ```bash
   ls -l ./bin || ls -l .
   ```

### Option B — Native Linux (no VM)
- Requirements: GCC/Clang, GNU Make, Linux with pthreads.  
- Clone the repo and integrate your changes into `kiwi-source/`, then:
  ```bash
  cd kiwi-source
  make clean && make all -j"$(nproc)"
  ```

## Usage
### CLI
```
./kiwi-bench <mode> <operationCount> [threadCount] [isRandom] [readPercentage]
# mode:          read | write | readwrite
# operationCount: positive integer
# threadCount:   >= 1 (>= 2 for readwrite)
# isRandom:      1=random access, 0=sequential
# readPercentage: 1..99 (only for readwrite; default 50)
```
**Examples**
```bash
# 10k reads with 5 threads, random access
./kiwi-bench read 10000 5 1

# 5k writes, single thread, sequential
./kiwi-bench write 5000

# 20k mixed ops, 4 threads, sequential access, 30% reads / 70% writes
./kiwi-bench readwrite 20000 4 0 30
```

## Benchmarking methodology
### Workload partitioning
- Total operations are split **as evenly as possible** among threads.
- For `readwrite`, the read/write split derives from `readPercentage` with clamp/correction to avoid zero threads for a role.
- Early threads may receive +1 operation when a remainder exists (deterministic fairness).

### Timing
- **Global timer:** wall‑time for the whole benchmark from thread creation to join.
- **Operation timers:** one per role (READ / WRITE), started by the first thread in the group and stopped by the last.
- **Thread timers:** one per thread for its assigned slice.
- All timers use `clock_gettime(CLOCK_MONOTONIC)` and compute nanosecond‑accurate deltas.

### Metrics
- **ops/sec:** total operations divided by operation‑group time.
- **sec/op:** operation‑group time divided by total ops.
- **sec/thread:** average per thread in the group.
- **keysFound (reads):** count of successful lookups as a basic correctness check.

## Concurrency design
### RWLocker API (simplified)
```c
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  can_read;
    pthread_cond_t  can_write;
    int readers;
    int writer;          // 0/1
    int waiting_writers; // optional, to control writer preference
} rwlocker_t;

void rwlocker_init(rwlocker_t* l);
void rwlocker_rlock(rwlocker_t* l);
void rwlocker_runlock(rwlocker_t* l);
void rwlocker_wlock(rwlocker_t* l);
void rwlocker_wunlock(rwlocker_t* l);
```
**Policy:** multiple readers or one writer. Writers gain exclusivity; implementation may use writer‑preference or balanced policy. Integrate locks around critical sections that touch **memtable** and **SST** state. Keep lock scope minimal to maximize parallelism while avoiding deadlocks.

### Lock granularity
- **Memtable path:** protect inserts/lookups.
- **SST path:** protect on‑disk table mutations and compaction side effects.
- Respect the engine’s background **merge/compaction** thread. Validate that your lock ordering does not conflict with its synchronization.

### Error handling
- Centralized error codes and messages. Fail fast on invalid CLI combinations or negative counts.

## Output
The benchmark prints:
- Effective mode, ops, threads, randomness, and read%.
- Global wall time.
- Per‑operation stats: threads, total ops, **ops/sec**, **sec/op**, **sec/thread**, and **keysFound** on READ.

## Reproducing experiments
Suggested scenarios:
1) **Write‑only**: `./kiwi-bench write 5000`  
2) **Read‑only**: `./kiwi-bench read 10000 5 1`  
3) **Mixed**: 70/30, 50/50, 30/70 with `readwrite` and 4–8 threads.

Automate with `scripts/run_scenarios.sh` and export CSV for plotting.

## Troubleshooting
- **Invalid args**: ensure `readwrite` uses ≥2 threads and `1..99` read%.
- **Environment quirks**: VM CPU/RAM settings can influence scheduling and stability. Prefer consistent cores/RAM across runs.
- **Segfaults under high cores/RAM**: review lock coverage and data lifetimes; verify compaction interactions and that shared buffers are protected.

## Development tips
- Use `gdb` with breakpoints on `db_add`, `db_get`, and lock calls.
- Log thread start/stop events and operation IDs for race diagnosis.
- Keep unit tests for RWLocker behavior (no starvation, no deadlock).

## Roadmap
- Finer lock splitting between memtable and SST tiers.
- Lock‑free read paths where safe.
- Continuous benchmarking and result plots.

## License
TBD (project/author’s choice).

## Acknowledgments
This project is based on the Kiwi storage engine used in the Operating Systems lab coursework.
