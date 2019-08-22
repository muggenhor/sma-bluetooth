#pragma once
#include "sma_struct.hpp"
#include <string>

std::string sunrise(const ConfType* conf, int debug);
std::string sunset(const ConfType* conf, int debug);
extern int todays_almanac(const ConfType* conf, int debug);
extern void update_almanac(const ConfType* conf, const char* sunrise, const char* sunset, int debug);
