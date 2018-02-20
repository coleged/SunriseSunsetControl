/*************************************************************

Copyright (GPL) 2004   Mike Chirico mchirico@comcast.net
   Updated: Sun Nov 28 15:15:05 EST 2004
   Revised: Nov 2017

   Program adapted by Mike Chirico mchirico@comcast.net

	Further adapted by Ed Cole colege@gmail.com
		No changes to sunrise/set calculations
		Improvements/additions to utility elements of the tool

		Tested on MAC OS (High Sierra and Ubuntu 17)

   Reference:
    http://prdownloads.sourceforge.net/souptonuts/working_with_time.tar.gz?download
    http://www.srrb.noaa.gov/highlights/sunrise/sunrise.html

  Compile:

     gcc -o sunrise -Wall -W -O2 -s -pipe -lm sunrise.c

  or for debug output

     gcc -o sunrise -DDEBUG=1 -Wall -W -O2 -s -pipe -lm sunrise.c


************************************************************/

#ifndef DEBUG
#define DEBUG 0 // dont change this - pass it via -D in Makefile
#endif


#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

// lat/log for Bath,UK
#define DEF_LAT  51.38 
#define DEF_LON  2.36
#define MY_NAME "sunrise"
#define TIME_FMT "%d-%m-%Y  %T"  // day-month-year time

double calcSunEqOfCenter(double t); // proto

//______________________________________
void usage(){
    fprintf(stderr,"\nUsage:\n %s [options]\n",MY_NAME);
	fprintf(stderr,"prints sunrise/sunset times for today\n");
        fprintf(stderr,"options modify behaviour thus:\n");
	fprintf(stderr," -b\t\tbegining of day. Print sunrise time\n");
	fprintf(stderr," -e\t\tend of day. Print sunset time\n");
	fprintf(stderr," -t \"format\"\tPrint time using strftime format strings\n");
	fprintf(stderr," -s\t\tsilent or status print nothing\n");
	fprintf(stderr,"   \t\treturn value = 0-daylight 1-darkness\n");
	fprintf(stderr," -h offset\thour offset. Use to test for time before/after  sunrise/set\n");
	fprintf(stderr,"   \t\te.g.  sunset -sh 1 will return 1 until one hour after sunrise\n");
        fprintf(stderr,"   \t\tthen 0, changing to 1, one hour after sunset\n");
	fprintf(stderr," -d day\t\tspecify day\n");
    	fprintf(stderr," -m month\tspecify month\n");
	fprintf(stderr," -y year\tspecify year\n");
	fprintf(stderr," -l lat\t\tspecify latitude\n");
	fprintf(stderr," -o lon\t\tspecify longitude\n");
	fprintf(stderr," -u\t\tprint this usage text\n");
    exit(-1);	

}// usage


//______________________________________
double  degToRad(double angleDeg)
{
	return (M_PI * angleDeg / 180.0);
}

//______________________________________
double radToDeg(double angleRad)
{
	return (180.0 * angleRad / M_PI);
}

//______________________________________
double calcMeanObliquityOfEcliptic(double t)
{
	double seconds = 21.448 - t*(46.8150 + t*(0.00059 - t*(0.001813)));
	double e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;

	return e0;              // in degrees
}

//______________________________________
double calcGeomMeanLongSun(double t)
{
	double L = 280.46646 + t * (36000.76983 + 0.0003032 * t);

	while( (int) L >  360 )
	{
		L -= 360.0;
	}
	while(  L <  0)
	{
		L += 360.0;
	}
	return L;              // in degrees
}

//______________________________________
double calcObliquityCorrection(double t)
{
	double e0 = calcMeanObliquityOfEcliptic(t);


	double omega = 125.04 - 1934.136 * t;
	double e = e0 + 0.00256 * cos(degToRad(omega));
	return e;               // in degrees
}

//______________________________________
double calcEccentricityEarthOrbit(double t)
{
	double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
	return e;               // unitless
}

//______________________________________
double calcGeomMeanAnomalySun(double t)
{
	double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
	return M;               // in degrees
}

//______________________________________
double calcEquationOfTime(double t)
{
	double epsilon = calcObliquityCorrection(t);               
	double  l0 = calcGeomMeanLongSun(t);
	double e = calcEccentricityEarthOrbit(t);
	double m = calcGeomMeanAnomalySun(t);
	double y = tan(degToRad(epsilon)/2.0);
	y *= y;
	double sin2l0 = sin(2.0 * degToRad(l0));
	double sinm   = sin(degToRad(m));
	double cos2l0 = cos(2.0 * degToRad(l0));
	double sin4l0 = sin(4.0 * degToRad(l0));
	double sin2m  = sin(2.0 * degToRad(m));
	double Etime = 		y * sin2l0 
				- 2.0 * e * sinm 
				+ 4.0 * e * y * sinm * cos2l0
				- 0.5 * y * y * sin4l0 
				- 1.25 * e * e * sin2m;

  return radToDeg(Etime)*4.0;	// in minutes of time
}

//______________________________________
double calcTimeJulianCent(double jd)
{
	double T = ( jd - 2451545.0)/36525.0;
	return T;
}

//______________________________________
double calcSunTrueLong(double t)
{
	double l0 = calcGeomMeanLongSun(t);
	double c = calcSunEqOfCenter(t);

	double O = l0 + c;
	return O;               // in degrees
}

//______________________________________
double calcSunApparentLong(double t)
{
	double o = calcSunTrueLong(t);

	double  omega = 125.04 - 1934.136 * t;
	double  lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
	return lambda;          // in degrees
}

//______________________________________
double calcSunDeclination(double t)
{
	double e = calcObliquityCorrection(t);
	double lambda = calcSunApparentLong(t);

	double sint = sin(degToRad(e)) * sin(degToRad(lambda));
	double theta = radToDeg(asin(sint));
	return theta;           // in degrees
}

//______________________________________
double calcHourAngleSunrise(double lat, double solarDec)
{
	double latRad = degToRad(lat);
	double sdRad  = degToRad(solarDec);

	double HA = (acos(cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))
			-tan(latRad) * tan(sdRad)));

	return HA;              // in radians
}

//______________________________________
double calcHourAngleSunset(double lat, double solarDec)
{
	double latRad = degToRad(lat);
	double sdRad  = degToRad(solarDec);


	double HA = (acos(cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))
			-tan(latRad) * tan(sdRad)));

	return -HA;              // in radians
}

//______________________________________
double calcJD(int year,int month,int day)
{
	if (month <= 2) {
		year -= 1;
		month += 12;
	}
	int A = floor(year/100);
	int B = 2 - A + floor(A/4);

	double JD = floor(365.25*(year + 4716)) 
			+ floor(30.6001*(month+1))
			+ day 
			+ B 
			- 1524.5;
	return JD;
}


//______________________________________
double calcJDFromJulianCent(double t)
{
	double JD = t * 36525.0 + 2451545.0;
	return JD;
}


//______________________________________
double calcSunEqOfCenter(double t)
{
	double m = calcGeomMeanAnomalySun(t);

	double mrad = degToRad(m);
	double sinm = sin(mrad);
	double sin2m = sin(mrad+mrad);
	double sin3m = sin(mrad+mrad+mrad);

	double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) 
		+ sin2m * (0.019993 - 0.000101 * t) 
		+ sin3m * 0.000289;
	return C;		// in degrees
}


//______________________________________
double calcSunriseUTC(double JD, double latitude, double longitude)
 {

	double t = calcTimeJulianCent(JD);

		// *** First pass to approximate sunrise


	double  eqTime = calcEquationOfTime(t);
	double  solarDec = calcSunDeclination(t);
	double  hourAngle = calcHourAngleSunrise(latitude, solarDec);
        double  delta = longitude - radToDeg(hourAngle);
	double  timeDiff = 4 * delta;	// in minutes of time	
	double  timeUTC = 720 + timeDiff - eqTime;	// in minutes	
        double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) 
			+ timeUTC/1440.0); 


         eqTime = calcEquationOfTime(newt);
         solarDec = calcSunDeclination(newt);
		
		
	hourAngle = calcHourAngleSunrise(latitude, solarDec);
	delta = longitude - radToDeg(hourAngle);
	timeDiff = 4 * delta;
	timeUTC = 720 + timeDiff - eqTime; // in minutes



	return timeUTC;
}

//______________________________________
double calcSunsetUTC(double JD, double latitude, double longitude)
{

	double t = calcTimeJulianCent(JD);

	// *** First pass to approximate sunset


	double  eqTime = calcEquationOfTime(t);
	double  solarDec = calcSunDeclination(t);
	double  hourAngle = calcHourAngleSunset(latitude, solarDec);
        double  delta = longitude - radToDeg(hourAngle);
	double  timeDiff = 4 * delta;	// in minutes of time	
	double  timeUTC = 720 + timeDiff - eqTime;	// in minutes	
        double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) 
			+ timeUTC/1440.0); 


         eqTime = calcEquationOfTime(newt);
         solarDec = calcSunDeclination(newt);
		
		
	hourAngle = calcHourAngleSunset(latitude, solarDec);
	delta = longitude - radToDeg(hourAngle);
	timeDiff = 4 * delta;
	timeUTC = 720 + timeDiff - eqTime; // in minutes

	// printf("* eqTime = %f  \nsolarDec = %f \ntimeUTC = %f\n\n",eqTime,solarDec,timeUTC);


	return timeUTC;
}




//______________________________________
//______________________________________
int main(int argc, char **argv)
{

  time_t now;
  time_t seconds;
  time_t tseconds;
  struct tm  *ptm=NULL;
  struct tm *today = NULL;
  struct tm  tm;
  int s_flag = 0; // if set return status 0=daylight, 1=darkness
  int rise=0,set = 0;
  int dark = 1;
  char *time_fmt = TIME_FMT;

  now = time(NULL);
  today = gmtime(&now);
  int year = today->tm_year + 1900 ;
  int month = today->tm_mon + 1;
  int day = today->tm_mday;
  int dst=-1;
  time_t hm;

  char buffer[30];

  float JD=calcJD(year,month,day);

  double latitude = DEF_LAT; 
  double longitude = DEF_LON;

 if(DEBUG){
	printf("Compiled with DEBUG flag set\n");
 }// if DEBUG

//Parse command line
   char opt;
   char *string = NULL;
   while ((opt = getopt(argc, argv, "subeh:y:m:d:l:o:t:")) != -1) 
   {
        switch (opt) 
        {
	  case 't': // time format
		string = (char *)malloc(sizeof(optarg));
		strcpy(string,optarg);
		//printf("fmt=%s\n",string);
		time_fmt=string;		
		//printf("fmt=%s\n",time_fmt);
	  break;

          case 's': // status. Return 0 if daylight, 1 if darkness
		s_flag=1;
	   break;
	  
	   case 'b': // begining of day. print sunrise
                rise = 1;
           break;

	   case 'e': // end of day. print sunset time value
		set = 1;
	   break;

           case 'h': // hour offset modifier
		hm = atoi(optarg);
	   break;

          case 'y': // 
	    year = atoi(optarg);
            break;
	
          case 'm': //
            month = atoi(optarg);
            break;
 
          case 'd': //
            day = atoi(optarg);
            break;

	case 'l': //
            latitude = atof(optarg);
            break;

	case 'o': //
            longitude = atof(optarg);
            break;

	case 'u': //
	    usage();
	    break;

	default : //
	    usage();
	    break;

	}
   }// while

    JD = calcJD(year,month,day);


 if(DEBUG){
  printf("Julian Date  %f \n",JD);
  printf("Sunrise timeUTC %lf \n", calcSunriseUTC( JD,  latitude,  longitude));
  printf("Sunset  timeUTC %lf \n", calcSunsetUTC( JD,  latitude,  longitude));
 }// DEBUG

// populate tm with time at start of day (SOD)
// into tm - structure of Broken Down Time (BDT)
  tm.tm_year= year-1900;
  tm.tm_mon=month-1;  /* Jan = 0, Feb = 1,.. Dec = 11 */
  tm.tm_mday=day;
  tm.tm_hour=0;
  tm.tm_min=0;
  tm.tm_sec=0;
  tm.tm_isdst=-1;  // -1 cos we don't know DST status

  seconds = mktime(&tm);	// clean up tm fields
				// including isdst
				// seconds at SOD
				
  if(DEBUG) printf("Seconds at SOD %lis\n",seconds);

  time_t delta;

  dst=tm.tm_isdst;

  ptm = gmtime ( &seconds );    // another tm structure with BDT at SOD

  delta = ptm->tm_hour;   // this will nudge the hour if DST is in effect

  if(DEBUG)
    printf("delta=%lis dst=%d\n",delta,dst);
  
  tseconds = seconds;

  if(DEBUG){
   printf("Number of seconds now %ld\n",seconds);
   strftime(buffer,30,time_fmt,localtime(&now));
   printf("%s\n",buffer);
  }


  // calculate sunrise time
  seconds = seconds + calcSunriseUTC( JD,  latitude,  longitude)*60;
  if(DEBUG) printf("Sunrise %lis\n",seconds);
  // adjust for DST and hour modifier from command line args
  if(DEBUG) printf("delta %lis, hm %lis\n",delta, ((time_t)hm)*3600);
  seconds = seconds - (time_t)delta*3600 + (time_t)hm*3600;
  if(DEBUG) printf("Sunrise (adjusted) %lis\n",seconds);

  strftime(buffer,30,time_fmt,localtime(&seconds));
  if(!s_flag && rise) printf("%s\n",buffer);
  if ( now > seconds ) dark = 0;


  // .... and the same for sunset
  seconds=tseconds;

  seconds+=calcSunsetUTC( JD,  latitude,  longitude)*60;
  seconds= seconds - delta*3600 + hm*3600;
  if(DEBUG) printf("Sunset %lis\n",seconds);

  strftime(buffer,30,time_fmt,localtime(&seconds));
  
  if(!s_flag && set) printf("%s\n",buffer);
  if ( now > seconds ) dark = 1;

  if( s_flag ){
  	return dark;
  }else{
	return 0;
 }

}
