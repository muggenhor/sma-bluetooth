#pragma once
#include "sma_struct.h"

extern unsigned char * ReadStream( ConfType *, FlagType *, ReadRecordType *, int *, unsigned char *, int *, unsigned char *, int *, unsigned char *, int , int *, int * );
extern char * return_xml_data( ConfType *,int );
extern long ConvertStreamtoLong( unsigned char * stream, int length, unsigned long long * value );
extern float ConvertStreamtoFloat( unsigned char *, int, float * );
extern char * ConvertStreamtoString( unsigned char *, int );
extern unsigned char conv( char * );
