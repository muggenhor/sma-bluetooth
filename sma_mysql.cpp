#include "sma_mysql.hpp"
#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sma_struct.hpp"
#include <time.h>
#include <stdexcept>
#include <fmt/printf.h>
#include "smatool.hpp"

MySQLResult::~MySQLResult()
{
  if (res)
    mysql_free_result(res);
}

MySQLResult& MySQLResult::operator=(MySQLResult&& rhs) noexcept
{
  if (res)
    mysql_free_result(res);
  res = std::exchange(rhs.res, nullptr);
  return *this;
}

unsigned long long MySQLResult::num_rows() const
{
  if (!res)
    return 0;
  return mysql_num_rows(res);
}

MYSQL_ROW MySQLResult::fetch_row()
{
  if (!res)
    return nullptr;
  return mysql_fetch_row(res);
}

MySQL::MySQL(const char* server, const char* user, const char* password, const char* database)
  : conn(mysql_init(nullptr))
{
   /* Connect to database */
   if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
     throw std::runtime_error(mysql_error(conn));
}

MySQL::~MySQL()
{
  if (conn)
    mysql_close(conn);
}

MySQL& MySQL::operator=(MySQL&& rhs) noexcept
{
  if (conn)
    mysql_close(conn);
  conn = std::exchange(rhs.conn, nullptr);
  return *this;
}

MySQLResult MySQL::fetch_query(const char* query)
{
  if (mysql_real_query(conn, query, strlen(query)))
    throw std::runtime_error(mysql_error(conn));
  return MySQLResult{mysql_store_result(conn)};
}

void MySQL::query(const char* query)
{
  if (mysql_real_query(conn, query, strlen(query)))
    throw std::runtime_error(mysql_error(conn));
}

int install_mysql_tables(const ConfType* conf, const FlagType* flag, const char* SCHEMA)
/*  Do initial mysql table creationsa */
{
    int	        found=0;
    std::string SQLQUERY;

    MySQL conn( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, "mysql");
    //Get Start of day value
    SQLQUERY = "SHOW DATABASES";
    if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);

    auto res = conn.fetch_query(SQLQUERY.c_str());
    for (auto row = res.fetch_row(); row; row = res.fetch_row())  //if there is a result, update the row
    {
       if( strcmp( row[0], conf->MySqlDatabase ) == 0 )
       {
          found=1;
          fmt::printf( "Database exists - exiting" );
       }
    }
    if( found == 0 )
    {
       SQLQUERY = fmt::sprintf("CREATE DATABASE IF NOT EXISTS %s", conf->MySqlDatabase);
       if (flag->debug == 1) fmt::printf("%s\n", SQLQUERY);
       conn.query(SQLQUERY.c_str());

       SQLQUERY = fmt::sprintf("USE  %s", conf->MySqlDatabase);
       if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
       conn.query(SQLQUERY.c_str());

       SQLQUERY = "CREATE TABLE `Almanac` ( `id` bigint(20) NOT NULL \
          AUTO_INCREMENT, \
          `date` date NOT NULL,\
          `sunrise` datetime DEFAULT NULL,\
          `sunset` datetime DEFAULT NULL,\
          `CHANGETIME` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
           PRIMARY KEY (`id`),\
           UNIQUE KEY `date` (`date`)\
           ) ENGINE=MyISAM";

       if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
       conn.query(SQLQUERY.c_str());
  
       SQLQUERY = "CREATE TABLE `DayData` ( \
           `DateTime` datetime NOT NULL, \
           `Inverter` varchar(30) NOT NULL, \
           `Serial` varchar(40) NOT NULL, \
           `CurrentPower` int(11) DEFAULT NULL, \
           `ETotalToday` DECIMAL(10,3) DEFAULT NULL, \
           `Voltage` DECIMAL(10,3) DEFAULT NULL, \
           `PVOutput` datetime DEFAULT NULL, \
           `CHANGETIME` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP, \
           PRIMARY KEY (`DateTime`,`Inverter`,`Serial`) \
           ) ENGINE=MyISAM";

       if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
       conn.query(SQLQUERY.c_str());

       SQLQUERY = "CREATE TABLE `LiveData` ( \
           `id` bigint(20) NOT NULL  AUTO_INCREMENT, \
           `DateTime` datetime NOT NULL, \
           `Inverter` varchar(30) NOT NULL, \
           `Serial` varchar(40) NOT NULL, \
           `Description` varchar(30) NOT NULL, \
           `Value` varchar(30) NOT NULL, \
           `Units` varchar(20) DEFAULT NULL, \
           `CHANGETIME` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP, \
           PRIMARY KEY (`id`), \
           UNIQUE KEY 'DateTime'(`DateTime`,`Inverter`,`Serial`,`Description`) \
           ) ENGINE=MyISAM";
       if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
       conn.query(SQLQUERY.c_str());

       SQLQUERY = "CREATE TABLE `settings` ( \
           `value` varchar(128) NOT NULL, \
           `data` varchar(500) NOT NULL, \
           PRIMARY KEY (`value`) \
           ) ENGINE=MyISAM";

       if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
       conn.query(SQLQUERY.c_str());
        
       
       SQLQUERY = fmt::sprintf("INSERT INTO `settings` SET `value` = \'schema\', `data` = \'%s\' ", SCHEMA);

       if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
       conn.query(SQLQUERY.c_str());
    }

    return found;
}

void update_mysql_tables(const ConfType* conf, const FlagType* flag)
/*  Do mysql table schema updates */
{
    int		schema_value=0;
    std::string SQLQUERY;

    MySQL conn( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, "mysql");
    SQLQUERY = fmt::sprintf("USE  %s", conf->MySqlDatabase);
    if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
    conn.query(SQLQUERY.c_str());
    /*Check current schema value*/
    SQLQUERY = fmt::sprintf("SELECT data FROM settings WHERE value=\'schema\' " );
    if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
    auto res = conn.fetch_query(SQLQUERY.c_str());
    if (auto row = res.fetch_row())  //if there is a result, update the row
    {
       schema_value=atoi(row[0]);
    }
    if( schema_value == 1 ) { //Upgrade from 1 to 2
        SQLQUERY = "ALTER TABLE `DayData` CHANGE `ETotalToday` `ETotalToday` DECIMAL(10,3) NULL DEFAULT NULL";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
        SQLQUERY = "UPDATE `settings` SET `value` = \'schema\', `data` = 2 ";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
    }
    /*Check current schema value*/

    SQLQUERY = "SELECT data FROM settings WHERE value=\'schema\' ";
    if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
    res = conn.fetch_query(SQLQUERY.c_str());
    if (auto row = res.fetch_row())  //if there is a result, update the row
    {
       schema_value=atoi(row[0]);
    }
    if( schema_value == 2 ) { //Upgrade from 2 to 3
        SQLQUERY = "CREATE TABLE `LiveData` ( \
		`id` BIGINT NOT NULL AUTO_INCREMENT , \
           	`DateTime` datetime NOT NULL, \
           	`Inverter` varchar(10) NOT NULL, \
           	`Serial` varchar(40) NOT NULL, \
		`Description` char(20) NOT NULL , \
		`Value` INT NOT NULL , \
		`Units` char(20) NOT NULL , \
           	`CHANGETIME` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP, \
           	UNIQUE KEY (`DateTime`,`Inverter`,`Serial`,`Description`), \
		PRIMARY KEY ( `id` ) \
		) ENGINE = MYISAM";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
        SQLQUERY = "UPDATE `settings` SET `value` = \'schema\', `data` = 3 ";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
    }

    if( schema_value == 3 ) { //Upgrade from 3 to 4
        SQLQUERY = "ALTER TABLE `DayData` CHANGE `Inverter` `Inverter` varchar(30) NOT NULL, CHANGE `Serial` `Serial` varchar(40) NOT NULL";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
        SQLQUERY = "ALTER TABLE `LiveData` CHANGE `Inverter` `Inverter` varchar(30) NOT NULL, CHANGE `Serial` `Serial` varchar(40) NOT NULL, CHANGE `Description` `Description` varchar(30) NOT NULL, CHANGE `Value` `Value` varchar(30), CHANGE `Units` `Units` varchar(20) NULL DEFAULT NULL ";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
        SQLQUERY = "UPDATE `settings` SET `value` = \'schema\', `data` = 4 ";
        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
        conn.query(SQLQUERY.c_str());
    }
}

int check_schema(const ConfType* conf, const FlagType* flag, const char* SCHEMA)
/*  Check if using the correct database schema */
{
    int	        found=0;

    MySQL conn( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, conf->MySqlDatabase);
    //Get Start of day value
    static const char SQLQUERY[] = "SELECT data FROM settings WHERE value=\'schema\' ";
    if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
    auto res = conn.fetch_query(SQLQUERY);
    if (auto row = res.fetch_row())  //if there is a result, update the row
    {
       if( strcmp( row[0], SCHEMA ) == 0 )
          found=1;
    }
    if( found != 1 )
    {
       fmt::printf( "Please Update database schema use --UPDATE\n" );
    }
    return found;
}


void live_mysql(const ConfType* conf, const FlagType* flag, const LiveDataType* livedatalist, int livedatalen)
/* Live inverter values mysql update */
{
    int		live_data=1;
    int		i;
 
    MySQL conn(conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, conf->MySqlDatabase);
    for( i=0; i<livedatalen; i++ ) {
        live_data=1;
        if( (livedatalist+i)->Persistent == 1 ) {
            const auto SQLQUERY = fmt::sprintf("SELECT IF (Value = \"%s\",NULL,Value) FROM LiveData where Inverter=\"%s\" and Serial=%llu and Description=\"%s\" ORDER BY DateTime DESC LIMIT 1", (livedatalist+i)->Value, (livedatalist+i)->inverter, (livedatalist+i)->serial, (livedatalist+i)->Description);
            if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
            auto res = conn.fetch_query(SQLQUERY.c_str());
            if (res) {
                if (auto row = res.fetch_row())  //if there is a result, update the row
                {
                     if( row[0] == NULL ) {
                         live_data=0;
                     }
                }
            }
        }
        if( live_data==1 ) {
            const auto SQLQUERY = fmt::sprintf("INSERT INTO LiveData ( DateTime, Inverter, Serial, Description, Value, Units ) VALUES ( \'%s\', \'%s\', %lld, \'%s\', \'%s\', \'%s\'  ) ON DUPLICATE KEY UPDATE DateTime=Datetime, Inverter=VALUES(Inverter), Serial=VALUES(Serial), Description=VALUES(Description), Description=VALUES(Description), Value=VALUES(Value), Units=VALUES(Units)", debugdate(), (livedatalist+i)->inverter, (livedatalist+i)->serial, (livedatalist+i)->Description, (livedatalist+i)->Value, (livedatalist+i)->Units);
            if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
            conn.query(SQLQUERY.c_str());
        }
    }
}
