#pragma once
#include "sma_struct.hpp"
#include <stdio.h>
#include <vector>

extern int ConnectSocket(const ConfType* conf);
extern int OpenInverter(ConfType* conf, const FlagType* flag, UnitType** unit, const int s, std::vector<ArchDataType>& archdata,
                        std::vector<LiveDataType>& livedata);
extern void InverterCommand(const char* command, ConfType* conf, const FlagType* flag, UnitType** unit, const int s,
                            FILE* fp, std::vector<ArchDataType>& archdata, std::vector<LiveDataType>& livedata);
