#pragma once
#include "sma_struct.hpp"
#include <string>
#include <vector>

std::vector<unsigned char> ReadStream(const ConfType* conf, const FlagType* flag, ReadRecordType* readRecord, int s,
                                 unsigned char* stream, int* streamlen, const std::vector<unsigned char>& last_sent,
                                 int* terminated, int* togo);
const char* return_sma_description(int index);
extern float ConvertStreamtoFloat(const unsigned char *, int);
std::string ConvertStreamtoString(const unsigned char *, int);
extern unsigned char conv(const char* nn);
extern int empty_read_bluetooth(const FlagType* flag, ReadRecordType* readRecord, int s, int* rr,
                                unsigned char* received, int* terminated);
extern int read_bluetooth(const ConfType* conf, const FlagType* flag, ReadRecordType* readRecord, int s, int* rr,
                          unsigned char* received, const std::vector<unsigned char>& last_sent, int* terminated);
int select_str(const char* s);
std::string debugdate();
void add_escapes(unsigned char* cp, int* len);
extern void fix_length_send(const FlagType* flag, unsigned char* cp, int* len);
extern void tryfcs16(const FlagType* flag, const unsigned char* cp, int len, unsigned char* fl, int* cc);
extern int ConvertStreamtoInt(const unsigned char* stream, int length);
extern time_t ConvertStreamtoTime(const unsigned char* stream, int length, int* day, int* month,
                                  int* year, int* hour, int* minute, int* second);
extern int check_send_error(const FlagType* flag, int s, int* rr, unsigned char* received, int cc,
                            const std::vector<unsigned char>& last_sent, int* terminated, int* already_read);
