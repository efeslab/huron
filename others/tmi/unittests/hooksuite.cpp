#include <gtest/gtest.h>

#include <hooks.hpp>
#include <pthread.h>

TEST(HookSuite, Init){
  init_orig_functions();
}

void* testJoin(void *arg){
  return nullptr;
}

TEST(HookSuite, CreateJoin){
  ASSERT_NE(orig_pthread_create,nullptr);
  ASSERT_NE(orig_pthread_join,nullptr);

  pthread_t thread;
  orig_pthread_create(&thread,nullptr,testJoin,(void*)10);
  orig_pthread_join(thread,nullptr);
}
