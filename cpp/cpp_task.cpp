#include <chrono>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>
#include <iostream>

using namespace std::chrono_literals;

void StartThread(
    std::thread& thread,
    std::atomic<bool>& running,
    const std::function<bool(void)>& Process,
    const std::chrono::seconds timeout)
{
    // Process and timeout are captured by value: Process is bound to a
    // temporary at the call site and timeout is a by-value parameter, so
    // both would dangle once StartThread returns if captured by reference.
    thread = std::thread(
        [&running, Process, timeout] ()
        {
            // steady_clock is monotonic; high_resolution_clock isn't
            // guaranteed to be (it can alias system_clock and jump backward).
            auto start = std::chrono::steady_clock::now();
            while(running)
            {
                bool aborted = Process();

                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                if (aborted || duration > timeout)
                {
                    running = false;
                    break;
                }
            }
        });
}

int main(int argc, char **argv)
{
    // Each thread needs its own running flag: sharing one atomic<bool>
    // means whichever thread stops first (by its own timeout or abort)
    // also kills the other thread's loop, even though the two loops
    // have independent timeouts and stop conditions.
    //
    // Not a bug, just a thought: these four variables likely end up on the
    // same cache line, and each thread pounds its own half concurrently --
    // that's false sharing. Not worth fixing here since Process() sleeps
    // 1-2s per iteration, so a few nanoseconds of cache traffic is noise.
    // If this ever became a tight, sleep-free loop, this is how you'd fix it:
    //
    //   struct alignas(64) ThreadState
    //   {
    //       std::atomic<bool> running{true};
    //       int loopCounter = 0;
    //   };
    //   ThreadState state1, state2; // each gets its own cache line
    std::atomic<bool> my_running1 = true;
    std::atomic<bool> my_running2 = true;
    std::thread my_thread1, my_thread2;
    int loop_counter1 = 0, loop_counter2 = 0;

    // start actions in seprate threads and wait of them

    StartThread(
        my_thread1,
        my_running1,
        [&]()
        {
            // "some actions" simulated with waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            loop_counter1++;
            return false;
        },
        10s); // loop timeout

    StartThread(
        my_thread2,
        my_running2,
        [&]()
        {
            // "some actions" simulated with waiting
            if (loop_counter2 < 5)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                loop_counter2++;
                return false;
            }
            return true;
        },
        10s); // loop timeout


    my_thread1.join();
    my_thread2.join();

    // print execlution loop counters
    std::cout << "C1: " << loop_counter1 << " C2: " << loop_counter2 << std::endl;
}
