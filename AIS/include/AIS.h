// AIS.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#pragma once
#ifndef AIS_H_
#define AIS_H_

#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "AisMessage.h"
using namespace std;


#define TIMEFOR_THREAD_SLEEP    500			//等待客户端请求线程睡眠时间
#define TIMEFOR_THREAD_EXIT     5000
#define TIMEFOR_THREAD_CLIENT   500			//客户端接收消息睡眠时间
#define TIMEFOR_THREAD_CLEAN    1000

extern bool licenseOK;

extern map<int, string>     mapShipInfo;						// MMSI/UpdateDate
extern map<int, int>        mapShipInfoCategory;                // MMSI/Category
extern map<int, string>     mapNationality;						// MID/Nationality
extern map<int, string>     mapHelpInfo;						// HelpDeviceID/UpdateDate
extern map<string, int>     mapHelpDeviceIDToPersonID;			// CardNo/UserID
extern mutex                mutexCache;
extern mutex				mutexDBOperate;
extern condition_variable	condDBOperate;

extern string dbSchemaName;
extern double windFarm_longitude;
extern double windFarm_latitude;
extern double maxDistance2WindFarm;
extern int windFarmID;

extern int maxIDofShipInfo;
extern int curWindFarmID;
extern int maxIDofHelpInfo;
extern int AISInitialID;

struct AISInfo{
	int         id;
	string      aisInitial;
	AisMessage  aisMessage;
};

extern vector<AISInfo> vAISInfo;

#ifdef WIN32
//睡眠函数
#define sleep_(time)    Sleep(time);
#define snprintf       _snprintf
#else
#define sleep_(time)    usleep(1000 * time);
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1

#endif


bool InitMember();
bool InitSocket();
bool StartServer();
//bool CreateThread(void);
void StopService();
void ExitServer();

void CleanThread();
void AcceptThread();
void DBOperateThread();

#endif // !AIS_H_


//
