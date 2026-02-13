#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.


using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn)
{
  return isn + static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
inline uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // 修正 2: 正确计算 offset (确保是 uint32_t，避免符号扩展)
    // 这里利用 uint32_t 的自然溢出计算 n 到 isn 的距离
    uint32_t offset = n.raw_value() - isn.raw_value();

    // 修正 3: 优化算法，移除循环
    // 步骤 A: 构造一个基础候选值
    // 取 checkpoint 的高 32 位，拼上计算出的 offset
    uint64_t t = (checkpoint & 0xFFFFFFFF00000000) + offset;

    // 步骤 B: 调整 t 使其最接近 checkpoint
    // 距离超过一半范围 (2^31)，说明我们猜错“圈”了
    
    // 情况 1: t 比 checkpoint 大太多，说明应该在前一圈
    // 注意：要确保 t >= 2^32 才能减，防止下溢（虽然 0xFFFFFFFF00000000 保证了这点，除非 checkpoint 很小）
    if (t > checkpoint && (t - checkpoint) > (1ULL << 31)) {
        if (t >= (1ULL << 32)) { 
            t -= (1ULL << 32);
        }
    }
    // 情况 2: t 比 checkpoint 小太多，说明应该在后一圈
    else if (t < checkpoint && (checkpoint - t) > (1ULL << 31)) {
        t += (1ULL << 32);
    }

    return t;
}
