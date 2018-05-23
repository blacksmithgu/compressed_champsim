#ifndef ___COUNTER_H___
#define ___COUNTER_H___

/**
 * A saturating-counter with a compile-time max value.
 */
template<uint32_t MAX_VALUE> class Counter {
private:
    // The current counter value.
    uint32_t value_;

public:
    Counter() : value_(0) {}
    Counter(uint32_t initial_value) : value_(std::min(initial_value, MAX_VALUE)) {}

    // Obtains the counter's current value.
    uint32_t value() { return value_; }

    // Attempts to increment the counter's value, saturating at MAX_VALUE.
    void increment() {
        if(value_ >= MAX_VALUE) return;
        value_++;
    }

    // Attempts to decrement the counter's value, saturating at 0.
    void decrement() {
        if(value_ == 0) return;
        value_--;
    }
};

#endif
