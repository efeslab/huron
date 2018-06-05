#ifndef MEMARITH_H
#define MEMARITH_H

typedef unsigned long uintptr_t;

/* Cache line is 64 (2^6) bytes. */
const unsigned long cacheline_size_power = 6;

/* Word size is 4 bytes. */
const unsigned long word_size_power = 2, word_size = 4;

bool is_aligned_by(uintptr_t addr, size_t alignment) {
    return addr % alignment == 0;
}

uintptr_t round_up_size(size_t size, size_t alignment_power) {
    return is_aligned_by(size, 1ul << alignment_power)
           ? size
           : ((size >> alignment_power) + 1) << alignment_power;
}

// WordInfo *get_word_info(uintptr_t start, size_t total_words, uintptr_t addr) {
//     size_t nth_word = (addr - start) >> word_size_power;
//     return (WordInfo*)(start + total_words * word_size) + nth_word;
// }

bool addr_is_in_range(uintptr_t addr, uintptr_t start, size_t total_words) {
    return addr >= start && addr < start + (total_words << word_size_power);
}

#endif