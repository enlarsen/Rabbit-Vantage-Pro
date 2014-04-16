//#memmap xmem
//#memmap root

#define BAUD_RATE 19200L

#define EINBUFSIZE 1023
#define EOUTBUFSIZE 1023

#define DEBUG
#define NTP_DEBUG

#define TCPCONFIG 3
#define NIST_SERVER_IP "nist1-sj.glassey.com"
#define MAX_UDP_SOCKET_BUFFERS 1

#define LOCAL_PORT	22554
#define REMOTE_PORT 22554
#define REMOTE_IP "192.168.1.255"

#define NIST_PORT 13
#define TIMEZONE (-8)

// Must be one less than a power of 2
#define ARQUEUESIZE 15

#define DEBUG
#define LOG_ON

#ifdef DEBUG
	#define DEBUG_MYSQL
	#define DEBUG_LOG
#endif


//#use RS232.LIB

#use "vp.lib"
#use "dcrtcp.lib"
#use "mysql.lib"

VPDateTime weatherData;

archiveRecord arQueue[ARQUEUESIZE + 1];

udp_Socket sock;

int arQueueHead, arQueueTail;

typedef struct
{
	int timeStamp;
	int barometer;
	int outsideTemperature;
	int outsideHumidity;
	int windSpeed;
	int windDirection;
} foobar;

extern int serialPortInUse;

int archiveCallback(archiveRecord *ar)
{

	if(((arQueueTail + 1) & ARQUEUESIZE) == arQueueHead)
    {
		printf("Archive record queue full\n");
        return 0;
    }
    weatherData.dateStamp = ar->dateStamp;
	weatherData.timeStamp = ar->timeStamp;
//	printf(" \x1Bt");
    printf("Time:             %02.2d:%02.2d\n", ar->timeStamp / 100, ar->timeStamp % 100);
    printf("Barometer:        %.3f\n", (float)ar->barometer/1000);
    printf("Outside temp:     %.1f\n", (float)ar->outsideTemperature/10);
    printf("Outside humidity: %d\n", ar->outsideHumidity);
    printf("Wind speed:       %d\n", ar->windSpeed);
    printf("Wind direction    %d\n", ar->windDirection);

    memcpy((void *)&arQueue[arQueueTail], (void *)ar, sizeof(archiveRecord));

    printf("arQueueTail=%d\n", arQueueTail);
    arQueueTail = (arQueueTail + 1) & ARQUEUESIZE;
    return 1;
}

scofunc void addRecordsToDB()
{
	mysql_connection conn;
    char buffer[255];
    int year, month, day;
    int hour, minute;
    int rc;

    if(arQueueTail == arQueueHead)
    	return;

    wfd rc = mysql_connect(&conn, resolve("192.168.1.100"), MYSQL_PORT, "weatherpw", "weatherpw");

	// If there is an error, try again later
    if(rc == 0)
    {
        sock_close(&conn.sock);

    	return;
    }

    while(arQueueHead != arQueueTail)
    {
	    year = arQueue[arQueueHead].dateStamp / 512 + 2000;
	    month = arQueue[arQueueHead].dateStamp % 512 / 32;
	    day = arQueue[arQueueHead].dateStamp % 512 % 32;

	    hour = arQueue[arQueueHead].timeStamp / 100;
	    minute = arQueue[arQueueHead].timeStamp % 100;

	    sprintf(buffer, "insert into BellevueWeather.current values (" \
	            "'%d-%d-%d %02.2d:%02.2d',%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
	            year, month, day, hour, minute, arQueue[arQueueHead].outsideTemperature,
	            arQueue[arQueueHead].highOutsideTemperature,
	            arQueue[arQueueHead].lowOutsideTemperature,
	            arQueue[arQueueHead].rainfall, arQueue[arQueueHead].highRainRate,
	            arQueue[arQueueHead].barometer,
	            arQueue[arQueueHead].windSamples, arQueue[arQueueHead].insideTemperature,
	            arQueue[arQueueHead].insideHumidity, arQueue[arQueueHead].outsideHumidity,
	            arQueue[arQueueHead].windSpeed, arQueue[arQueueHead].highWindSpeed,
	            (short)arQueue[arQueueHead].highWindDirection, (short)arQueue[arQueueHead].windDirection,
	            arQueue[arQueueHead].reedClosed, arQueue[arQueueHead].reedOpened);

	    printf("%s\n", buffer);

	    wfd rc = mysql_query(&conn, buffer);

		// If there is an error, try again later.
        if(rc == 0)
        {
        	sock_close(&conn.sock);
        	return;
		}

        arQueueHead = (arQueueHead + 1) & ARQUEUESIZE;
    }
    sock_close(&conn.sock);

}

scofunc void getDateTimeFromDB()
{
    int rc;
    mysql_connection conn;
    int fieldLength;
	char buffer[255];
    char temp[255];
    char *p;
    int i;
    int j;

        wfd rc = mysql_connect(&conn, resolve("192.168.1.100"), MYSQL_PORT, "weatherpw", "weatherpw");
        if(rc == 0)
        {
			sock_close(&conn.sock);
            printf("Could not connect\n");
            return;
		}

//        wfd rc = mysql_query(&conn, "select * from BellevueWeather.test");


        wfd rc = mysql_query(&conn, "select (year(datetime)-2000)*512+month(datetime)*32+dayofmonth(datetime), " \
        	"hour(datetime)*100+minute(datetime) from BellevueWeather.current order by datetime desc limit 1");

        if(rc == 0)
        {
			sock_close(&conn.sock);
            printf("Could not query\n");
            return;
		}


        while( mysql_getFieldInfo(&conn, NULL,  buffer) )
		{
            log( LOG_DEBUG, "Field: %s", buffer );
        }

		wfd rc = mysql_next(&conn, buffer, sizeof(buffer));

/*        i = 0;
		while(rc)
        {
        	log(LOG_DEBUG, "Row %d", i);
            for(j = 0; j < mysql_fieldCount(&conn); j++)
            {
            	if(p = mysql_fieldData(&conn, j, &fieldLength));
                {
                	strncpy(temp, p, fieldLength);
                    temp[fieldLength] = '\0';
                    log(LOG_DEBUG, "Field %d: %s", j, temp);
                }
            }
            i++;
            wfd rc = mysql_next(&conn, buffer, sizeof(buffer));
        }

*/




        if(rc == 0)
        {
			sock_close(&conn.sock);
        	printf("Could not next\n");
            return;
		}

		printf("Buffer=%s\n", buffer);

        p = mysql_fieldData(&conn, 0, &fieldLength);
        strncpy(temp, p, fieldLength);
        temp[fieldLength] = '\0';
        printf("Field0=%s\n", temp);

        weatherData.dateStamp = atoi(temp);

        p = mysql_fieldData(&conn, 1, &fieldLength);
        strncpy(temp, p, fieldLength);
        temp[fieldLength] = '\0';
        printf("Field1=%s\n", temp);

        weatherData.timeStamp = atoi(temp);

        printf("weatherData.dateStamp=%x\tweatherData.timeStamp=%x\n",
        	weatherData.dateStamp, weatherData.timeStamp);

        sock_close(&conn.sock);
}

void nist_time()
{
 	int status;
	char buffer[2048];
	longword ip;
	int i, dst, health;
	struct tm	t;
	unsigned long	longsec;
    tcp_Socket s;

	ip=resolve(NIST_SERVER_IP);
	tcp_open(&s, 0, ip, NIST_PORT, NULL);
	sock_wait_established(&s, 0, NULL, &status);
	sock_mode(&s, TCP_MODE_ASCII);


	// receive and process data -- the server will close the connection,
	// which will cause execution to continue at sock_err below.

	while (tcp_tick(&s)) {
		sock_wait_input(&s, 0, NULL, &status);
		sock_gets(&s, buffer, 48);
	}

sock_err:
	if (status == -1) {
		printf("\nConnection timed out, exiting.\n");
		exit(1);
	}
	if (status != 1) {
		printf("\nUnknown sock_err (%d), exiting.\n", status);
		exit(1);
	}

	sock_close(&s);


	// Dynamic C doesn't have a sscanf function, so we do
	// it this way instead...
	t.tm_year = 100 + 10*(buffer[6]-'0') + (buffer[7]-'0');
	t.tm_mon  = 10*(buffer[9]-'0') + (buffer[10]-'0');
	t.tm_mday = 10*(buffer[12]-'0') + (buffer[13]-'0');
	t.tm_hour = 10*(buffer[15]-'0') + (buffer[16]-'0');
	t.tm_min  = 10*(buffer[18]-'0') + (buffer[19]-'0');
	t.tm_sec  = 10*(buffer[21]-'0') + (buffer[22]-'0');
	dst       = 10*(buffer[24]-'0') + (buffer[25]-'0');
	health    = buffer[27]-'0';


	// convert from tm_struct to seconds since Jan 1, 1980
	// (much easier to adjust for DST and timezone this way)

	longsec = mktime(&t);
	longsec += 3600ul * TIMEZONE;		// adjust for timezone
	if (dst != 0)
		longsec += 3600ul;	// DST is in effect


	// convert back to tm struct for display to stdio
	mktm(&t, longsec);
	printf("Current time:  %02d:%02d:%02d  %02d/%02d/%04d\n",
				t.tm_hour, t.tm_min, t.tm_sec,
				t.tm_mon, t.tm_mday, 1900 + t.tm_year);
	if (dst)
		printf("Daylight Saving Time is in effect.\n");

	switch (health) {
		case 0:
			printf("Server is healthy.\n");
			break;
		case 1:
			printf("Server may be off by up to 5 seconds.\n");
			break;
		case 2:
			printf("Server is off by more than 5 seconds; not setting RTC.\n");
			break;
		default:
			printf("Server failure has occured; try another server (not setting RTC).\n");
			break;
	}

	// finally, set the RTC if the results seems good
	if (health < 2)
		write_rtc(longsec);

}

void main()
{
    struct tm timeTest;
    loopPacket lp;
   	int loopRet;

    int archiveRet;
	char	buffer[100];

   	serEopen(BAUD_RATE);
    serEdatabits(PARAM_8BIT);


    loopinit();
    serialPortInUse = 0;
    arQueueHead = 0;
    arQueueTail = 0;


	sock_init();
	// Wait for the interface to come up
	while (ifpending(IF_DEFAULT) == IF_COMING_UP) {
		tcp_tick(NULL);
	}

	/* Print who we are... */
	printf( "My IP address is %s\n\n", inet_ntoa(buffer, gethostid()) );

    udp_open(&sock, LOCAL_PORT, resolve(REMOTE_IP), REMOTE_PORT, NULL);

    // nist_time();

    // mktm(&timeTest, weatherData.timeDateStamp);

    while(1)
    {
       loophead();

       tcp_tick(NULL);

       costate initDateTime init_on
       {
       		wfd getDateTimeFromDB();

		}

       costate // Handle archive records
        {
            wfd archiveRet = wakeUp();

            if(archiveRet == 1)
            {
//	            weatherData.timeDateStamp = read_rtc() - 5*60L;
	            wfd dumpArchiveRecords(&weatherData, &archiveCallback);
	            waitfor(DelaySec(60L));
            }
            else
            {
            	waitfor(DelaySec(1L));
            }
        }
		costate // Loop
		{

 	      	wfd loopRet = wakeUp();
            if(loopRet == 1)
            {
	            wfd loopRet = loop(&lp);
	            if(loopRet == 1)
	            {
	                printf("Barometer:        %.3f\n", (float)lp.lr.barometer/1000);
	                printf("Outside temp:     %.1f\n", (float)lp.lr.outsideTemperature/10);
	                printf("Outside humidity: %d\n", lp.lr.outsideHumidity);
	                printf("Wind speed:       %d\n", lp.lr.windSpeed);
	                printf("Wind direction    %d\n", lp.lr.windDirection);
	                printf("Sunrise:          %02d:%02d\n", lp.lr.sunrise/100, lp.lr.sunrise % 100);
	                printf("Sunset:           %02d:%02d\n", lp.lr.sunset/100, lp.lr.sunset % 100);
	                printf("Battery status:   %d\n", lp.lr.batteryStatus);
	                printf("Console battery:  %.2f\n", (float)(((float)lp.lr.consoleBattery * 300)/512)/100.0);
                    printf("Forecast icons:   %x\n", lp.lr.forecastIcons);
                    printf("Forecast rule:    %d\n", lp.lr.forecastRule);

                    udp_send(&sock, (char *)&lp.lr, sizeof(loopRecord));
	            }
	            waitfor(DelaySec(10L));
            }
            else
            {
            	waitfor(DelaySec(1L));
            }
		}
		costate // Add records to the DB
	    {
			wfd addRecordsToDB();
	    }
    }
}