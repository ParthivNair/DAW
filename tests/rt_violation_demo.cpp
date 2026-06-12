// rtsan negative-test demonstration.
//
// This target is built ONLY under the rtsan preset and is NOT part of the
// default ctest suite. Its whole purpose is to PROVE the rtsan lane actually
// catches a real-time violation: main() calls an RT_NONBLOCKING function that
// heap-allocates. Under -fsanitize=realtime that must abort with an RTSan
// diagnostic and a non-zero exit. If it ever exits 0, the rtsan lane is broken.
#include <cstdio>
#include <vector>

#include "rt/RTAnnotations.h"

// Marked nonblocking, but deliberately allocates — exactly the bug RTSan exists
// to catch. The volatile sink stops the optimiser from eliding the allocation.
static void pretendAudioCallback() RT_NONBLOCKING
{
    std::vector<int> buffer;     // heap allocation in a nonblocking context
    buffer.resize (1024, 7);     // ... and a growth, for good measure
    volatile int sink = buffer[512];
    (void) sink;
}

int main()
{
    std::puts ("rt_violation_demo: calling an RT_NONBLOCKING fn that allocates...");
    pretendAudioCallback();
    // Should be unreachable under rtsan (RTSan aborts above).
    std::puts ("rt_violation_demo: returned WITHOUT an RTSan diagnostic — lane is BROKEN");
    return 0;
}
