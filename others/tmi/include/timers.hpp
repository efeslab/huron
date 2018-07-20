#pragma once

#include <chrono>

typedef std::chrono::high_resolution_clock clock_type;

class timers{
  static unsigned long *_segvTime;
  static unsigned long *_commitTime;

  static clock_type::time_point _startSegv;
  static clock_type::time_point _startCommit;

 public:
  static const int SEGV = 1;
  static const int COMMIT = 2;

  static void initTimer(int name);
  static void startTimer(int name);
  static void stopTimer(int name);
  static void printTimers();
};
