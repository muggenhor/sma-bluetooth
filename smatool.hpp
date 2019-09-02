#pragma once
#include "sma_struct.hpp"
#include <string>

extern unsigned char* ReadStream(const ConfType* conf, const FlagType* flag, ReadRecordType* readRecord, int s,
                                 unsigned char* stream, int* streamlen, int* datalen, const unsigned char* last_sent,
                                 int cc, int* terminated, int* togo);
const char* return_sma_description(int index);
extern long ConvertStreamtoLong( unsigned char * stream, int length, unsigned long long * value );
extern float ConvertStreamtoFloat( unsigned char *, int, float * );
std::string ConvertStreamtoString( unsigned char *, int );
extern unsigned char conv(const char* nn);
extern int empty_read_bluetooth(const FlagType* flag, ReadRecordType* readRecord, int s, int* rr,
                                unsigned char* received, int* terminated);
extern int read_bluetooth(const ConfType* conf, const FlagType* flag, ReadRecordType* readRecord, int s, int* rr,
                          unsigned char* received, int cc, const unsigned char* last_sent, int* terminated);
int select_str(const char* s);
std::string debugdate();
void add_escapes(unsigned char* cp, int* len);
extern void fix_length_send(const FlagType* flag, unsigned char* cp, int* len);
extern void tryfcs16(const FlagType* flag, const unsigned char* cp, int len, unsigned char* fl, int* cc);
extern int ConvertStreamtoInt(const unsigned char* stream, int length, int* value);
extern time_t ConvertStreamtoTime(const unsigned char* stream, int length, time_t* value, int* day, int* month,
                                  int* year, int* hour, int* minute, int* second);
extern int check_send_error(const FlagType* flag, int s, int* rr, unsigned char* received, int cc,
                            const unsigned char* last_sent, int* terminated, int* already_read);
