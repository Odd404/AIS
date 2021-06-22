#ifndef time_utils_h__
#define time_utils_h__

#ifndef WIN32
	#include <sys/time.h>
#else
	#include<ctime> 
#endif

#include <string>

namespace time_utils
{
#ifndef WIN32
	inline std::string NowtimeString()
	{
        time_t   time_;
        struct   tm   *ptm;
        time_    =   time(NULL);
        time(&time_);
        ptm      =   localtime(&time_);

        char temp[100] = {0};
        strftime(temp,sizeof(temp),"%Y-%m-%d %H:%M:%S",ptm);
        std::string str;
        str.assign(temp);

        return str;
	}

	inline std::string NowDateString()
	{
        time_t   time_;
        struct   tm   *ptm;
        time_    =   time(NULL);
        time(&time_);
        ptm      =   localtime(&time_);

        char temp[100] = {0};
        strftime(temp,sizeof(temp),"%Y%m%d",ptm);
        std::string str;
        str.assign(temp);

        return str;
	}

#else
	inline std::string NowtimeString()
	{
		std::time_t time_now = std::time(NULL);
		tm* tm_now = localtime(&time_now);
		char time_str[sizeof("yyyy-mm-dd hh:mm:ss")] = {0};
		
		std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_now);
		
		return time_str;
	}

	inline std::string NowDateString()
	{
		std::time_t time_now = std::time(NULL);
		tm* tm_now = localtime(&time_now);

		char time_str[sizeof("yyyymmdd")] = { 0 };

		std::strftime(time_str, sizeof(time_str), "%Y%m%d", tm_now);

		return time_str;
	}

	inline std::string FormatTimeString(unsigned __int64 time_stamp)
	{
		std::time_t time_then = time_stamp;
		tm* tm_now = localtime(&time_then);
		char time_str[sizeof("yyyy-mm-dd hh:mm:ss")] = {0};
	
		std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_now);

		return time_str;
	}
#endif
}
#endif // time_utils_h__