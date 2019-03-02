#include "../replacement/size_aware_optgen.h"
#include <array>
#include <assert.h>
#include <iostream>

struct Interval {
    uint64_t start_interval;
    uint64_t end_interval;
    uint64_t address;

    constexpr Interval(uint64_t start, uint64_t end, uint64_t address) : start_interval(start), end_interval(end), address(address) { }
};

// example from https://www.cs.utexas.edu/~lin/papers/isca16.pdf
constexpr std::array<Interval, 6> reuse_access_stream {
    Interval(1, 2, 'B'),
    Interval(0, 6, 'A'),
    Interval(4, 8, 'D'),
    Interval(5, 9, 'E'),
    Interval(7, 10, 'F'),
    Interval(3, 11, 'C'),
};

int main() {
    OPTgen<1024> test_optgen(2);
    int hits = 0;
    for (int i = 0; i < reuse_access_stream.size(); i++) {
        const auto& current_interval = reuse_access_stream[i];
        if (test_optgen.try_cache(current_interval.start_interval, current_interval.end_interval, current_interval.address, 1)) {
            hits++;
        }
    }
    std::cout << "Hits are " << hits << std::endl;
    assert(hits == 4);
}
