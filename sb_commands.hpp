#pragma once
#include "sma_struct.hpp"
#include <stdio.h>

extern int ConnectSocket(const ConfType* conf);
extern int OpenInverter(ConfType* conf, const FlagType* flag, UnitType** unit, const int s, ArchDataType** archdatalist,
                        int* archdatalen, LiveDataType** livedatalist, int* livedatalen);
extern void InverterCommand(const char* command, ConfType* conf, const FlagType* flag, UnitType** unit, const int s,
                            FILE* fp, ArchDataType** archdatalist, int* archdatalen, LiveDataType** livedatalist,
                            int* livedatalen);
