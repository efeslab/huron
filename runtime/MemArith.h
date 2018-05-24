#ifndef MEMARITH_H
#define MEMARITH_H

typedef unsigned long cacheline_id_t;
typedef unsigned long uintptr_t;

/* Cache line is 64 (2^6) bytes. */
const unsigned long cacheline_size_power = 6;

/* Word size is 4 bytes. */
const unsigned long word_size_power = 2, word_size = 4;

cacheline_id_t get_cache_line_id(void *addr) {
    return (cacheline_id_t)(addr) >> cacheline_size_power;
}

bool is_word_aligned(uintptr_t addr) {
    return addr % word_size == 0;
}

uintptr_t round_up_size(size_t size) {
    return is_word_aligned(size) 
        ? size 
        : ((size >> word_size_power) + 1) * word_size;
}

// WordInfo *get_word_info(uintptr_t start, size_t total_words, uintptr_t addr) {
//     size_t nth_word = (addr - start) >> word_size_power;
//     return (WordInfo*)(start + total_words * word_size) + nth_word;
// }

bool addr_is_in_range(uintptr_t addr, uintptr_t start, size_t total_words) {
    return addr >= start && addr < start + (total_words << word_size_power);
}

#endif