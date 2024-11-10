# xv6

## Page Fault frequency

**Lazyfork**: During the execution of lazyfork the number of page faults lies around 56,000.

**Reading the Memory**: During the execution of readtest.c which checks the number of page faults during reading, the number of page faults turns out to be 0, as CoW modifies the page only during a write and not during a read.

**Writing the Memory**: During the execution of writetest.c which checks the number of page faults during writing, the number of page faults turns out to be 1 (It is how i implemented it).

## Memory Conservation:
Instead of duplicating the entire address space immediately, COW allows the parent and child processes to share memory pages until either one modifies a page. This conserves memory because only the modified pages are duplicated, reducing the overall memory usage in scenarios where few pages are changed by the child after forking.

## Efficiency
COW improves efficiency in fork() by deferring the duplication of memory pages until they are modified. This approach reduces the immediate workload of fork() since memory isn’t copied until necessary, allowing fork() to execute more quickly, especially in cases where the child process quickly calls exec(). By avoiding unnecessary duplication, COW also minimizes costly page faults and reduces delays associated with large memory copying operations. This makes fork() with COW particularly time-efficient for processes that make limited changes to their address space after forking, resulting in faster process creation and lower latency.

# Potential Optimizations for COW
**Optimizing Page Duplication Frequency:** For pages that are frequently modified by both the parent and child, adaptive strategies could be used to detect such patterns and duplicate these pages preemptively, reducing the performance overhead of repeated COW faults.

**Minimizing Lock Contention:** To manage shared pages, COW implementations often use locks, which can create contention, especially in multi-threaded environments. Employing finer-grained locking or lock-free mechanisms for managing page tables can enhance performance.