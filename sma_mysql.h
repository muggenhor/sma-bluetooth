#pragma once

#include <mysql/mysql.h>
#include "sma_struct.h"

extern MYSQL *conn;
extern MYSQL_RES *res;
extern MYSQL_RES *res1;
extern MYSQL_RES *res2;

extern void OpenMySqlDatabase(const char* server, const char* user, const char* password, const char* database);
extern void CloseMySqlDatabase();
extern int DoQuery(const char* query);
extern int DoQuery1(const char* query);
extern int DoQuery2(const char* query);
extern int install_mysql_tables(const ConfType* conf, const FlagType* flag, const char* SCHEMA);
extern void update_mysql_tables(const ConfType* conf, const FlagType* flag);
extern int check_schema(const ConfType* conf, const FlagType* flag, const char* SCHEMA);
extern void live_mysql(const ConfType* conf, const FlagType* flag, const LiveDataType* livedatalist, int livedatalen);
