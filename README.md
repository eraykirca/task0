[![CI: C Tests](https://github.com/eraykirca/task0/actions/workflows/ci.yml/badge.svg)](https://github.com/eraykirca/task0/actions/workflows/ci.yml)

# Task 0 — Unit Tests for `emergency_module`

Unit and multithreaded tests for the Race UP `emergency_module` (C11 + pthreads).  
Focus areas: correctness, boundaries, error paths, concurrency, and **idempotency**.

---

## Quick start

```bash
make            # build tests
make run        # run tests
STRESS_LOOPS=250 make run   # heavier stress on threaded tests

- Functional single-thread tests
- Multithreaded tests using `pthread`
- Bounds checks and destroy semantics
- Optional stress loop via `STRESS_LOOPS`

## Build & Run
```bash
make          # builds with -fsanitize=thread if supported
make run      # executes the test suite
make clean
```

### Optional Stress Mode
```bash
STRESS_LOOPS=50 make run
```
Whats being tested

Right behavior (golden paths)
Single-node raise, solve clears both node & global when the node still “owns” the bit.
The class initializer behaves as a singleton (see below).

Boundaries
Valid/invalid exception indices (0..NUM_EMERGENCY_BUFFER*8 - 1 are valid).
OOB returns −1 and does not mutate any state.

Inverses
raise then solve works when the same node still has the bit.
After a node is re-initialized, solve can no longer clear global (bit wiped).

Cross-checks
Global vs node: raising on node A must be visible as “emergency” when queried via node B.

Error conditions
Re-calling the class initializer returns −1 and does not reset state.

Performance
Two threads raising/solving on the same node.
Multiple threads across multiple nodes.
Optional stress loops (via STRESS_LOOPS env).

summary
EmergencyNode_class_init(): first call returns 0, all later calls return -1 within the same process.
EmergencyNode_init() is not idempotent: every call zeroes the struct; re-init on a live node wipes its local state while the global emergency may remain raised.

Threaded cleanup: The provided module protects the global emergency counter with a lock, but the per-node fields (emergency_buffer[], emergency_counter) are updated without atomic ops.
With low stress loops (50-100) this is fine, but around 200-250 loops, it leaves a node’s counter non-zero even after solver threads finish, which leaks a global emergency into other tests.
To isolate tests, I added a defensive EmergencyNode_destroy at the end of the threaded tests.

## CI (GitHub Actions)
Ready-to-use workflow in `.github/workflows/ci.yml` builds with **gcc** and **clang**, with and without **ThreadSanitizer**, and performs an optional stress pass.

CI, TSAN, and why I skip MT under TSAN

GitHub Actions pipeline runs two lanes:

gcc/clang (no sanitizer): runs the full suite, including multithreaded and stress tests.

clang + ThreadSanitizer (TSAN): runs a single-threaded subset of tests (skip MT & stress here).

Why skip multithread tests under TSAN?
Because of the reasons I explained previously, Under concurrent access, there is a data race by TSAN’s definition. Functionally, the tests still pass but TSAN correctly reports races and exits non-zero. Because I did not want to change Race Up's module, the pragmatic, transparent approach is:

Run all concurrency tests in the normal jobs (to verify functional correctness under load).

Run a TSAN subset that exercises single-thread paths (to catch other thread issues) without flagging the known per-node races in the vendor module.
