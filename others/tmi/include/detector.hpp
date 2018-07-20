#include "perf.hpp"

struct Mem_access_type_size
{
  bool isStore;
  int size;
};

extern "C"{
  void detector_init(int window_size, int sample_period);
  void detector_handle_record(DataRecord *dr);
  void detector_check_false_sharing();
  void detector_do_output();
}
