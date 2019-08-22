#pragma once

#include <mysql/mysql.h>
#include "sma_struct.hpp"

extern MYSQL* OpenMySqlDatabase(const char* server, const char* user, const char* password, const char* database);
extern MYSQL_RES* DoQuery(MYSQL* conn, const char* query);
extern int install_mysql_tables(const ConfType* conf, const FlagType* flag, const char* SCHEMA);
extern void update_mysql_tables(const ConfType* conf, const FlagType* flag);
extern int check_schema(const ConfType* conf, const FlagType* flag, const char* SCHEMA);
extern void live_mysql(const ConfType* conf, const FlagType* flag, const LiveDataType* livedatalist, int livedatalen);
