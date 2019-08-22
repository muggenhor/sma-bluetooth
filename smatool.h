#pragma once
#include "sma_struct.h"

extern unsigned char * ReadStream( ConfType *, FlagType *, ReadRecordType *, int *, unsigned char *, int *, int *, unsigned char *, int , int *, int * );
extern char * return_xml_data( ConfType *,int );
extern long ConvertStreamtoLong( unsigned char * stream, int length, unsigned long long * value );
extern float ConvertStreamtoFloat( unsigned char *, int, float * );
extern char * ConvertStreamtoString( unsigned char *, int );
extern unsigned char conv( char * );
int empty_read_bluetooth(FlagType* flag, ReadRecordType* readRecord, int* s, int* rr, unsigned char* received,
                         int* terminated);
int read_bluetooth(ConfType* conf, FlagType* flag, ReadRecordType* readRecord,
                   int* s, int* rr, unsigned char* received, int cc,
                   unsigned char* last_sent, int* terminated);
int select_str(char* s);
extern char* debugdate();
void add_escapes(unsigned char* cp, int* len);
void fix_length_send(FlagType* flag, unsigned char* cp, int* len);
void tryfcs16(FlagType* flag, unsigned char* cp, int len, unsigned char* fl,
              int* cc);
int ConvertStreamtoInt(unsigned char* stream, int length, int* value);
time_t ConvertStreamtoTime(unsigned char* stream, int length, time_t* value,
                           int* day, int* month, int* year, int* hour,
                           int* minute, int* second);
