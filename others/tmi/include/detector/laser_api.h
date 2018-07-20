#pragma once
#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

class LaserEvent
{
public:
  uint64_t PC;
  uint64_t Addr;
}__attribute__((packed));
