/* 
 * File:   time_func.h
 * Author: root
 *
 * Created on 21 марта 2016 г., 23:12
 */

#ifndef TIME_FUNC_H
#define	TIME_FUNC_H

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

inline uint64_t	
get_tick_count(void)
{
        struct timeval tv;
        if(gettimeofday(&tv, NULL) != 0)
                return 0;

        return (uint64_t)(tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

inline uint64_t	
get_nanosec(void)
{
	struct timespec		time;
	
	clock_gettime(CLOCK_MONOTONIC, &time);
	
	return (uint64_t)(time.tv_sec*1000000000 + time.tv_nsec);
}

inline uint64_t	
get_microsec(void)
{
	struct timespec		time;
	
	clock_gettime(CLOCK_MONOTONIC, &time);
	
	return (uint64_t)(time.tv_sec*1000000 + time.tv_nsec/1000);
}

inline uint64_t	
get_millisec(void)
{
	struct timespec		time;
	
	clock_gettime(CLOCK_MONOTONIC, &time);
	
	return (uint64_t)(time.tv_sec*1000 + time.tv_nsec/1000000);
}

inline uint32_t	
get_sec(void)
{
	struct timespec		time;
	
	clock_gettime(CLOCK_MONOTONIC, &time);
	
	return time.tv_sec;
}

inline uint64_t	
get_server_millisec(const int64_t iDeltaMillis)
{
	struct timespec		time;
	
	clock_gettime(CLOCK_MONOTONIC, &time);
	
	//its true if delta_millis=client_millis - server_millis
	return (uint64_t)((time.tv_sec*1000000000 + time.tv_nsec)/1000000 - iDeltaMillis);//if 
}

/* Warning! This function can only be used in one instance due to the use of static variables. */
inline float	
get_fps(void)
{
	static uint64_t oldMillis=0, newMillis ;
	static float fFrameCounter=0, fFPS=100, fDelta;

	newMillis=get_millisec();
	fFrameCounter++;
	fDelta = newMillis-oldMillis;
	if( fDelta>1000 )
	{
		fFPS = (float)((fFrameCounter/fDelta)*1000);
		fFrameCounter = 0;
		oldMillis = newMillis;
	}
	return fFPS;
}

/* Warning! This function can only be used in one instance due to the use of static variables.  */
inline float	
traf_per_sec(uint64_t	*uNewBytes)
{
	uint64_t	static		uOldBytes = 0;
	uint64_t	static		uOldMillis = 0;
	uint64_t				uNewMillis = 0;
	float		static		fTPS = 0;
	
	if( uNewBytes == NULL)
		return 0;
	
	uNewMillis = get_millisec();
	if( (uNewMillis - uOldMillis) > 1000)
	{
		fTPS = (*uNewBytes-uOldBytes)*1;
		uOldBytes = *uNewBytes;
		uOldMillis = uNewMillis;
	}
	
	return fTPS;
}

inline void		
sleep_nanosec(uint32_t uiNanosec)
{
	struct timespec	ts;
	ts.tv_sec = uiNanosec/1000000000;
	ts.tv_nsec = uiNanosec - ts.tv_sec*1000000000;
	nanosleep(&ts, NULL);
}

inline void		
sleep_microsec(uint32_t uiMicrosec)
{
	struct timespec	ts;
	ts.tv_sec = uiMicrosec/1000000;
	ts.tv_nsec = uiMicrosec*1000 - ts.tv_sec*1000000000;
	nanosleep(&ts, NULL);
}

inline void		
sleep_millisec(uint32_t uiMillisec)
{
	struct timespec	ts;
	ts.tv_sec = uiMillisec/1000;
	ts.tv_nsec = uiMillisec*1000000 - ts.tv_sec*1000000000;
	nanosleep(&ts, NULL);
}

inline void		
sleep_sec(uint32_t uiSec)
{
	struct timespec	ts;
	ts.tv_sec = uiSec;
	ts.tv_nsec = 0;
	nanosleep(&ts, NULL);
}

inline void 
get_time_string_from_millis(char *szBuff, size_t uBuffSize, uint64_t uMillis)
{
	if(szBuff == NULL || uBuffSize == 0)
		return;
	
	uint64_t uSec = uMillis/1000;
	uint32_t uHours = uSec/3600;
	uint32_t uMinutes = uSec%3600/60;
	uint32_t uSeconds = uSec%3600%60;
	
	snprintf(szBuff, uBuffSize, "%0.2d:%0.2d:%0.2d", uHours, uMinutes, uSeconds);
}

/* Wait uWaitMillis milliseconds for defined value uVal of flag pFlag */
/* Function for threads syncronization */
inline int32_t	
wait_for_flag(const uint8_t* pFlag, const uint32_t uVal, const uint32_t uWaitMillis)
{
	uint64_t	uOldMillis = get_millisec();
	
	while(uWaitMillis > (get_millisec() - uOldMillis) )
	{
		if(*pFlag == uVal)
			return 1;
		
		sleep_microsec(100);
	}
	
	return -1;
}

#endif	/* TIME_FUNC_H */

