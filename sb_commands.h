#pragma once
#include "sma_struct.h"
#include <stdio.h>

int ConnectSocket(ConfType* conf);
int OpenInverter(ConfType* conf, FlagType* flag, UnitType** unit, int* s,
                 ArchDataType** archdatalist, int* archdatalen,
                 LiveDataType** livedatalist, int* livedatalen);
char* InverterCommand(const char* command, ConfType* conf, FlagType* flag,
                      UnitType** unit, int* s, FILE* fp,
                      ArchDataType** archdatalist, int* archdatalen,
                      LiveDataType** livedatalist, int* livedatalen);
