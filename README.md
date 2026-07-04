# QuantumSystemsAssignment

Quantum-Systems Assignment — solutions to the three tasks from `input/QS_Linux_Tasks.pptx`:

1. Review and fix `cpp_task.cpp` (bugs in multithreaded code).
2. Fix the 90° matrix rotation in `python_task.py`.
3. Design and implement a C++ class for sending UDP messages (immediately / delayed / periodically).

## Repository structure

```
input/            original assignment files (left unchanged)
  QS_Linux_Tasks.pptx
  cpp_task.cpp
  python_task.py

cpp/
  cpp_task.cpp    fixed version of task 1
  udp/            task 3 — UDP messenger
    IUdpMessenger.h    pure virtual interface (SendNow/SendDelayed/SendPeriodic/Cancel)
    UdpMessenger.h/.cpp concrete implementation of the interface
    demo.cpp           self-verifying demo with a local UDP listener

python/
  python_task.py  fixed version of task 2
```

## Task 1 — cpp/cpp_task.cpp

Fixed bugs: a shared `running` flag across two independent threads, a dangling reference to `Process`/`timeout` caused by `[&]` capture, and a non-monotonic `high_resolution_clock`. Details are in the code comments and in PR [#2](../../pull/2).

Build and run:

```bash
g++ -std=c++17 -pthread -Wall -Wextra -O2 cpp/cpp_task.cpp -o /tmp/cpp_task
/tmp/cpp_task
```

Expected output: `C1: 5 C2: 5`.

## Task 2 — python/python_task.py

Fixed the matrix rotation logic (transpose + reverse each row, instead of reading and writing the same array in place). Details are in PR [#3](../../pull/3).

Run:

```bash
python3 python/python_task.py
```

Expected output: `Testcase OK!` twice.

## Task 3 — cpp/udp/

`IUdpMessenger` is the interface, `UdpMessenger` is the implementation, built on a single UDP socket and one background scheduler thread (no thread spawned per call). Design details are in PR [#4](../../pull/4).

Build and run the demo (it starts its own UDP listener on loopback and exercises all three send modes plus cancellation):

```bash
g++ -std=c++17 -pthread -Wall -Wextra -O2 cpp/udp/UdpMessenger.cpp cpp/udp/demo.cpp -o /tmp/udp_demo
/tmp/udp_demo
```

Usage in code:

```cpp
#include "UdpMessenger.h"

UdpMessenger messenger;
messenger.SendNow("127.0.0.1", 9123, "hello");

auto delayedId = messenger.SendDelayed("127.0.0.1", 9123, "hello", 5);   // once, after 5s
auto periodicId = messenger.SendPeriodic("127.0.0.1", 9123, "hello", 2); // every 2s

messenger.Cancel(periodicId); // stop the periodic send
```

`delaySeconds`/`periodSeconds` accept values `1–255`; `0` throws `std::invalid_argument`.
