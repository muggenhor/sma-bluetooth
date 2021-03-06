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

/* compile gcc -lbluetooth -g -o smatool smatool.c */

#define _XOPEN_SOURCE 700 /* glibc needs this */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utility>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include "smatool.hpp"
#include "sb_commands.hpp"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/printf.h>

using namespace fmt::literals;

/*
 * u16 represents an unsigned 16-bit number.  Adjust the typedef for
 * your hardware.
 */
typedef u_int16_t u16;

#define PPPINITFCS16 0xffff /* Initial FCS value    */
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */
#define ASSERT(x) assert(x)

static const char* const accepted_strings[] = {
"$END",
"$ADDR",
"$TIME",
"$SERIAL",
"$CRC",
"$POW",
"$DTOT",
"$ADD2",
"$CHAN",
"$ITIME",
"$TMMI",
"$TMPL",
"$TIMESTRING",
"$TIMEFROM1",
"$TIMETO1",
"$TIMEFROM2",
"$TIMETO2",
"$TESTDATA",
"$ARCHIVEDATA1",
"$PASSWORD",
"$SIGNAL",
"$SUSYID",
"$INVCODE",
"$ARCHCODE",
"$INVERTERDATA",
"$CNT",        /*Counter of sent packets*/
"$TIMEZONE",    /*Timezone seconds +1 from GMT*/
"$TIMESET",    /*Unknown string involved in time setting*/
"$DATA",        /*Data string */
"$MYSUSYID",
"$MYSERIAL",
"$LOGIN"
};

static const u16 fcstab[256] = {
   0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
   0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
   0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
   0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
   0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
   0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
   0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
   0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
   0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
   0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
   0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
   0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
   0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
   0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
   0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
   0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
   0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
   0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
   0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
   0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
   0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
   0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
   0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
   0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
   0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
   0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
   0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
   0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
   0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
   0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
   0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
   0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*
 * Calculate a new fcs given the current fcs and the new data.
 */
u16 pppfcs16(u16 fcs, void *_cp, int len)
{
    register unsigned char *cp = (unsigned char *)_cp;
    /* don't worry about the efficiency of these asserts here.  gcc will
     * recognise that the asserted expressions are constant and remove them.
     * Whether they are usefull is another question. 
     */

    ASSERT(sizeof (u16) == 2);
    ASSERT(((u16) -1) > 0);
    while (len--)
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
    return (fcs);
}

/*
 * Strip escapes (7D) as they aren't includes in fcs
 */
void
strip_escapes(unsigned char *cp, int *len)
{
    int i,j;

    for( i=0; i<(*len); i++ ) {
      if( cp[i] == 0x7d ) { /*Found escape character. Need to convert*/
        cp[i] = cp[i+1]^0x20;
        for( j=i+1; j<(*len)-1; j++ ) cp[j] = cp[j+1];
        (*len)--;
      }
   }
}

int quick_pow10(int n)
{
    static int pow10[10] = {
        1, 10, 100, 1000, 10000, 
        100000, 1000000, 10000000, 100000000, 1000000000
    };

    return pow10[n]; 
}

/*
 * Add escapes (7D) as they are required
 */
void
add_escapes(unsigned char *cp, int *len)
{
    int i,j;

    for( i=19; i<(*len); i++ ) {
      switch( cp[i] ) {
        case 0x7d :
        case 0x7e :
        case 0x11 :
        case 0x12 :
        case 0x13 :
          for( j=(*len); j>i; j-- ) cp[j] = cp[j-1];
          cp[i+1] = cp[i]^0x20;
          cp[i]=0x7d;
          (*len)++;
      }
   }
}

/*
 * Recalculate and update length to correct for escapes
 */
void fix_length_send(const FlagType* flag, unsigned char* cp, int* len)
{
    if( flag->debug == 1 ) 
       fmt::printf( "sum=%x\n", cp[1]+cp[3] );
    if(( cp[1] != (*len)+1 ))
    {
      if( flag->debug == 1 ) {
          fmt::printf( "  length change from %x to %x diff=%x \n", cp[1],(*len)+1,cp[1]+cp[3] );
      }
      cp[3] = (cp[1]+cp[3])-((*len)+1);
      cp[1] =(*len)+1;

      cp[3]=cp[0]^cp[1]^cp[2];
      /*
      switch( cp[1] ) {
        case 0x3a: cp[3]=0x44; break;
        case 0x3b: cp[3]=0x43; break;
        case 0x3c: cp[3]=0x42; break;
        case 0x3d: cp[3]=0x41; break;
        case 0x3e: cp[3]=0x40; break;
        case 0x3f: cp[3]=0x41; break;
        case 0x40: cp[3]=0x3e; break;
        case 0x41: cp[3]=0x3f; break;
        case 0x42: cp[3]=0x3c; break;
        case 0x52: cp[3]=0x2c; break;
        case 0x53: cp[3]=0x2b; break;
        case 0x54: cp[3]=0x2a; break;
        case 0x55: cp[3]=0x29; break;
        case 0x56: cp[3]=0x28; break;
        case 0x57: cp[3]=0x27; break;
        case 0x58: cp[3]=0x26; break;
        case 0x59: cp[3]=0x25; break;
        case 0x5a: cp[3]=0x24; break;
        case 0x5b: cp[3]=0x23; break;
        case 0x5c: cp[3]=0x22; break;
        case 0x5d: cp[3]=0x23; break;
        case 0x5e: cp[3]=0x20; break;
        case 0x5f: cp[3]=0x21; break;
        case 0x60: cp[3]=0x1e; break;
        case 0x61: cp[3]=0x1f; break;
        case 0x62: cp[3]=0x1e; break;
        default: fmt::printf( "NO CONVERSION!" );getchar();break;
      }
      */
      if( flag->debug == 1 ) 
         fmt::printf( "new sum=%x\n", cp[1]+cp[3] );
    }
}
            
/*
 * Recalculate and update length to correct for escapes
 */
static void fix_length_received(const FlagType* flag, unsigned char* received, const int* len)
{
    if( received[1] != (*len) )
    {
      int sum = received[1]+received[3];
      if (flag->debug == 1) fmt::printf( "sum=%x", sum );
      if (flag->debug == 1) fmt::printf( "length change from %x to %x\n", received[1], (*len) );
      if(( received[3] != 0x13 )&&( received[3] != 0x14 )) { 
        received[1] = (*len);
        switch( received[1] ) {
          case 0x52: received[3]=0x2c; break;
          case 0x5a: received[3]=0x24; break;
          case 0x66: received[3]=0x1a; break;
          case 0x6a: received[3]=0x14; break;
          default:  received[3]=sum-received[1]; break;
        }
      }
    }
}

/*
 * How to use the fcs
 */
void tryfcs16(const FlagType* flag, const unsigned char* cp, int len, unsigned char* fl, int* cc)
{
    u16 trialfcs;
    unsigned char stripped[1024] = { 0 };

    memcpy( stripped, cp, len );
    /* add on output */
    if (flag->debug ==2){
 	fmt::printf("String to calculate FCS\n");	 
        	for (int i=0;i<len;i++) fmt::printf("%02x ",cp[i]);
	 	fmt::printf("\n\n");
    }	
    trialfcs = pppfcs16( PPPINITFCS16, stripped, len );
    trialfcs ^= 0xffff;               /* complement */
    fl[(*cc)] = (trialfcs & 0x00ff);    /* least significant byte first */
    fl[(*cc)+1] = ((trialfcs >> 8) & 0x00ff);
    (*cc)+=2;
    if (flag->debug == 2 ){ 
	fmt::printf("FCS = %x%x %x\n",(trialfcs & 0x00ff),((trialfcs >> 8) & 0x00ff), trialfcs); 
    }
}

unsigned char conv(const char* nn)
{
    unsigned char tt=0,res=0;
    int i;   

    for(i=0;i<2;i++){
        switch(nn[i]){

            case 'A':
            case 'a':
                tt = 10;
                break;

            case 'B':
            case 'b':
                tt = 11;
                break;

            case 'C':
            case 'c':
                tt = 12;
                break;

            case 'D':
            case 'd':
                tt = 13;
                break;

            case 'E':
            case 'e':
                tt = 14;
                break;

            case 'F':
            case 'f':
                tt = 15;
                break;


            default:
                tt = nn[i] - 48;
        }
        res = res + (tt * pow(16,1-i));
    }
    return res;
}

int check_send_error(const FlagType* flag, const int s, int* rr, unsigned char* received, int cc,
                     const std::vector<unsigned char>& last_sent, int* terminated, int* already_read)
{
    int bytes_read,j;
    unsigned char buf[1024]; /*read buffer*/
    unsigned char header[3]; /*read buffer*/
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 0; // set timeout of reading
    tv.tv_usec = 5000;
    memset(buf,0,1024);

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
				
    select(s+1, &readfds, NULL, NULL, &tv);
				
    (*terminated) = 0; // Tag to tell if string has 7e termination
    // first read the header to get the record length
    if (FD_ISSET(s, &readfds)){	// did we receive anything within 5 seconds
        bytes_read = recv(s, header, sizeof(header), 0); //Get length of string
	(*rr) = 0;
        for (size_t i = 0; i < sizeof(header); i++) {
            received[(*rr)] = header[i];
	    if (flag->debug == 1) fmt::printf("%02x ", received[(*rr)]);
            (*rr)++;
        }
    }
    else
    {
       if( flag->verbose==1) fmt::printf("Timeout reading bluetooth socket\n");
       (*rr) = 0;
       memset(received,0,1024);
       return -1;
    }
    if (FD_ISSET(s, &readfds)){	// did we receive anything within 5 seconds
        bytes_read = recv(s, buf, header[1]-3, 0); //Read the length specified by header
    }
    else
    {
       if( flag->verbose==1) fmt::printf("Timeout reading bluetooth socket\n");
       (*rr) = 0;
       memset(received,0,1024);
       return -1;
    }
    if ( bytes_read > 0){
	if (flag->debug == 1){ 
           fmt::printf("\nReceiving\n");
           fmt::printf( "    %08x: .. .. .. .. .. .. .. .. .. .. .. .. ", 0 );
           j=12;
           for (size_t i = 0; i < sizeof(header); i++) {
              if( j%16== 0 )
                 fmt::printf( "\n    %08x: ",j);
              fmt::printf("%02x ",header[i]);
              j++;
           }
	   for (int i = 0; i < bytes_read; i++) {
              if( j%16== 0 )
                 fmt::printf( "\n    %08x: ",j);
              fmt::printf("%02x ",buf[i]);
              j++;
           }
           fmt::printf(" rr=%d",(bytes_read+(*rr)));
	   fmt::printf("\n\n");
        }
        if ((std::size_t)bytes_read == last_sent.size()
         && std::equal(received, received + cc, begin(last_sent)))
        {
           fmt::printf( "ERROR received what we sent!" ); getchar();
           //Need to do something
        }
        if( buf[ bytes_read-1 ] == 0x7e )
           (*terminated) = 1;
        else
           (*terminated) = 0;
        for (int i = 0; i < bytes_read; i++) { //start copy the rec buffer in to received
            if (buf[i] == 0x7d){ //did we receive the escape char
	        switch (buf[i+1]){   // act depending on the char after the escape char
					
		    case 0x5e :
	                received[(*rr)] = 0x7e;
		        break;
							   
		    case 0x5d :
		        received[(*rr)] = 0x7d;
		        break;
							
		    default :
		        received[(*rr)] = buf[i+1] ^ 0x20;
		        break;
	        }
		    i++;
	    }
	    else { 
               received[(*rr)] = buf[i];
            }
	    if (flag->debug == 1) fmt::printf("%02x ", received[(*rr)]);
	    (*rr)++;
	}
        fix_length_received( flag, received, rr );
	if (flag->debug == 1) {
	    fmt::printf("\n");
            for (int i = 0; i < *rr; i++) fmt::printf("%02x ", received[(i)]);
        }
	if (flag->debug == 1) fmt::printf("\n\n");
        (*already_read)=1;
    }	
    return 0;
}

int empty_read_bluetooth(const FlagType* flag, ReadRecordType* readRecord, const int s, int* rr, unsigned char* received,
                         int* terminated)
{
    int bytes_read,j, last_decoded;
    unsigned char buf[1024]; /*read buffer*/
    unsigned char header[4]; /*read buffer*/
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 1; // set timeout of reading
    tv.tv_usec = 0;
    memset(buf,0,1024);

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
				
    if( select(s+1, &readfds, NULL, NULL, &tv) <  0) {
        fmt::printf( "select error has occurred" ); getchar();
    }

				
    (*terminated) = 0; // Tag to tell if string has 7e termination
    // first read the header to get the record length
    if (FD_ISSET(s, &readfds)){	// did we receive anything within 5 seconds
        bytes_read = recv(s, header, sizeof(header), 0); //Get length of string
	(*rr) = 0;
        for (size_t i = 0; i < sizeof(header); i++) {
            received[(*rr)] = header[i];
	    if (flag->debug == 2) fmt::printf("%02x ", received[i]);
            (*rr)++;
        }
    }
    else
    {
       memset(received,0,1024);
       (*rr)=0;
       return -1;
    }
    if (FD_ISSET(s, &readfds)){	// did we receive anything within 5 seconds
        bytes_read = recv(s, buf, header[1]-3, 0); //Read the length specified by header
    }
    else
    {
       memset(received,0,1024);
       (*rr)=0;
       return -1;
    }
    readRecord->Status[0]=0;
    readRecord->Status[1]=0;
    if ( bytes_read > 0){
	if (flag->debug == 1){ 
           /*
           fmt::printf("\nReceiving\n");
           fmt::printf( "    %08x: .. .. .. .. .. .. .. .. .. .. .. .. ", 0 );
           j=12;
           for( i=0; i<sizeof(header); i++ ) {
              if( j%16== 0 )
                 fmt::printf( "\n    %08x: ",j);
              fmt::printf("%02x ",header[i]);
              j++;
           }
	   for (i=0;i<bytes_read;i++) {
              if( j%16== 0 )
                 fmt::printf( "\n    %08x: ",j);
              fmt::printf("%02x ",buf[i]);
              j++;
           }
           fmt::printf(" rr=%d",(bytes_read+(*rr)));
	   fmt::printf("\n\n");
           */
           fmt::printf( "\n-----------------------------------------------------------" );
           fmt::printf( "\nREAD:");
           //Start byte
           fmt::printf("\n7e "); j++;
           //Size and checkbit
           fmt::printf("%02x ",header[1]);
           fmt::printf("                      size:              %d", header[1] );
           fmt::printf("\n   " );
           fmt::printf("%02x ",header[2]);
           fmt::printf("\n   " );
           fmt::printf("%02x ",header[3]);
           fmt::printf("                      checkbit:          %d", header[3] );
           fmt::printf("\n   " );
           //Source Address
           for (int i = 0; i < bytes_read; i++) {
              if( i > 5 ) break;
              fmt::printf("%02x ",buf[i]);
           }
           fmt::printf("       source:            %02x:%02x:%02x:%02x:%02x:%02x", buf[5], buf[4], buf[3], buf[2], buf[1], buf[0] );
           fmt::printf("\n   " );
           //Destination Address
           for (int i = 6; i < bytes_read; i++) {
              if( i > 11 ) break;
              fmt::printf("%02x ",buf[i]);
           }
           fmt::printf("       destination:       %02x:%02x:%02x:%02x:%02x:%02x", buf[11], buf[10], buf[9], buf[8], buf[7], buf[6] );
           fmt::printf("\n   " );
           //Destination Address
           for (int i = 12; i < bytes_read; i++) {
              if( i > 13 ) break;
              fmt::printf("%02x ",buf[i]);
           }
           fmt::printf("                   control:           %02x%02x", buf[13], buf[12] );
           readRecord->Control[0]=buf[12];
           readRecord->Control[1]=buf[13];
           
           last_decoded=14;
           if( memcmp( buf+14, "\x7e\xff\x03\x60\x65", 5 ) == 0 ){
               fmt::printf("\n");
               for (int i = 14; i < bytes_read; i++) {
                   if( i > 18 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("             SMA Data2+ header: %02x:%02x:%02x:%02x:%02x", buf[18], buf[17], buf[16], buf[15], buf[14] );
               fmt::printf("\n   " );
               for (int  i = 19; i < bytes_read; i++) {
                   if( i > 19 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                      data packet size:  %02d", buf[19] );
               fmt::printf("\n   " );
               for (int i = 20; i < bytes_read; i++) {
                   if( i > 20 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                      control:           %02x", buf[20] );
               fmt::printf("\n   " );
               for (int i = 21; i < bytes_read; i++) {
                   if( i > 26 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("       source:            %02x %02x:%02x:%02x:%02x:%02x", buf[21], buf[26], buf[25], buf[24], buf[23], buf[22] );
               fmt::printf("\n   " );
               for (int i = 27; i < bytes_read; i++) {
                   if( i > 28 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                   read status:       %02x %02x", buf[28], buf[27] );
               fmt::printf("\n   " );
               for (int i = 29; i < bytes_read; i++) {
                   if( i > 30 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               readRecord->Status[0]=buf[28];
               readRecord->Status[1]=buf[27];
               fmt::printf("                   count up:          %02d %02x:%02x", buf[29]+buf[30]*256, buf[30], buf[29] );
               fmt::printf("\n   " );
               for (int i = 31; i < bytes_read; i++) {
                   if( i > 32 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                   count down:        %02d %02x:%02x", buf[31]+buf[32]*256, buf[32], buf[31] );
               fmt::printf("\n   " );
               last_decoded=33;
           }
           fmt::printf("\n   " );
           j=0;
	   for (int i = last_decoded; i < bytes_read; i++) {
              if( j%16== 0 )
                 fmt::printf( "\n   %08x: ",j);
              fmt::printf("%02x ",buf[i]);
              j++;
           }
           fmt::printf(" rr=%d",(bytes_read+3));
	   fmt::printf("\n\n");
        }
           

 
    }	
    (*rr)=0;
    memset(received,0,1024);
    return 0;
}
int read_bluetooth(const ConfType* conf, const FlagType* flag, ReadRecordType* readRecord, const int s, int* rr,
                   unsigned char* received, const std::vector<unsigned char>& last_sent, int* terminated)
{
    int bytes_read,j, last_decoded;
    unsigned char buf[1024]; /*read buffer*/
    unsigned char header[4]; /*read buffer*/
    unsigned char checkbit;
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = conf->bt_timeout; // set timeout of reading
    tv.tv_usec = 0;
    memset(buf,0,1024);

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
				
    if( select(s+1, &readfds, NULL, NULL, &tv) <  0) {
        fmt::printf( "select error has occurred" ); getchar();
    }
				
    if( flag->verbose==1) fmt::printf("Reading bluetooth packett\n");
    if( flag->verbose==1) fmt::printf("socket=%d\n", s);
    (*terminated) = 0; // Tag to tell if string has 7e termination
    // first read the header to get the record length
    if (FD_ISSET(s, &readfds)){	// did we receive anything within 5 seconds
        bytes_read = recv(s, header, sizeof(header), 0); //Get length of string
	(*rr) = 0;
        for (size_t i = 0; i < sizeof(header); i++) {
            received[(*rr)] = header[i];
	    if (flag->debug == 2) fmt::printf("%02x ", received[i]);
            (*rr)++;
        }
    }
    else
    {
       if( flag->verbose==1) fmt::printf("Timeout reading bluetooth socket\n");
       (*rr) = 0;
       memset(received,0,1024);
       return -1;
    }
    if (FD_ISSET(s, &readfds)){	// did we receive anything within 5 seconds
        bytes_read = recv(s, buf, header[1]-3, 0); //Read the length specified by header
    }
    else
    {
       if( flag->verbose==1) fmt::printf("Timeout reading bluetooth socket\n");
       (*rr) = 0;
       memset(received,0,1024);
       return -1;
    }
    readRecord->Status[0]=0;
    readRecord->Status[1]=0;
    if ( bytes_read > 0){
	if (flag->debug == 1){ 
           fmt::printf( "\n-----------------------------------------------------------" );
           fmt::printf( "\nREAD:");
           //Start byte
           fmt::printf("\n7e "); j++;
           //Size and checkbit
           fmt::printf("%02x ",header[1]);
           fmt::printf("                      size:              %d", header[1] );
           fmt::printf("\n   " );
           fmt::printf("%02x ",header[2]);
           fmt::printf("\n   " );
           fmt::printf("%02x ",header[3]);
           fmt::printf("                      checkbit:          %d", header[3] );
           fmt::printf("\n   " );
           //Source Address
           for (int i = 0; i < bytes_read; i++) {
              if( i > 5 ) break;
              fmt::printf("%02x ",buf[i]);
           }
           fmt::printf("       source:            %02x:%02x:%02x:%02x:%02x:%02x", buf[5], buf[4], buf[3], buf[2], buf[1], buf[0] );
           fmt::printf("\n   " );
           //Destination Address
           for (int i = 6; i < bytes_read; i++) {
              if( i > 11 ) break;
              fmt::printf("%02x ",buf[i]);
           }
           fmt::printf("       destination:       %02x:%02x:%02x:%02x:%02x:%02x", buf[11], buf[10], buf[9], buf[8], buf[7], buf[6] );
           fmt::printf("\n   " );
           //Destination Address
           for (int i = 12; i < bytes_read; i++) {
              if( i > 13 ) break;
              fmt::printf("%02x ",buf[i]);
           }
           fmt::printf("                   control:           %02x%02x", buf[13], buf[12] );
           readRecord->Control[0]=buf[12];
           readRecord->Control[1]=buf[13];
           
           last_decoded=14;
           if( memcmp( buf+14, "\x7e\xff\x03\x60\x65", 5 ) == 0 ){
               fmt::printf("\n");
               for (int i = 14; i < bytes_read; i++) {
                   if( i > 18 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("             SMA Data2+ header: %02x:%02x:%02x:%02x:%02x", buf[18], buf[17], buf[16], buf[15], buf[14] );
               fmt::printf("\n   " );
               for (int i = 19; i < bytes_read; i++) {
                   if( i > 19 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                      data packet size:  %02d", buf[19] );
               fmt::printf("\n   " );
               for (int i = 20; i < bytes_read; i++) {
                   if( i > 20 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                      control:           %02x", buf[20] );
               fmt::printf("\n   " );
               for (int i = 21; i < bytes_read; i++) {
                   if( i > 26 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("       source:            %02x %02x:%02x:%02x:%02x:%02x", buf[21], buf[26], buf[25], buf[24], buf[23], buf[22] );
               fmt::printf("\n   " );
               for (int i = 27; i < bytes_read; i++) {
                   if( i > 28 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                   read status:       %02x %02x", buf[28], buf[27] );
               fmt::printf("\n   " );
               for (int i = 29; i < bytes_read; i++) {
                   if( i > 30 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               readRecord->Status[0]=buf[28];
               readRecord->Status[1]=buf[27];
               fmt::printf("                   count up:          %02d %02x:%02x", buf[29]+buf[30]*256, buf[30], buf[29] );
               fmt::printf("\n   " );
               for (int i = 31; i < bytes_read; i++) {
                   if( i > 32 ) break;
                   fmt::printf("%02x ",buf[i]);
               }
               fmt::printf("                   count down:        %02d %02x:%02x", buf[31]+buf[32]*256, buf[32], buf[31] );
               fmt::printf("\n   " );
               last_decoded=33;
           }
           fmt::printf("\n   " );
           j=0;
	   for (int i = last_decoded; i < bytes_read; i++) {
              if( j%16== 0 )
                 fmt::printf( "\n   %08x: ",j);
              fmt::printf("%02x ",buf[i]);
              j++;
           }
           fmt::printf(" rr=%d",(bytes_read+3));
	   fmt::printf("\n\n");
        }
           

 
        if ((std::size_t)bytes_read == last_sent.size()
         && std::equal(received, received + bytes_read, begin(last_sent)))
        {
           fmt::printf( "ERROR received what we sent!" ); getchar();
           //Need to do something
        }
        // Check check bit
        checkbit=header[0]^header[1]^header[2];
        if( checkbit != header[3] )
        {
            fmt::printf("\nCheckbit Error! %02x!=%02x\n",  header[0]^header[1]^header[2], header[3]);
            (*rr) = 0;
            memset(received,0,1024);
            return -1;
        }
        if( buf[ bytes_read-1 ] == 0x7e )
           (*terminated) = 1;
        else
           (*terminated) = 0;
        for (int i = 0; i < bytes_read; i++){ //start copy the rec buffer in to received
            if (buf[i] == 0x7d){ //did we receive the escape char
	        switch (buf[i+1]){   // act depending on the char after the escape char
					
		    case 0x5e :
	                received[(*rr)] = 0x7e;
		        break;
							   
		    case 0x5d :
		        received[(*rr)] = 0x7d;
		        break;
							
		    default :
		        received[(*rr)] = buf[i+1] ^ 0x20;
		        break;
	        }
		    i++;
	    }
	    else { 
               received[(*rr)] = buf[i];
            }
	    if (flag->debug == 2) fmt::printf("%02x ", received[(*rr)]);
	    (*rr)++;
	}
        fix_length_received( flag, received, rr );
	if (flag->debug == 2) {
	    fmt::printf("\n");
            for (int i = 0; i < *rr; i++) fmt::printf("%02x ", received[(i)]);
        }
	if (flag->debug == 1) fmt::printf("\n\n");
    }	
    return 0;
}

int select_str(const char *s)
{
    for (size_t i = 0; i < sizeof(accepted_strings) / sizeof(*accepted_strings); i++)
    {
       //fmt::printf( "\ni=%d accepted=%s string=%s", i, accepted_strings[i], s );
       if (!strcmp(s, accepted_strings[i])) return i;
    }
    return -1;
}

unsigned char *  get_timezone_in_seconds( FlagType * flag, unsigned char *tzhex )
{
   time_t curtime;
   struct tm *loctime;
   struct tm *utctime;
   int day,month,year,hour,minute,isdst;

   float localOffset;
   int	 tzsecs;

   curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
   loctime = localtime(&curtime);
   day = loctime->tm_mday;
   month = loctime->tm_mon +1;
   year = loctime->tm_year + 1900;
   hour = loctime->tm_hour;
   minute = loctime->tm_min; 
   isdst  = loctime->tm_isdst;
   utctime = gmtime(&curtime);
   

   if( flag->debug == 1 ) fmt::printf( "utc=%04d-%02d-%02d %02d:%02d local=%04d-%02d-%02d %02d:%02d diff %d hours\n", utctime->tm_year+1900, utctime->tm_mon+1,utctime->tm_mday,utctime->tm_hour,utctime->tm_min, year, month, day, hour, minute, hour-utctime->tm_hour );
   localOffset=(hour-utctime->tm_hour)+(float)(minute-utctime->tm_min)/60;
   if( flag->debug == 1 ) fmt::printf( "localOffset=%f\n", localOffset );
   if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
      localOffset+=24;
   if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
      localOffset-=24;
   if( flag->debug == 1 ) fmt::printf( "localOffset=%f isdst=%d\n", localOffset, isdst );
   if( isdst > 0 ) 
       localOffset=localOffset-1;
   tzsecs = (localOffset) * 3600 + 1;
   if( tzsecs < 0 )
       tzsecs=65536+tzsecs;
   if( flag->debug == 1 ) fmt::printf( "tzsecs=%x %d\n", tzsecs, tzsecs );
   tzhex[1] = tzsecs/256;
   tzhex[0] = tzsecs -(tzsecs/256)*256;
   if( flag->debug == 1 ) fmt::printf( "tzsecs=%02x %02x\n", tzhex[1], tzhex[0] );

   return tzhex;
}

//Set a value depending on inverter
void SetInverterType(ConfType * conf, UnitType& unit)
{
    srand(time(NULL));
    unit.SUSyID[0] = 0xFF;
    unit.SUSyID[1] = 0xFF;
    conf->MySUSyID[0] = rand()%254;
    conf->MySUSyID[1] = rand()%254;
    conf->MySerial[0] = rand()%254;
    conf->MySerial[1] = rand()%254;
    conf->MySerial[2] = rand()%254;
    conf->MySerial[3] = rand()%254;
}

//Convert a recieved string to a value
float ConvertStreamtoFloat(const unsigned char * stream, int length)
{
   float value = 0.f;
   bool nullvalue = true;

   for (int i = 0; i < length; ++i)
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = false;
      value += stream[i] * pow(256, i);
   }
   if (nullvalue)
      return 0.f; //Asigning null to 0 at this stage unless it breaks something
   return value;
}

//Convert a recieved string to a value
std::string ConvertStreamtoString(const unsigned char* stream, int length)
{
   bool nullvalue = true;

   std::string value;
   value.reserve(length);
   for (int i = 0; i < length; ++i)
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = false;
      if( stream[i] != 0 ) {
        value.push_back(stream[i]);
      }
   }
   if (nullvalue)
      value.clear(); //Asigning empty at this stage unless it breaks something
   return value;
}
//read return value data from init file
static std::vector<ReturnType> InitReturnKeys(ConfType * conf)
{
   FILE		*fp;
   char		line[400];
   ReturnType   tmp;
   std::vector<ReturnType> returnkeys;
   int		data_follows;

   data_follows = 0;

   fp=fopen(conf->File,"r");
   if( fp == NULL ) {
       fmt::printf( "\nCouldn't open file %s", conf->File );
       fmt::printf( "\nerror=%s\n", strerror( errno ));
       exit(1);
   }
   else {

      while (!feof(fp)){	
         if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
            if( line[0] != '#' ) 
            {
                if( strncmp( line, ":unit conversions", 17 ) == 0 )
                    data_follows = 1;
                if( strncmp( line, ":end unit conversions", 21 ) == 0 )
                    data_follows = 0;
                if( data_follows == 1 ) {
                    tmp.key1=0x0;
                    tmp.key2=0x0;
                    strcpy( tmp.description, "" ); //Null out value
                    strcpy( tmp.units, "" ); //Null out value
                    tmp.divisor=0;
                    tmp.decimal=0;
                    tmp.datalength=0;
                    tmp.recordgap=0;
                    tmp.persistent=1;
                    if( sscanf( line, "%x %x \"%[^\"]\" \"%[^\"]\" %d %d %d %d", &tmp.key1, &tmp.key2, tmp.description, tmp.units, &tmp.decimal, &tmp.recordgap, &tmp.datalength, &tmp.persistent ) == 8 ) {
                      ReturnType returnkey;
                      returnkey.key1=tmp.key1;
                      returnkey.key2=tmp.key2;
                      strcpy( returnkey.description, tmp.description );
                      strcpy( returnkey.units, tmp.units );
                      returnkey.decimal = tmp.decimal;
                      returnkey.divisor = (float)pow( 10, tmp.decimal );
                      returnkey.datalength = tmp.datalength;
                      returnkey.recordgap = tmp.recordgap;
                      returnkey.persistent = tmp.persistent;
                      returnkeys.push_back(std::move(returnkey));
                    }
                    else
                    {
                        if( line[0] != ':' )
                        {
                             fmt::printf( "\nWarning Data Scan Failure\n %s\n", line ); getchar();
                        }
                    }
                }
            }
         }
      }
   fclose(fp);
   }
   return returnkeys;
}

//Convert a recieved string to a value
int ConvertStreamtoInt(const unsigned char* stream, int length)
{
   int value = 0;
   bool nullvalue = true;

   for (int i = 0; i < length; ++i)
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = false;
      value += stream[i] << (8 * i);
   }
   if (nullvalue)
      return 0; //Asigning null to 0 at this stage unless it breaks something
   return value;
}

//Convert a recieved string to a value
time_t ConvertStreamtoTime(const unsigned char* stream, int length, int* day, int* month, int* year,
                           int* hour, int* minute, int* second)
{
   time_t value = 0;
   bool nullvalue = true;

   for (int i = 0; i < length; ++i) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = false;
      value += (time_t)stream[i] << (8 * i);
   }
   if (nullvalue)
      return 0; //Asigning null to 0 at this stage unless it breaks something
   else
   {
      //Get human readable dates
      auto loctime = localtime(&value);
      (*day) = loctime->tm_mday;
      (*month) = loctime->tm_mon +1;
      (*year) = loctime->tm_year + 1900;
      (*hour) = loctime->tm_hour;
      (*minute) = loctime->tm_min; 
      (*second) = loctime->tm_sec; 
   }
   return value;
}

// Set switches to save lots of strcmps
static void SetSwitches(const ConfType* conf, FlagType* flag)
{
    //Check if all File variables are set
    if(( strlen(conf->datefrom) > 0 )
	 &&( strlen(conf->dateto) > 0 ))
        flag->daterange=1;
    else
        flag->daterange=0;
}

std::vector<unsigned char> ReadStream(const ConfType* conf, const FlagType* flag, ReadRecordType* readRecord, const int s,
                          unsigned char* stream, int* streamlen, const std::vector<unsigned char>& last_sent,
                          int* terminated, int* togo)
{
   int	finished;
   int	finished_record;
   int  i;

   (*togo) = ConvertStreamtoInt(stream + 43, 2);
   if( flag->debug==1 ) fmt::printf( "togo=%d\n", (*togo) );
   i=59; //Initial position of data stream
   std::vector<unsigned char> data;
   finished=0;
   finished_record=0;
   while( finished != 1 ) {
     while( finished_record != 1 ) {
        if( i> 500 ) break; //Somthing has gone wrong
        
        if(( i < (*streamlen) )&&(( (*terminated) != 1)||(i+3 < (*streamlen) ))) 
	{
           data.push_back(stream[i]);
           i++;
        }
        else
           finished_record = 1;
           
     }
     finished_record = 0;
     if( (*terminated) == 0 )
     {
         if (read_bluetooth(conf, flag, readRecord, s, streamlen, stream, last_sent, terminated) != 0)
         {
           data.clear();
         }
             
         if (!data.empty())
           i = 18;
     }
     else
         finished = 1;
   }
   if( flag->debug== 1 ) {
     fmt::printf("len=%d data=", data.size());
     for (const auto c : data)
       fmt::printf("%02x ", c);
     fmt::printf( "\n" );
   }
   return data;
}

/* Init Config to default values */
static void InitConfig(ConfType* conf)
{
    strcpy(conf->Config, "/etc/smatool.conf");
    strcpy( conf->BTAddress, "" );  
    conf->bt_timeout = 30;  
    strcpy( conf->Password, "0000" );  
    strcpy(conf->File, "/etc/sma.in");
    strcpy( conf->datefrom, "" );  
    strcpy( conf->dateto, "" );  
}

/* Init Flagsg to default values */
static void InitFlag(FlagType* flag)
{
    flag->debug=0;         /* debug flag */
    flag->verbose=0;       /* verbose flag */
    flag->daterange=0;     /* is system using a daterange */
    flag->test=0;     /* is system using a daterange */
}

/* read Config from file */
static int GetConfig(ConfType* conf, const FlagType* flag)
{
    char	line[400];
    char	variable[400];
    char	value[400];

  auto fp = fopen(conf->Config, "r");
  if (!fp)
  {
    fmt::printf("Error! Could not open file %s\n", conf->Config);
    return -1; // Could not open file
  }
    while (!feof(fp)){	
	if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
            if( line[0] != '#' ) 
            {
                strcpy( value, "" ); //Null out value
                sscanf( line, "%s %s", variable, value );
                if( flag->debug == 1 ) fmt::printf( "variable=%s value=%s\n", variable, value );
                if( value[0] != '\0' )
                {
                    if( strcmp( variable, "BTAddress" ) == 0 )
                       strcpy( conf->BTAddress, value );  
                    if( strcmp( variable, "BTTimeout" ) == 0 )
                       conf->bt_timeout =  atoi(value);  
                    if( strcmp( variable, "Password" ) == 0 )
                       strcpy( conf->Password, value );  
                    if( strcmp( variable, "File" ) == 0 )
                    {
                      if (value[0] == '/')
                      {
                        conf->File[0] = '\0';
                      }
                      else
                      {
                        // Make relative paths relative to the config file's directory
                        strcpy(conf->File, conf->Config);
                        if (auto sep = strrchr(conf->File, '/'))
                        {
                          sep[1] = '\0';
                        }
                        else
                        {
                          // config file is loaded from the current working directory, do the same for this one
                          conf->File[0] = '\0';
                        }
                      }
                      strcat(conf->File, value);
                    }
                }
            }
        }
    }
    fclose( fp );
    return( 0 );
}

/* read  Inverter Settings from file */
int GetInverterSetting(const ConfType* conf, const FlagType* flag)
{
    FILE 	*fp;
    char	line[400];
    char	variable[400];
    char	value[400];

    if (strlen(conf->Setting) > 0 )
    {
        if(( fp=fopen(conf->Setting,"r")) == (FILE *)NULL )
        {
           fmt::printf( "Error! Could not open file %s\n", conf->Setting );
           return( -1 ); //Could not open file
        }
    }
    else
    {
        if(( fp=fopen("./invcode.in","r")) == (FILE *)NULL )
        {
           fmt::printf( "Error! Could not open file ./invcode.in\n" );
           return( -1 ); //Could not open file
        }
    }
    while (!feof(fp)){	
	if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
            if( line[0] != '#' ) 
            {
                strcpy( value, "" ); //Null out value
                sscanf( line, "%s %s", variable, value );
                if( flag->debug == 1 ) fmt::printf( "variable=%s value=%s\n", variable, value );
                if( value[0] != '\0' )
                {
                    /*
                    int found_inverter=0;
                    if( strcmp( variable, "Inverter" ) == 0 )
                    {
                       if( strcmp( value, conf->Inverter ) == 0 )
                          found_inverter = 1;
                       else
                          found_inverter = 0;
                    }
                    if(( strcmp( variable, "Code1" ) == 0 )&& found_inverter )
                    {
                       sscanf( value, "%X", &conf->InverterCode[0] );
                    }
                    if(( strcmp( variable, "Code2" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->InverterCode[1] );
                    if(( strcmp( variable, "Code3" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->InverterCode[2] );
                    if(( strcmp( variable, "Code4" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->InverterCode[3] );
                    if(( strcmp( variable, "InvCode" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->ArchiveCode );
*/
                }
            }
        }
    }
    fclose( fp );
    return( 0 );
}

const char* return_sma_description(int index)
{
    static const std::pair<int, const char*> values[] = {
#include "sma_map.ipp"
    };
    auto i = std::lower_bound(std::begin(values), std::end(values), index,
            [] (decltype(values[0])& lhs, const int rhs) {
            return lhs.first < rhs;
        });
    if (i == std::end(values)
     || i->first != index)
    {
        fmt::printf("Failed to find XML data for idx %d\n", index);
        exit(3);
     }
    return i->second;
}

/* Print a help message */
void PrintHelp()
{
    fmt::printf( "Usage: smatool [OPTION]\n" );
    fmt::printf( "  -v,  --verbose                           Give more verbose output\n" );
    fmt::printf( "  -d,  --debug                             Show debug\n" );
    fmt::printf( "  -c,  --config CONFIGFILE                 Set config file default smatool.conf\n" );
    fmt::printf( "       --test                              Run in test mode - don't update data\n" );
    fmt::printf( "\n" );
    fmt::printf( "  -from  --datefrom YYYY-MM-DD HH:MM:00    Date range from date\n" );
    fmt::printf( "  -to  --dateto YYYY-MM-DD HH:MM:00        Date range to date\n" );
    fmt::printf( "\n" );
    fmt::printf( "The following options are in config file but may be overridden\n" );
    fmt::printf( "  -i,  --inverter INVERTER_MODEL           inverter model\n" );
    fmt::printf( "  -a,  --address INVERTER_ADDRESS          inverter BT address\n" );
    fmt::printf( "  -t,  --timeout TIMEOUT                   bluetooth timeout (secs) default 5\n" );
    fmt::printf( "  -p,  --password PASSWORD                 inverter user password default 0000\n" );
    fmt::printf( "  -f,  --file FILENAME                     command file default sma.in.new\n" );
    fmt::printf( "\n\n" );
}

/* Init Config to default values */
int ReadCommandConfig( ConfType *conf, FlagType *flag, int argc, char **argv)
{
    int	i;

    // these need validation checking at some stage TODO
    for (i=1;i<argc;i++)			//Read through passed arguments
    {
	if ((strcmp(argv[i],"-v")==0)||(strcmp(argv[i],"--verbose")==0)) flag->verbose = 1;
	else if ((strcmp(argv[i],"-d")==0)||(strcmp(argv[i],"--debug")==0)) flag->debug = 1;
	else if ((strcmp(argv[i],"-c")==0)||(strcmp(argv[i],"--config")==0)){
	    i++;
	    if(i<argc){
		strcpy(conf->Config,argv[i]);
	    }
	}
	else if (strcmp(argv[i],"--test")==0) flag->test=1;
	else if ((strcmp(argv[i],"-from")==0)||(strcmp(argv[i],"--datefrom")==0)){
	    i++;
	    if(i<argc){
		strcpy(conf->datefrom,argv[i]);
	    }
	}
	else if ((strcmp(argv[i],"-to")==0)||(strcmp(argv[i],"--dateto")==0)){
	    i++;
	    if(i<argc){
		strcpy(conf->dateto,argv[i]);
	    }
	}
        else if ((strcmp(argv[i],"-a")==0)||(strcmp(argv[i],"--address")==0)){
            i++;
            if (i<argc){
	        strcpy(conf->BTAddress,argv[i]);
            }
	}
        else if ((strcmp(argv[i],"-t")==0)||(strcmp(argv[i],"--timeout")==0)){
            i++;
            if (i<argc){
	        conf->bt_timeout = atoi(argv[i]);
            }
	}
        else if ((strcmp(argv[i],"-p")==0)||(strcmp(argv[i],"--password")==0)){
            i++;
            if (i<argc){
	        strcpy(conf->Password,argv[i]);
            }
	}
        else if ((strcmp(argv[i],"-f")==0)||(strcmp(argv[i],"--file")==0)){
            i++;
            if (i<argc){
	        strcpy(conf->File,argv[i]);
            }
	}
	else if ((strcmp(argv[i],"-h")==0) || (strcmp(argv[i],"--help") == 0 ))
        {
           PrintHelp();
           return( -1 );
        }
        else
        {
           fmt::printf("Bad Syntax\n\n" );
           for( i=0; i< argc; i++ )
             fmt::printf( "%s ", argv[i] );
           fmt::printf( "\n\n" );
          
           PrintHelp();
           return( -1 );
        }
    }
    return( 0 );
}

std::string debugdate()
{
    return "{:%Y-%m-%d %H:%M:%S}"_format(fmt::localtime(std::time(nullptr)));
}

int main(int argc, char **argv)
{
    ConfType 		conf;
    FlagType 		flag;
    UnitType 		unit;
    unsigned char 	tzhex[2] = { 0 };
    std::vector<ArchDataType> archdata;
    std::vector<LiveDataType> livedata;

    /* get the report time - used in various places */
   
    // set config to defaults
    InitConfig( &conf );
    InitFlag( &flag );
    // read command arguments needed so can get config
    if( ReadCommandConfig( &conf, &flag, argc, argv) < 0 )
        exit(0);
    // read Config file
    if( GetConfig( &conf, &flag ) < 0 )
        exit(-1);
    // read command arguments  again - they overide config
    if( ReadCommandConfig( &conf, &flag, argc, argv) < 0 )
        exit(0);
    // read Inverter Setting file
    //if( GetInverterSetting( &conf ) < 0 )
      //  exit(-1);
    // set switches used through the program
    SetSwitches( &conf, &flag );  
    // Get Return Value lookup from file
    conf.returnkeys = InitReturnKeys(&conf);
    // Set value for inverter type
    
    SetInverterType(&conf, unit);
    // Get Local Timezone offset in seconds
    get_timezone_in_seconds( &flag, tzhex );
    if( flag.verbose == 1 ) fmt::printf( "QUERY RANGE    from %s to %s\n", conf.datefrom, conf.dateto ); 
    if (flag.debug ==1) fmt::printf("Address %s\n",conf.BTAddress);
    //Connect to Inverter
    const int s = ConnectSocket(&conf);
    if (s == -1)
       exit(-1);

    auto fp = fopen(conf.File,"r");

    // convert address
/*
    dest_address[5] = conv(strtok(conf.BTAddress,":"));
    dest_address[4] = conv(strtok(NULL,":"));
    dest_address[3] = conv(strtok(NULL,":"));
    dest_address[2] = conv(strtok(NULL,":"));
    dest_address[1] = conv(strtok(NULL,":"));
    dest_address[0] = conv(strtok(NULL,":"));
*/

    OpenInverter(&conf, &flag, unit, s, archdata, livedata);
    InverterCommand("login",               &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("typelabel",           &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("typelabel",           &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("startuptime",         &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("getacvoltage",        &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("getenergyproduction", &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("getspotdcpower",      &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("getspotdcvoltage",    &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("getspotacpower",      &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("getgridfreq",         &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("maxACPower",          &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("maxACPowerTotal",     &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("ACPowerTotal",        &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("DeviceStatus",        &conf, &flag, unit, s, fp, archdata, livedata);
    if (flag.daterange)
      InverterCommand("getrangedata",        &conf, &flag, unit, s, fp, archdata, livedata);
    InverterCommand("logoff",              &conf, &flag, unit, s, fp, archdata, livedata);

    close(s);

    auto instance = archdata.empty() ? "" : "{}-{}"_format(archdata.back().inverter, archdata.back().serial);
    instance.erase(std::remove(begin(instance), end(instance), ' '), end(instance));
    std::transform(begin(instance), end(instance), begin(instance), [] (unsigned char i) {
        return std::tolower(i);
      });

    float prev_value = -1.f, prev_total = -1.f;
    time_t prev_skipped_zero_date = 0;
    for (const auto& data : archdata)
    {
      if  (!(0 <= prev_value && prev_value < 0.5f
          && 0 <= data.current_value && data.current_value < 0.5f))
      {
        // don't just keep the first zero-power value, keep the last one as well (to ensure mean(value) gives correct results there)
        if (prev_skipped_zero_date)
        {
          fmt::print("kW,domain=sensor,type=sma-bluetooth,entity_id=power_production,instance={instance}        value={current:0.3f} {date}000000000\n"
            , "instance"_a=instance
            , "current"_a=0.f
            , "date"_a=prev_skipped_zero_date
            );
          prev_skipped_zero_date = 0;
        }

        fmt::print("kW,domain=sensor,type=sma-bluetooth,entity_id=power_production,instance={instance}        value={current:0.3f} {date}000000000\n"
          , "instance"_a=instance
          , "current"_a=data.current_value / 1000.f
          , "date"_a=data.date
          );
        prev_value = data.current_value;
      }
      else
      {
        prev_skipped_zero_date = data.date;
      }
      if (data.accum_value - prev_total >= 0.0005f)
      {
        fmt::print("kWh,domain=sensor,type=sma-bluetooth,entity_id=power_production_total,instance={instance} value={total:0.3f} {date}000000000\n"
          , "instance"_a=instance
          , "total"_a=data.accum_value
          , "date"_a=data.date
          );
        prev_total = data.accum_value;
      }
    }

#if 0
    if (flag.debug == 1)
    {
      for (const auto& data: livedata)
      {
        fmt::printf("INSERT INTO LiveData ( DateTime, Inverter, Serial, Description, Value, Units ) VALUES ( \'%s\', \'%s\', %lld, \'%s\', \'%s\', \'%s\'  ) ON DUPLICATE KEY UPDATE DateTime=Datetime, Inverter=VALUES(Inverter), Serial=VALUES(Serial), Description=VALUES(Description), Description=VALUES(Description), Value=VALUES(Value), Units=VALUES(Units)\n", debugdate(), data.inverter, data.serial, data.Description, data.Value, data.Units);
      }

      bool first = true;
      for (const auto& data : archdata)
      {
        if (first)
        {
          // Skip first record as it's a dummy
          first = false;
          continue;
        }
        fmt::printf("INSERT INTO DayData ( DateTime, Inverter, Serial, CurrentPower, EtotalToday ) VALUES ( FROM_UNIXTIME(%ld),\'%s\',%llu,%0.f, %.3f ) ON DUPLICATE KEY UPDATE DateTime=Datetime, Inverter=VALUES(Inverter), Serial=VALUES(Serial), CurrentPower=VALUES(CurrentPower), EtotalToday=VALUES(EtotalToday)\n", data.date, data.inverter, data.serial, data.current_value, data.accum_value);
      }
    }
#endif
}
