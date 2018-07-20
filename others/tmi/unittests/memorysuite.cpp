#include <gtest/gtest.h>

#include <tmimem.hpp>

TEST(MemorySuite, Init){
  memory::init();
}

TEST(MemorySuite, Alloc){
  void *p = memory::adjust_slab_memory(128);
  ASSERT_NE(p,nullptr);
  
  void *m = memory::get_mmap_memory(4096);
  ASSERT_NE(m,nullptr);
}

TEST(MemorySuite, Protect){
  void *p1 = memory::adjust_slab_memory(4096);
  void *p2 = memory::adjust_slab_memory(4096);
  void *p3 = memory::adjust_slab_memory(512);

  ASSERT_TRUE(memory::protectAddress(p1));
  ASSERT_TRUE(memory::protectAddress(p3));
}
