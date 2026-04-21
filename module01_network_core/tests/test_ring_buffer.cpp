#include <gtest/gtest.h>
#include "net/ring_buffer.h"

using namespace net;

TEST(RingBuffer, PushPop) {
    RingBuffer<int, 8> rb;
    EXPECT_TRUE(rb.push(42));
    EXPECT_FALSE(rb.empty());

    int val = 0;
    EXPECT_TRUE(rb.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, Full) {
    RingBuffer<int, 4> rb;
    // Capacity is N-1 = 3 (one slot kept empty to distinguish full from empty)
    EXPECT_TRUE(rb.push(1));
    EXPECT_TRUE(rb.push(2));
    EXPECT_TRUE(rb.push(3));
    EXPECT_TRUE(rb.full());
    EXPECT_FALSE(rb.push(4)); // Should fail: full

    // Pop one and push again
    int val = 0;
    EXPECT_TRUE(rb.pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_FALSE(rb.full());
    EXPECT_TRUE(rb.push(4));
}

TEST(RingBuffer, Empty) {
    RingBuffer<int, 8> rb;
    EXPECT_TRUE(rb.empty());

    int val = 0;
    EXPECT_FALSE(rb.pop(val)); // Should fail: empty

    rb.push(99);
    EXPECT_FALSE(rb.empty());

    rb.pop(val);
    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.pop(val)); // Still empty
}

TEST(RingBuffer, Wraparound) {
    // N=4, usable capacity=3. Push N+1=5 items to exercise wraparound.
    // We drain between pushes to allow wraparound of the index.
    RingBuffer<int, 4> rb;

    // Fill buffer
    EXPECT_TRUE(rb.push(1));
    EXPECT_TRUE(rb.push(2));
    EXPECT_TRUE(rb.push(3));
    EXPECT_TRUE(rb.full());

    // Drain all
    int val;
    EXPECT_TRUE(rb.pop(val)); EXPECT_EQ(val, 1);
    EXPECT_TRUE(rb.pop(val)); EXPECT_EQ(val, 2);
    EXPECT_TRUE(rb.pop(val)); EXPECT_EQ(val, 3);
    EXPECT_TRUE(rb.empty());

    // Now indices have advanced: head_=3, tail_=3 (mod 4)
    // Push two more items — they wrap around the array
    EXPECT_TRUE(rb.push(4));
    EXPECT_TRUE(rb.push(5));

    EXPECT_TRUE(rb.pop(val)); EXPECT_EQ(val, 4);
    EXPECT_TRUE(rb.pop(val)); EXPECT_EQ(val, 5);
    EXPECT_TRUE(rb.empty());
}
