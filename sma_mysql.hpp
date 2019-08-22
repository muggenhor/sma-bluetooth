#pragma once

#include <mysql/mysql.h>
#include <utility>
#include "sma_struct.hpp"

class MySQLResult
{
public:
  ~MySQLResult();
  MySQLResult(MySQLResult&& rhs) noexcept
    : res(std::exchange(rhs.res, nullptr))
  {
  }
  MySQLResult& operator=(MySQLResult&& rhs) noexcept;

  explicit operator bool() noexcept { return !!res; }
  unsigned long long num_rows() const;
  MYSQL_ROW fetch_row();

private:
  explicit constexpr MySQLResult(MYSQL_RES* res) noexcept
    : res(res)
  {
  }

  friend class MySQL;
  MYSQL_RES* res;
};

class MySQL
{
public:
  MySQL(const char* server, const char* user, const char* password, const char* database);
  ~MySQL();
  MySQL(MySQL&& rhs) noexcept
    : conn(std::exchange(rhs.conn, nullptr))
  {
  }
  MySQL& operator=(MySQL&& rhs) noexcept;

  explicit operator bool() noexcept { return !!conn; }
  MySQLResult fetch_query(const char* query);
  void query(const char* query);

private:
  MYSQL* conn;
};

extern int install_mysql_tables(const ConfType* conf, const FlagType* flag, const char* SCHEMA);
extern void update_mysql_tables(const ConfType* conf, const FlagType* flag);
extern int check_schema(const ConfType* conf, const FlagType* flag, const char* SCHEMA);
extern void live_mysql(const ConfType* conf, const FlagType* flag, const LiveDataType* livedatalist, int livedatalen);
