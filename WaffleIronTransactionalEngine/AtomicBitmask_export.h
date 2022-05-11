#pragma once

namespace WITE {

  /*
   * each bit in the mask is atomic. Adjacent bits do not interfere with each others' values, though may wait each other.
   * used internally for dirty checks on thread-specific resources (each thread sets its own flag, main runs getAndClear)
   */
  class AtomicBitmask {
  public:
    typedef inner_t uint_fast8_t;//prefer smaller to reduce the chance of locking nearby bits in the same atomic
    typedef atomic_t std::atomic<inner_t>;
    constexpr static int BYTES_PER_ATOMIC = sizeof(inner_t);
    constexpr static int BITS_PER_ATOMIC = BYTES_PER_ATOMIC * 8;
    constexpr static inner_t xmas = ~static_cast<inner_t>(0);
    constexpr static size_t getIntCount(size_t bitCount) { return (bitCount - 1) / BITS_PER_ATOMIC + 1; };
    AtomicBitmask(size_t bitCount) : bitCount(bitCount), byteCount(getIntCount(bitCount)), data(std::make_unique(byteCount)) {};
    bool getBit(size_t idx) { return data[getByte(idx)] & getBit(idx); };
    void setBit(size_t idx) { data[getByte(idx)].fetch_or(getBit(idx)); };
    void clearBit(size_t idx) { data[getByte(idx)].fetch_and(xmas ^ getBit(idx)); };
    inline void setBit(size_t idx, bool value) { if(value) setBit(idx) else clearBit(idx) };
    void clearAll() { for(size_t i = 0;i < atomicCount;i++) data[i] = 0; };
    size_t getIntCount() { return atomicCount; };
    void snapshot(inner_t* out) { for(size_t i = 0;i < atomicCount;i++) out[i] = data[i]; }:
    std::unique_ptr<inner_t> snapshot() { auto ret = std::make_unique(atomicCount); snapshot(ret.get()); return ret; };
    void getAndClear(inner_t* out) { for(size_t i = 0;i < atomicCount;i++) out[i] = data[i].exchange(0); }:
    std::unique_ptr<inner_t> getAndClear() { auto ret = std::make_unique(atomicCount); getAndClear(ret.get()); return ret; };
  private:
    size_t bitCount, atomicCount;
    std::unique_ptr<atomic_t[]> data;
    inline size_t getByte(size_t idx) { return idx / BITS_PER_ATOMIC; };
    inline uint_fast8_t getBit(size_t idx) { return 1 << (idx % BITS_PER_ATOMIC); };
  }

}
