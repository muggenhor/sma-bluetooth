/* tool to read power production data for SMA solar power convertors 
   Copyright Wim Hofman 2010 
   Copyright Stephen Collier 2010,2011 

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* repost is a utility to check pvoutput data and repost any differences */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <curl/curl.h>
#include "repost.hpp"
#include "sma_struct.hpp"
#include "sma_mysql.hpp"
#include <fmt/printf.h>

size_t write_data(char* ptr, size_t size, size_t nmemb, void* stream)
{
    return fwrite(ptr, size, nmemb, (FILE*)stream);
}

void sma_repost(const ConfType* conf, const FlagType* flag)
{
    FILE* fp;
    CURL *curl;
    int result;
    char buf[1024], buf1[400];
    int	 update_data;

    /* Connect to database */
    MySQL conn(conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, conf->MySqlDatabase);
    //Get Start of day value
    std::string SQLQUERY{"SELECT DATE_FORMAT( dt1.DateTime, \"%Y%m%d\" ), round((dt1.ETotalToday*1000-dt2.ETotalToday*1000),0) FROM DayData as dt1 join DayData as dt2 on dt2.DateTime = DATE_SUB( dt1.DateTime, interval 1 day ) WHERE dt1.DateTime LIKE \"%-%-% 23:55:00\" ORDER BY dt1.DateTime DESC"};
    fmt::printf("%s\n",SQLQUERY);
    auto res = conn.fetch_query(SQLQUERY.c_str());
    for (auto row = res.fetch_row(); row; res.fetch_row())
    {
        startforwait:
        fp=fopen( "/tmp/curl_output", "w+" );
        update_data = 0;
        float dtotal = atof(row[1]);
        sleep(2);  //pvoutput limits 1 second output
	auto compurl = fmt::sprintf("http://pvoutput.org/service/r1/getstatistic.jsp?df=%s&dt=%s&key=%s&sid=%s",row[0],row[0],conf->PVOutputKey,conf->PVOutputSid);
        curl = curl_easy_init();
        if (curl){
	     curl_easy_setopt(curl, CURLOPT_URL, compurl.c_str());
	     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	     curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	     //curl_easy_setopt(curl, CURLOPT_FAILONERROR, compurl.c_str());
	     auto cresult = curl_easy_perform(curl);
             if (flag->debug == 1) fmt::printf("result = %d\n",cresult);
             rewind( fp );
             fgets( buf, sizeof( buf ), fp );
             result = sscanf( buf, "Bad request %s has no outputs between the requested period", buf1 );
             fmt::printf( "return=%d buf1=%s\n", result, buf1 );
             if( result > 0 )
             {
                 update_data=1;
                 fmt::printf( "test\n" );
             }
             else
             {
                 fmt::printf( "buf=%s here 1.\n", buf );
                 result = sscanf( buf, "Forbidden 403: Exceeded 60 requests %s", buf1 );
		 if( result > 0 ) {
		    fmt::printf( "Too Many requests in 1hr sleeping for 1hr\n");
                    fclose(fp);
                    sleep(3600);
                    goto startforwait;
                 }
                    
                 fmt::printf( "return=%d buf1=%s\n", result, buf1 );
		 float power;
                 if( sscanf( buf, "%f,%s", &power, buf1 ) > 0 ) {
                    fmt::printf( "Power %f\n", power );
                    if( power != dtotal )
                    {
                       fmt::printf( "Power %f Produced=%f\n", power, dtotal );
                       update_data=1;
                    }
                 }
             }
	     curl_easy_cleanup(curl);
             if( update_data == 1 ) {
                 curl = curl_easy_init();
                 if (curl){
	            compurl = fmt::sprintf("http://pvoutput.org/service/r2/addoutput.jsp?d=%s&g=%f&key=%s&sid=%s",row[0],dtotal,conf->PVOutputKey,conf->PVOutputSid);
                    if (flag->debug == 1) fmt::printf("url = %s\n",compurl);
		    curl_easy_setopt(curl, CURLOPT_URL, compurl.c_str());
		    curl_easy_setopt(curl, CURLOPT_FAILONERROR, compurl.c_str());
		    auto cresult = curl_easy_perform(curl);
                    sleep(1);
	            if (flag->debug == 1) fmt::printf("result = %d\n",cresult);
		    curl_easy_cleanup(curl);
                    if( cresult==0 ) 
                    {
                        SQLQUERY = fmt::sprintf("UPDATE DayData set PVOutput=NOW() WHERE DateTime=\"%s235500\"  ", row[0]);
                        if (flag->debug == 1) fmt::printf("%s\n",SQLQUERY);
			//conn.query(SQLQUERY.c_str());
                    }
                    else
                        break;
                 }
             }
        }
        fclose(fp);
    }
}
