#define _XOPEN_SOURCE 700

#include "sb_commands.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sma_struct.hpp"
#include <sys/socket.h>
#include <time.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <errno.h>
#include "smatool.hpp"
#include <unistd.h>
#include <fmt/printf.h>

static int GetLine(const char* command, FILE* fp);

int ConnectSocket(const ConfType* conf)
{
    struct sockaddr_rc addr = {};
    int i;
    int s=0;
    int status=-1; //connection status
   
    //Try for a few connects
    for( i=1; i<20; i++ ){
        // allocate a socket
        if(( (s) = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) > 0 ) {

            // set the connection parameters (who to connect to)
            addr.rc_family = AF_BLUETOOTH;
            addr.rc_channel = (uint8_t) 1;
            str2ba(conf->BTAddress, &addr.rc_bdaddr);
    
            // connect to server
      	    status = connect((s), (struct sockaddr *)&addr, sizeof(addr));
      	    if (status <0){
                fmt::printf("Error connecting to %s\n", conf->BTAddress);
                close( (s) );
      	    }
            else
                //conected
                break;
        }
        else
        {
            //Can't open socket try again
            close((s));
        }
    }
    return( s );
}
/*
 * Update internal running list with live data for later processing
 */
static int UpdateLiveList(const FlagType* flag, const UnitType* unit, const char* format, time_t idate,
                          const char* description, float fvalue, int ivalue, const char* svalue, const char* units,
                          int persistent, std::vector<LiveDataType>& livedata)
{
  if (strlen(unit->Inverter) <= 0)
  {
    if (flag->debug == 1) fmt::printf("Don't have inverter details yet\n");
    return 1;
  }

  LiveDataType data;
  data.date        = idate;
  strcpy(data.inverter, unit->Inverter);
  data.serial      = (unit->Serial[0] << 24) + (unit->Serial[1] << 16) + (unit->Serial[2] << 8) + (unit->Serial[3] << 0);
  strcpy(data.Description, description);
  strcpy(data.Units, units);
  if (fvalue >= 0)
    sprintf(data.Value, format, fvalue);
  else if (ivalue > 0)
    sprintf(data.Value, format, ivalue);
  else if (strlen(svalue) > 0)
    sprintf(data.Value, format, svalue);
  data.Persistent = persistent;
  livedata.push_back(std::move(data));
  return 0;
}

static int ProcessCommand(ConfType* conf, const FlagType* flag, UnitType** unit, const int s, FILE* fp, int* linenum,
                          std::vector<ArchDataType>& archdata, std::vector<LiveDataType>& livedata)
{
   char  *line;
   size_t len=0;
   ssize_t read;
   int   i, j, cc, rr;
   int	 datalen=0;
   int   failedbluetooth=0;
   int   togo=0;
   int   finished;
   time_t reporttime;
   time_t fromtime;
   time_t totime;
   time_t idate;
   time_t prev_idate;
   unsigned char tzhex[2] = { 0 };
   unsigned char timeset[4] = { 0x30,0xfe,0x7e,0x00 };
   struct tm tm;
   int day,month,year,hour,minute,second;
   unsigned char fl[1024] = { 0 };
   unsigned char received[1024];
   unsigned char datarecord[1024];
   unsigned char dest_address[6] = { 0 };
   unsigned char timestr[25] = { 0 };
   ReadRecordType readRecord;
   char  *lineread;
   unsigned char * last_sent;
   unsigned char * data;
   char	BTAddressBuf[20];
   char tt[10] = {48,48,48,48,48,48,48,48,48,48}; 
   char ti[3];	
   float currentpower_total;
   float dtotal;
   float gtotal;
   float ptotal;
   float strength;
   int   found, already_read, terminated;
   int   return_key, datalength=0;
   int	 pass_i, send_count = 0;
   int	 persistent;
   int index;
   unsigned long long inverter_serial;

   //convert address
   strncpy( BTAddressBuf, conf->BTAddress, 20);
   dest_address[5] = conv(strtok( BTAddressBuf,":"));
   dest_address[4] = conv(strtok(NULL,":"));
   dest_address[3] = conv(strtok(NULL,":"));
   dest_address[2] = conv(strtok(NULL,":"));
   dest_address[1] = conv(strtok(NULL,":"));
   dest_address[0] = conv(strtok(NULL,":"));
    /* get the report time - used in various places */
    reporttime = time(NULL);  //get time in seconds since epoch (1/1/1970)

    while (( read = getline(&line,&len,fp) ) != -1){	//read line from sma.in
   	(*linenum)++;
   	last_sent = (unsigned  char *)malloc( sizeof( unsigned char ));
   	if ( last_sent == NULL ) {
       	    fmt::printf( "\nOut of memory\n" );
            return( -1 );
   	}
   	lineread = strtok(line," ;");
   	if( lineread[0] == ':'){		//See if line is something we need to receive
       	    if( flag->debug == 1 ) fmt::printf( "\nCommand line we have finished%s\n", line ); 
            break;
        }
   	if(!strcmp(lineread,"R")){		//See if line is something we need to receive
       if (flag->debug == 1) fmt::printf("[%d] %s Waiting for string\n",(*linenum), debugdate() );
       cc = 0;
       do{
	   lineread = strtok(NULL," ;");
	   switch(select_str(lineread)) {
	
	   case 0: // $END
		//do nothing
		break;			

	   case 1: // $ADDR
		for (i=0;i<6;i++){
	 	    fl[cc] = dest_address[i];
		    cc++;
		}
	   	break;	

	   case 3: // $SERIAL
		for (i=0;i<4;i++){
	  	    fl[cc] = unit[0]->Serial[i];
		    cc++;
		}
		break;	
				
	   case 7: // $ADD2
		for (i=0;i<6;i++){
		    fl[cc] = conf->MyBTAddress[i];
		    cc++;
		}
		break;	

	   default :
	   	fl[cc] = conv(lineread);
		cc++;
	   }

        } 
        while (strcmp(lineread,"$END"));
	if (flag->debug == 1){ 
	    fmt::printf("[%d] %s waiting for: ", (*linenum), debugdate() );
	    for (i=0;i<cc;i++) fmt::printf("%02x ",fl[i]);
	    fmt::printf("\n\n");
	}
	if (flag->debug == 1) fmt::printf("[%d] %s Waiting for data on rfcomm\n", (*linenum), debugdate());
	found = 0;
	do {
            if( already_read == 0 )
                rr=0;
            if(( already_read == 0 )&& read_bluetooth(conf, flag, &readRecord, s, &rr, received, cc, last_sent, &terminated) != 0)
            {
                already_read=0;
                found=0;
                strcpy( lineread, "" );
                sleep(10);
                failedbluetooth++;
                if( failedbluetooth > 3 )
                    return( -1 );
            }
            else {
                already_read=0;
                            
	        if (flag->debug == 1){ 
                    fmt::printf( "[%d] %s looking for: ",(*linenum), debugdate());
		    for (i=0;i<cc;i++) fmt::printf("%02x ",fl[i]);
                    fmt::printf( "\n" );
                    fmt::printf( "[%d] %s received:    ",(*linenum), debugdate());
		    for (i=0;i<rr;i++) fmt::printf("%02x ",received[i]);
		    fmt::printf("\n\n");
		}
                           
		if (memcmp(fl+4,received+4,cc-4) == 0){
		    found = 1;
		    if (flag->debug == 1) fmt::printf("[%d] %s Found string we are waiting for\n",(*linenum), debugdate()); 
		} else {
		    if (flag->debug == 1) fmt::printf("[%d] %s Did not find string\n", (*linenum),debugdate()); 
		}
            }
	} while (found == 0);
	if (flag->debug == 2){ 
	    for (i=0;i<cc;i++) fmt::printf("%02x ",fl[i]);
	    fmt::printf("\n\n");
	}
    }
    if(!strcmp(lineread,"S")){		//See if line is something we need to send
        //Empty the receive data ready for new command
        while( ((*linenum)>22)&& empty_read_bluetooth(flag, &readRecord, s, &rr, received, &terminated) >= 0);
	if (flag->debug	== 1) fmt::printf("[%d] %s Sending\n", (*linenum),debugdate());
	cc = 0;
	do{
            lineread = strtok(NULL," ;");
	    switch(select_str(lineread)) {
	
	    case 0: // $END
		    //do nothing
		break;			

	    case 1: // $ADDR
		for (i=0;i<6;i++){
		    fl[cc] = dest_address[i];
		    cc++;
		}
		break;

	    case 3: // $SERIAL
		for (i=0;i<4;i++){
		    fl[cc] = unit[0]->Serial[i];
		    cc++;
		}
		break;	

	    case 7: // $ADD2
		for (i=0;i<6;i++){
		    fl[cc] = conf->MyBTAddress[i];
		    cc++;
		}
		break;

	    case 2: // $TIME	
		// get report time and convert
		sprintf(tt,"%x",(int)reporttime); //convert to a hex in a string
		for (i=7;i>0;i=i-2){ //change order and convert to integer
		    ti[1] = tt[i];
		    ti[0] = tt[i-1];	
                    ti[2] = '\0';
		    fl[cc] = conv(ti);
		    cc++;		
		}
		break;

	    case 11: // $TMPLUS	
	 	// get report time and convert
		sprintf(tt,"%x",(int)reporttime+1); //convert to a hex in a string
		for (i=7;i>0;i=i-2){ //change order and convert to integer
	            ti[1] = tt[i];
		    ti[0] = tt[i-1];	
                    ti[2] = '\0';
		    fl[cc] = conv(ti);
		    cc++;		
		}
		break;


	    case 10: // $TMMINUS
		// get report time and convert
		sprintf(tt,"%x",(int)reporttime-1); //convert to a hex in a string
		for (i=7;i>0;i=i-2){ //change order and convert to integer
		    ti[1] = tt[i];
	 	    ti[0] = tt[i-1];	
                    ti[2] = '\0';
		    fl[cc] = conv(ti);
		    cc++;		
		}
		break;

	    case 4: //$crc
		tryfcs16(flag, fl+19, cc -19,fl,&cc);
                add_escapes(fl,&cc);
                fix_length_send(flag,fl,&cc);
		break;

	    case 12: // $TIMESTRING
		for (i=0;i<25;i++){
		    fl[cc] = timestr[i];
		    cc++;
		}
		break;

	    case 13: // $TIMEFROM1	
		// get report time and convert
                if( flag->daterange == 1 ) {
                    if( strptime( conf->datefrom, "%Y-%m-%d %H:%M:%S", &tm) == 0 ) 
                    {
                        if( flag->debug==1 ) fmt::printf( "datefrom %s\n", conf->datefrom );
                        fmt::printf( "Time Coversion Error\n" );
                        exit(-1);
                    }
                    tm.tm_isdst=-1;
                    fromtime=mktime(&tm);
                    if( fromtime == -1 ) {
                        // Error we need to do something about it
                        fmt::printf( "%03x",(int)fromtime ); getchar();
                        fmt::printf( "\n%03x", (int)fromtime ); getchar();
                        fromtime=0;
                        fmt::printf( "bad from" ); getchar();
                    }
                }
                else
                {
                    fmt::printf( "no from" ); getchar();
                    fromtime=0;
                }
		sprintf(tt,"%03x",(int)fromtime-300); //convert to a hex in a string and start 5 mins before for dummy read.
		for (i=7;i>0;i=i-2){ //change order and convert to integer
		    ti[1] = tt[i];
		    ti[0] = tt[i-1];	
                    ti[2] = '\0';
		    fl[cc] = conv(ti);
		    cc++;		
		}
		break;

	    case 14: // $TIMETO1	
                if( flag->daterange == 1 ) {
                    if( strptime( conf->dateto, "%Y-%m-%d %H:%M:%S", &tm) == 0 ) 
                    {
                        if( flag->debug==1 ) fmt::printf( "dateto %s\n", conf->dateto );
                        fmt::printf( "Time Coversion Error\n" );
                        exit(-1);
                    }
                    tm.tm_isdst=-1;
                    totime=mktime(&tm);
                    if( totime == -1 ) {
                        // Error we need to do something about it
                        fmt::printf( "%03x",(int)totime ); getchar();
                        fmt::printf( "\n%03x", (int)totime ); getchar();
                        totime=0;
                        fmt::printf( "bad to" ); getchar();
                    }
                }
                else
                    totime=0;
		sprintf(tt,"%03x",(int)totime); //convert to a hex in a string
		   // get report time and convert
		for (i=7;i>0;i=i-2){ //change order and convert to integer
		    ti[1] = tt[i];
		    ti[0] = tt[i-1];	
                    ti[2] = '\0';
		    fl[cc] = conv(ti);
		    cc++;		
		}
		break;

	    case 15: // $TIMEFROM2	
                if( flag->daterange == 1 ) {
                    strptime( conf->datefrom, "%Y-%m-%d %H:%M:%S", &tm);
                    tm.tm_isdst=-1;
                    fromtime=mktime(&tm)-86400;
                    if( fromtime == -1 ) {
                        // Error we need to do something about it
                        fmt::printf( "%03x",(int)fromtime ); getchar();
                        fmt::printf( "\n%03x", (int)fromtime ); getchar();
                        fromtime=0;
                        fmt::printf( "bad from" ); getchar();
                    }
                }
                else
                {
                    fmt::printf( "no from" ); getchar();
                    fromtime=0;
                }
		sprintf(tt,"%03x",(int)fromtime); //convert to a hex in a string
		for (i=7;i>0;i=i-2){ //change order and convert to integer
		    ti[1] = tt[i];
		    ti[0] = tt[i-1];	
                    ti[2] = '\0';
		    fl[cc] = conv(ti);
		    cc++;		
		}
		break;

	    case 16: // $TIMETO2	
                if( flag->daterange == 1 ) {
                    strptime( conf->dateto, "%Y-%m-%d %H:%M:%S", &tm);

                    tm.tm_isdst=-1;
                    totime=mktime(&tm)-86400;
                    if( totime == -1 ) {
                         // Error we need to do something about it
                         fmt::printf( "%03x",(int)totime ); getchar();
                         fmt::printf( "\n%03x", (int)totime ); getchar();
                         fromtime=0;
                         fmt::printf( "bad from" ); getchar();
                    }
                }
                else
                    totime=0;
		    sprintf(tt,"%03x",(int)totime); //convert to a hex in a string
		    for (i=7;i>0;i=i-2){ //change order and convert to integer
			ti[1] = tt[i];
		        ti[0] = tt[i-1];	
                        ti[2] = '\0';
		        fl[cc] = conv(ti);
			cc++;		
		    }
	       	    break;
				
	    case 19: // $PASSWORD
                              
                j=0;
		for(i=0;i<12;i++){
		    if( conf->Password[j] == '\0' )
                       	fl[cc] = 0x88;
                    else {
                        pass_i = conf->Password[j];
                        fl[cc] = (( pass_i+0x88 )%0xff);
                        j++;
                    }
                    cc++;
		}
		break;	

	    case 21: // $SUSyID
                for( i=0; i<2; i++ ) {
	            fl[cc] = unit[0]->SUSyID[i];
	            cc++;
                }
                break;

	    case 22: // $INVCODE
	        fl[cc] = conf->NetID;
	        cc++;
                break;

	    case 25: // $CNT send counter
                send_count++;
	        fl[cc] = send_count;
	        cc++;
                break;

            case 26: // $TIMEZONE timezone in seconds, reverse endian
	        fl[cc] = tzhex[0];
	        fl[cc+1] = tzhex[1];
	        cc+=2;
                break;

	    case 27: // $TIMESET unknown setting
                for( i=0; i<4; i++ ) {
	            fl[cc] = timeset[i];
	            cc++;
                }
                break;

	    case 29: // $MYSUSYID
                for( i=0; i<2; i++ ) {
	            fl[cc] = conf->MySUSyID[i];
	            cc++;
                }
                break;

	    case 30: // $MYSERIAL
                for( i=0; i<4; i++ ) {
	            fl[cc] = conf->MySerial[i];
		    cc++;
                }
                fmt::printf( "\n" );
                break;

	    default :
		fl[cc] = conv(lineread);
		cc++;
	    }

	} while (strcmp(lineread,"$END"));
	if (flag->debug == 1){ 
            int last_decoded;

            fmt::printf(" cc=%d",cc);
	    fmt::printf("\n\n");
            fmt::printf( "\n-----------------------------------------------------------" );
            fmt::printf( "\nSEND:");
            //Start byte
            fmt::printf("\n7e ");
            j=0;
            //Size and checkbit
            fmt::printf("%02x ",fl[++j]);
            fmt::printf("                      size:              %d", fl[j] );
            fmt::printf("\n   " );
            fmt::printf("%02x ",fl[++j]);
            fmt::printf("\n   " );
            fmt::printf("%02x ",fl[++j]);
            fmt::printf("                      checkbit:          %d", fl[j] );
            fmt::printf("\n   " );
            //Source Address
            for( i=++j; i<cc; i++ ) {
                if( i > j+5 ) break;
                fmt::printf("%02x ",fl[i]);
            }
            fmt::printf("       source:            %02x:%02x:%02x:%02x:%02x:%02x", fl[j+5], fl[j+4], fl[j+3], fl[j+2], fl[j+1], fl[j] );
            j=j+5;
            fmt::printf("\n   " );
            //Destination Address
            for( i=++j; i<cc; i++ ) {
                if( i > j+5 ) break;
                fmt::printf("%02x ",fl[i]);
            }
            fmt::printf("       destination:       %02x:%02x:%02x:%02x:%02x:%02x", fl[j+5], fl[j+4], fl[j+3], fl[j+2], fl[j+1], fl[j] );
            j=j+5;
            fmt::printf("\n   " );
            //Destination Address
            for( i=++j; i<cc; i++ ) {
                if( i > j+1 ) break;
                fmt::printf("%02x ",fl[i]);
            }
            fmt::printf("                   control:           %02x%02x", fl[j+1], fl[j] );
            j++;
            last_decoded=j+1;
            j++;
            if( memcmp( fl+j, "\x7e\xff\x03\x60\x65", 5 ) == 0 ){
                fmt::printf("\n");
                for( i=j; i<cc; i++ ) {
                    if( i > j+4 ) break;
                    fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("             SMA Data2+ header: %02x:%02x:%02x:%02x:%02x", fl[j+4], fl[j+3], fl[j+2], fl[j+1], fl[j] );
                j+=5;
                fmt::printf("\n   " );
                for( i=++j; i<cc; i++ ) {
                    if( i > j ) break;
                    fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("                      data packet size:  %02d", fl[j] );
                fmt::printf("\n   " );
                for( i=++j; i<cc; i++ ) {
                    if( i > j ) break;
                    fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("                      SUSYId:            %02x", fl[j] );
                j++;
                fmt::printf("\n   " );
                for( i=++j; i<cc; i++ ) {
                    if( i > j+3 ) break;
                    fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("             Serial:            %02x:%02x:%02x:%02x",  fl[j+3], fl[j+2], fl[j+1], fl[j] );
                j=j+3;
                fmt::printf("\n   " );
                for( i=++j; i<cc; i++ ) {
                    if( i > j+1 ) break;
                    fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("                   unknown:           %02x %02x", fl[j+1], fl[j] );
                j++;
                fmt::printf("\n   " );
                for( i=++j; i<cc; i++ ) {
                    if( i > j+1 ) break;
                    fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("                   MySUSId:           %02x:%02x", fl[j+1], fl[j] );
                fmt::printf("\n   " );
                for( i=++j; i<cc; i++ ) {
                    if( i > j+3 ) break;
                        fmt::printf("%02x ",fl[i]);
                }
                fmt::printf("             MySerial:          %02x:%02x:%02x:%02x", fl[j+3],fl[j+2],fl[j+1], fl[j] );
                fmt::printf("\n   " );
                j++;
                last_decoded=j+1;
             }
             fmt::printf("\n   " );
             j=0;
	     for (i=last_decoded;i<cc;i++) {
                 if( j%16== 0 )
                     fmt::printf( "\n   %08x: ",j);
                 fmt::printf("%02x ",fl[i]);
                 j++;
             }
             fmt::printf(" rr=%d",(cc+3));
	     fmt::printf("\n\n");
        }
        last_sent = (unsigned  char *)realloc( last_sent, sizeof( unsigned char )*(cc));
        memcpy(last_sent,fl,cc);
        write(s, fl, cc);
        already_read=0;
       //check_send_error( &conf, &s, &rr, received, cc, last_sent, &terminated, &already_read ); 
    }


    if(!strcmp(lineread,"E")){		//See if line is something we need to extract
        if( readRecord.Status[0]==0xe0 ) {
	    if (flag->debug	== 1) fmt::printf("\nThere is no data currently available %s\n", debugdate());
                // Read the rest of the records
                while( read_bluetooth(conf, flag, &readRecord, s, &rr, received, cc, last_sent, &terminated) == 0 );

            }
            else
            {
	        if (flag->debug	== 1) fmt::printf("[%d] %s Extracting\n", (*linenum), debugdate());
		cc = 0;
		do{
		    lineread = strtok(NULL," ;");
		    //fmt::printf( "\nselect=%d", select_str(lineread)); 
		    switch(select_str(lineread)) {
                                
		    case 9: // extract Time from Inverter
                        idate=ConvertStreamtoTime( received+66, 4, &idate, &day, &month, &year, &hour, &minute, &second );
			fmt::printf("Date power = %d/%d/%4d %02d:%02d:%02d\n",day, month, year, hour, minute,second);
			//currentpower = (received[72] * 256) + received[71];
			//fmt::printf("Current power = %i Watt\n",currentpower);
			break;
		    case 5: // extract current power $POW
                        if(( data = ReadStream(conf, flag, &readRecord, s, received, &rr, &datalen, last_sent, cc, &terminated, &togo)) != NULL )
                        {
                           //fmt::printf( "\ndata=%02x:%02x:%02x:%02x:%02x:%02x\n", data[0], (data+1)[0], (data+2)[0], (data+3)[0], (data+4)[0], (data+5)[0] );
                            int gap;
                            switch (data[3])
                            {
                                case 0x08: gap = 40; break;
                                case 0x10: gap = 40; break;
                                case 0x40: gap = 28; break;
                                case 0x00: gap = 28; break;
                                default:
                                    abort();
                            }
                            for ( i = 0; i<datalen; i+=gap ) 
                            {
                                idate=ConvertStreamtoTime( data+i+4, 4, &idate, &day, &month, &year, &hour, &minute, &second );
                                ConvertStreamtoFloat( data+i+8, 3, &currentpower_total );
                                return_key=-1;
                                for (unsigned int j = 0; j < conf->num_return_keys; j++)
                                {
                                    if(( (data+i+1)[0] == conf->returnkeylist[j].key1 )&&((data+i+2)[0] == conf->returnkeylist[j].key2)) {
                                        return_key=j;
                                        break;
                                    }
                                }
                                if( return_key >= 0 )
				{
				    fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-20s = %.0f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
				    inverter_serial = (unit[0]->Serial[3]<<24) + (unit[0]->Serial[2]<<16) + (unit[0]->Serial[1]<<8) + (unit[0]->Serial[0]);
                                }
                                else
                                    if( (data+0)[0] > 0 )

				        fmt::printf("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS\n", year, month, day, hour, minute, second, (data+i+1)[0], (data+i+1)[1], currentpower_total );
			    }
                            free( data );
			    break;
                        }
                        else
                            //An Error has occurred
                            break;

		    case 6: // extract total energy collected today
                         
			gtotal = (received[69] * 65536) + (received[68] * 256) + received[67];
			gtotal = gtotal / 1000;
                        fmt::printf("G total so far = %.2f Kwh\n",gtotal);
			dtotal = (received[84] * 256) + received[83];
			dtotal = dtotal / 1000;
                        fmt::printf("E total today = %.2f Kwh\n",dtotal);
                        break;		

		    case 7: // extract 2nd address
                        fmt::printf( "\naddress=" );
                        for( i=0; i<6; i++ ) {
                            fmt::printf( "%02x ", received[26+i] );
                        }
                        for (i=0; i<6; i++ ) {
                            conf->MyBTAddress[i]=received[26+i];
                        }
			if (flag->debug == 1) fmt::printf("address 2 = %02x:%02x:%02x:%02x:%02x:%02x \n", conf->MyBTAddress[0], conf->MyBTAddress[1], conf->MyBTAddress[2], conf->MyBTAddress[3], conf->MyBTAddress[4], conf->MyBTAddress[5] );
			break;
				
		    case 12: // extract time strings $TIMESTRING
                        if (flag->debug == 1) fmt::printf("received[60]=0x%0X - Expected 0x6D\n", received[60]);
                        if (flag->debug == 1) fmt::printf("received[60]=0x%0X - Expected 0x6D\n", received[60]);
                        if(( received[60] == 0x6d )&&( received[61] == 0x23 ))
                        {
			    memcpy(timestr,received+63,24);
			    if (flag->debug == 1) fmt::printf("extracting timestring\n");
                            memcpy(timeset,received+79,4);
                            idate=ConvertStreamtoTime( received+63,4, &idate, &day, &month, &year, &hour, &minute, &second  );
                            /* Allow delay for inverter to be slow */
                            if( reporttime > idate ) {
                                if( flag->debug == 1 )
                                    fmt::printf( "delay=%d\n", (int)(reporttime-idate) );
                                //sleep( reporttime - idate );
                                sleep(5);    //was sleeping for > 1min excessive
                            }
                        }
                        else
                        {
                            if (received[61]==0x7e) {
				fmt::printf("$TIMESTRING extraction failed. Check password!!!\n");
			    }	
			    else
                            {
			        memcpy(timestr,received+63,24);
			        if (flag->debug == 1) fmt::printf("bad extracting timestring\n");
                            }
                            already_read=0;
                            found=0;
                            strcpy( lineread, "" );
                            failedbluetooth++;
                            if( failedbluetooth > 60 )
                                exit(-1);
                            //exit(-1);
                        }
                              
			break;

		    case 17: // Test data
                        if(( data = ReadStream(conf, flag,  &readRecord, s, received, &rr, &datalen, last_sent, cc, &terminated, &togo)) != NULL )
                        {
                            fmt::printf( "\n" );
                        
                            free( data );
		            break;
                        }
                        else
                           //An Error has occurred
                            break;
				
		    case 18: // $ARCHIVEDATA1
                        finished=0;
                        ptotal=0;
                        idate=0;
                        fmt::printf( "\n" );
                        while( finished != 1 ) {
                            if(( data = ReadStream(conf, flag,  &readRecord, s, received, &rr, &datalen, last_sent, cc, &terminated, &togo)) != NULL )
                            {
                                j=0;
                                for( i=0; i<datalen; i++ )
                                {
                                    datarecord[j]=data[i];
                                    j++;
                                    if( j > 11 ) {
                                        if( idate > 0 ) prev_idate=idate;
                                        else prev_idate=0;
                                        idate=ConvertStreamtoTime( datarecord, 4, &idate, &day, &month, &year, &hour, &minute, &second  );
                                        if( prev_idate == 0 )
                                            prev_idate = idate-300;

                                        ConvertStreamtoFloat( datarecord+4, 8, &gtotal );
                                        if (archdata.empty())
                                            ptotal = gtotal;
	                                    fmt::printf("\n%d/%d/%4d %02d:%02d:%02d  total=%.3f Kwh current=%.0f Watts togo=%d i=%d", day, month, year, hour, minute,second, gtotal/1000, (gtotal-ptotal)*12, togo, i);
					    if( idate != prev_idate+300 ) {
                                                fmt::printf( "Date Error! prev=%d current=%d\n", (int)prev_idate, (int)idate );
					        break;
                                            }
                                            ArchDataType adata;
					    adata.date          = idate;
                                            strcpy(adata.inverter, unit[0]->Inverter);
				            adata.serial        = (unit[0]->Serial[0] << 24) + (unit[0]->Serial[1] << 16)
                                                                + (unit[0]->Serial[2] <<  8) + (unit[0]->Serial[3] <<  0);
                                            adata.accum_value   = gtotal / 1000;
                                            adata.current_value = (gtotal - ptotal) * 12;
                                            archdata.push_back(std::move(adata));
                                            ptotal=gtotal;
                                            j=0; //get ready for another record
                                        }
                                    }
                                    if( togo == 0 ) 
                                        finished=1;
                                    else
                                        if( read_bluetooth(conf, flag, &readRecord, s, &rr, received, cc, last_sent, &terminated) != 0 )
                                        {
                                            found=0;
                                            /*
                                            archdata.clear();
                                            livedata.clear();
                                            */
                                            strcpy( lineread, "" );
                                            sleep(10);
                                            failedbluetooth++;
                                            if( failedbluetooth > 3 )
                                                exit(-1);
                                        }
                                    }
                                    else
                                        //An Error has occurred
                                        break;
				}
                                free( data );
                                fmt::printf( "\n" );
                          
				break;
			    case 20: // SIGNAL signal strength
                          
				strength  = (received[22] * 100.0)/0xff;
	                        if (flag->verbose == 1) {
                                    fmt::printf("bluetooth signal = %.0f%%\n",strength);
                                }
                                break;		
			    case 21: // extract $SUSID
			        unit[0]->SUSyID[0]=received[24];
			        unit[0]->SUSyID[1]=received[25];
				if (flag->debug == 1) fmt::printf("extracting SUSyID=%02x:%02x\n", unit[0]->SUSyID[0], unit[0]->SUSyID[1] );
				break;
			    case 22: // extract time strings $INVCODE
				conf->NetID=received[22];
				if (flag->debug == 1) fmt::printf("extracting invcode=%02x\n", conf->NetID);
                                
				break;
			    case 24: // Inverter data $INVERTERDATA
                                if(( data = ReadStream(conf, flag,  &readRecord, s, received, &rr, &datalen, last_sent, cc, &terminated, &togo)) != NULL )
                                {
                                    if( flag->debug==1 ) fmt::printf( "data=%02x\n",(data+3)[0] );
                                    int gap;
                                    switch (data[3])
                                    {
                                        case 0x08: gap = 40; break;
                                        case 0x10: gap = 40; break;
                                        case 0x40: gap = 28; break;
                                        case 0x00: gap = 28; break;
                                        default:
                                            abort();
                                    }
                                    for ( i = 0; i<datalen; i+=gap ) 
                                    {
                                       idate=ConvertStreamtoTime( data+i+4, 4, &idate, &day, &month, &year, &hour, &minute, &second  );
                                       ConvertStreamtoFloat( data+i+8, 3, &currentpower_total );
                                       return_key=-1;
                                       for (unsigned int j = 0; j < conf->num_return_keys; j++)
                                       {
                                          if(( (data+i+1)[0] == conf->returnkeylist[j].key1 )&&((data+i+2)[0] == conf->returnkeylist[j].key2)) {
                                              return_key=j;
                                              break;
                                          }
                                       }
                                       if( return_key >= 0 ) {
                                           if( i==0 )
				               fmt::printf("%d-%02d-%02d  %02d:%02d:%02d %s\n", year, month, day, hour, minute, second, (data+i+8) );
				           fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-20s = %.0f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
                                       }
                                       else
                                           if( data[0]>0 )
				                fmt::printf("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS \n", year, month, day, hour, minute, second, (data+i+1)[0], (data+i+1)[0], currentpower_total );
                                    }
                                    free( data );
				    break;
                                }
                                else
                                    //An Error has occurred
                                    break;
			    case 28: // extract data $DATA
                                if(( data = ReadStream(conf, flag, &readRecord, s, received, &rr, &datalen, last_sent, cc, &terminated, &togo)) != NULL )
                                {
                                    int gap = 0;
                                    return_key=-1;
                                    for (unsigned int j = 0; j < conf->num_return_keys; j++)
                                    {
                                       if(( (data+1)[0] == conf->returnkeylist[j].key1 )&&((data+2)[0] == conf->returnkeylist[j].key2)) {
                                          return_key=j;
                                          break;
                                       }
                                    }
                                    if( return_key >= 0 ) {
				        gap=conf->returnkeylist[return_key].recordgap;
                                        datalength=conf->returnkeylist[return_key].datalength;
                                    }
                                    else
                                        if( datalen > 0 )
                                             fmt::printf( "\nFailed to find key %02x:%02x", (data+1)[0],(data+2)[0] );
                                    
                                    for ( i = 0; i<datalen; i+=gap ) 
                                    {
                                       idate=ConvertStreamtoTime( data+i+4, 4, &idate, &day, &month, &year, &hour, &minute, &second  );
                                       return_key=-1;
                                       for (unsigned int j = 0; j < conf->num_return_keys; j++)
                                       {
                                          if(( (data+i+1)[0] == conf->returnkeylist[j].key1 )&&((data+i+2)[0] == conf->returnkeylist[j].key2)) {
                                              return_key=j;
                                              break;
                                          }
                                       }
                                       if( return_key >= 0 )
				       {
      				           switch( conf->returnkeylist[return_key].decimal ) {
					   case 0 :
                                               ConvertStreamtoFloat( data+i+8, datalength, &currentpower_total );
                                               if( currentpower_total == 0 )
                                                   persistent=1;
                                               else
                                                   persistent = conf->returnkeylist[return_key].persistent;
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %.0f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%.0f",  idate, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, -1, (char *)NULL, conf->returnkeylist[return_key].units, persistent, livedata);
					       break;
        				   case 1 :
                                               ConvertStreamtoFloat( data+i+8, datalength, &currentpower_total );
                                               if( currentpower_total == 0 )
                                                   persistent=1;
                                               else
                                                   persistent = conf->returnkeylist[return_key].persistent;
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %.1f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%.1f",  idate, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, -1, (char *)NULL, conf->returnkeylist[return_key].units, persistent, livedata);
					       break;
        				   case 2 :
                                               ConvertStreamtoFloat( data+i+8, datalength, &currentpower_total );
                                               if( currentpower_total == 0 )
                                                   persistent=1;
                                               else
                                                   persistent = conf->returnkeylist[return_key].persistent;
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %.2f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%.2f",  idate, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, -1, (char *)NULL, conf->returnkeylist[return_key].units, persistent, livedata);
					       break;
        				   case 3 :
                                               ConvertStreamtoFloat( data+i+8, datalength, &currentpower_total );
                                               if( currentpower_total == 0 )
                                                   persistent=1;
                                               else
                                                   persistent = conf->returnkeylist[return_key].persistent;
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %.3f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%.3f",  idate, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, -1, (char *)NULL, conf->returnkeylist[return_key].units, persistent, livedata);
					       break;
        				   case 4 :
                                               ConvertStreamtoFloat( data+i+8, datalength, &currentpower_total );
                                               if( currentpower_total == 0 )
                                                   persistent=1;
                                               else
                                                   persistent = conf->returnkeylist[return_key].persistent;
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %.4f %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%.4f",  idate, conf->returnkeylist[return_key].description, currentpower_total/conf->returnkeylist[return_key].divisor, -1, (char *)NULL, conf->returnkeylist[return_key].units, persistent, livedata);
					       break;
                                           case 97 :
                                           {
                                               idate=ConvertStreamtoTime( data+i+4, 4, &idate, &day, &month, &year, &hour, &minute, &second  );
		       			       fmt::printf("                    %-30s = %d-%02d-%02d %02d:%02d:%02d\n", conf->returnkeylist[return_key].description, year, month, day, hour, minute, second );
                                               auto valuebuf = fmt::sprintf("%d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second );
                                               UpdateLiveList(flag, unit[0], "%s",  idate, conf->returnkeylist[return_key].description, -1.0, -1, valuebuf.c_str(), conf->returnkeylist[return_key].units, conf->returnkeylist[return_key].persistent, livedata);

                                               break;
                                           }
        				   case 98 :
                                           {
                                               idate=ConvertStreamtoTime( data+i+4, 4, &idate, &day, &month, &year, &hour, &minute, &second  );
                                               ConvertStreamtoInt( data+i+8, 2, &index );
                                               auto datastring = return_sma_description(index);
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %s %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, datastring, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%s",  idate, conf->returnkeylist[return_key].description, -1.0, -1, datastring, conf->returnkeylist[return_key].units, conf->returnkeylist[return_key].persistent, livedata);
                                               if( (data+i+1)[0]==0x20 && (data+i+2)[0] == 0x82 ) {
                                                    strcpy(unit[0]->Inverter, datastring);
                                               }
					       break;
                                           }
        				   case 99 :
                                           {
                                               idate=ConvertStreamtoTime( data+i+4, 4, &idate, &day, &month, &year, &hour, &minute, &second  );
                                               auto datastring = ConvertStreamtoString( data+i+8, datalength );
		       			       fmt::printf("%d-%02d-%02d %02d:%02d:%02d %-30s = %s %-20s\n", year, month, day, hour, minute, second, conf->returnkeylist[return_key].description, datastring, conf->returnkeylist[return_key].units );
                                               UpdateLiveList(flag, unit[0], "%s",  idate, conf->returnkeylist[return_key].description, -1.0, -1, datastring.c_str(), conf->returnkeylist[return_key].units, conf->returnkeylist[return_key].persistent, livedata);
                                               
					       break;
                                           }
      				           }
                                       }
                                       else
                                       {
                                           if( data[0]>0 )
				               fmt::printf("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS\n", year, month, day, hour, minute, second, (data+i+1)[0], (data+i+1)[1], currentpower_total );
                                           break;
                                       }
                                    }
                                    free( data );
				    break;
                                }
                                else
                                {
                                    //An Error has occurred
                                    break;
				}
			    case 31: // LOGIN Data
                                idate=ConvertStreamtoTime( received+59, 4, &idate, &day, &month, &year, &hour, &minute, &second );
                                if( flag->debug == 1) fmt::printf("Date power = %d/%d/%4d %02d:%02d:%02d\n",day, month, year, hour, minute,second);
			        if (flag->debug == 1) fmt::printf("extracting SUSyID=%02x:%02x\n", received[33], received[34]);
                                unit[0]->Serial[3]=received[35];
                                unit[0]->Serial[2]=received[36];
                                unit[0]->Serial[1]=received[37];
                                unit[0]->Serial[0]=received[38];
			        if (flag->verbose	== 1) fmt::printf( "serial=%02x:%02x:%02x:%02x\n",unit[0]->Serial[3]&0xff,unit[0]->Serial[2]&0xff,unit[0]->Serial[1]&0xff,unit[0]->Serial[0]&0xff );  
                                    //This is where we poll for other inverters
				inverter_serial=(unit[0]->Serial[0]<<24)+(unit[0]->Serial[1]<<16)+(unit[0]->Serial[2]<<8)+unit[0]->Serial[3];
                                sprintf( unit[0]->SerialStr, "%llu", inverter_serial ); 
                                //This is where we poll for other inverters
                                unit[0]->SUSyID[0]=received[33];
                                unit[0]->SUSyID[1]=received[34];
                                //This is where we poll for other inverters
			        break;
                    }
		}
				
		while (strcmp(lineread,"$END"));
 	}
           }
    } 
    return 0;
}
/*
 * Run a command on an inverter
 *
 */
void InverterCommand(const char* command, ConfType* conf, const FlagType* flag, UnitType** unit, const int s, FILE* fp,
                     std::vector<ArchDataType>& archdata, std::vector<LiveDataType>& livedata)
{
    int linenum;

    if (fseek( fp, 0L, 0 ) < 0 )
        fmt::printf( "\nError" );
    if(( linenum = GetLine( command, fp )) > 0 ) {
        if (ProcessCommand(conf, flag, unit, s, fp, &linenum, archdata, livedata) < 0) {
            fmt::printf( "\nError need to do something" ); getchar();
        }
    }
    else
    {
	//Command not found in config
	fmt::printf( "\nCommand %s not found", command );
    }    
}

int OpenInverter(ConfType* conf, const FlagType* flag, UnitType** unit, const int s, std::vector<ArchDataType>& archdata,
                 std::vector<LiveDataType>& livedata)
{
    FILE * fp;

    if (flag->file ==1)
        fp=fopen(conf->File,"r");
    else
        fp=fopen("/etc/sma.in","r");
    if( fp == NULL ) {
        //Can't open file
        perror( "Can't open conf file" );
        return -1;
    }
    InverterCommand("init", conf, flag, unit, s, fp, archdata, livedata);

    fclose(fp);
    return 0;
}


/*
 * Get Line number of the command required
 * reurn line number on success 0 on failure
 */
static int GetLine( const char * command, FILE * fp )
{
    char *line = NULL;
    size_t len=0;
    ssize_t read;
    char *lineread;
    int	 linenum=0;
    int  found=0;

    while (( read = getline(&line,&len,fp) ) != -1){	//read line from sma.in
	linenum++;
	lineread = strtok(line," ;");
	if(!strncmp(lineread,":", 1)){	//See if line is something we need to receive
            if( ! strcmp( lineread+1, command ) )
            {
                found=1;
                break;
            }
        }
    }
    if( !found ) linenum=0;
    free(line);
    return linenum;
}
