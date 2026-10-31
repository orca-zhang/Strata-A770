#include <orca_l0/ring.hpp>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

static void require(bool value) {
    if (!value) throw std::runtime_error("ring check failed");
}

int main() {
    orca::l0::Ring<uint64_t, 2> small;
    uint64_t value = 99;
    require(!small.try_pop(value) && value == 99);
    require(small.try_push(1) && small.try_push(2) && !small.try_push(3));
    require(small.try_pop(value) && value == 1);
    require(small.try_push(3) && !small.try_push(4));
    require(small.try_pop(value) && value == 2);
    require(small.try_pop(value) && value == 3 && !small.try_pop(value));

    // Two rings are the request/completion channels; the returned sequence releases the slot to its owner.
    struct Item { uint64_t sequence = 0, checksum = 0; };
    orca::l0::Ring<Item, 64> requests, completions;
    constexpr uint64_t count = 200000;
    std::atomic<bool> valid{true};
    std::thread consumer([&] {
        for (uint64_t i = 0; i < count; ++i) {
            Item item;
            while (!requests.try_pop(item)) std::this_thread::yield();
            if (item.sequence != i || item.checksum != (i ^ 0xa5a5a5a5ULL)) valid = false;
            while (!completions.try_push(item)) std::this_thread::yield();
        }
    });
    uint64_t sent = 0, received = 0;
    while (received != count) {
        if (sent < count && requests.try_push({sent, sent ^ 0xa5a5a5a5ULL})) ++sent;
        Item item;
        if (completions.try_pop(item)) {
            if (item.sequence != received) valid = false;
            ++received;
        }
    }
    consumer.join();
    require(valid.load());
    std::cout << "PASS empty/full/backpressure, wraparound, 200000 ordered request/completion handoffs\n";
}
