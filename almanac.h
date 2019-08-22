#pragma once
#include "sma_struct.h"

char* sunrise(ConfType* conf, int debug);
char* sunset(ConfType* conf, int debug);
int todays_almanac(ConfType* conf, int debug);
void update_almanac(ConfType* conf, char* sunrise, char* sunset, int debug);
