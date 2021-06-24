// AIS.cpp: 定义应用程序的入口点。
//

#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
//网络部分，不同环境的设置

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef size_t SOCKET;
#endif

#include "AIS.h"
#include "CClient.h"
#include "CConnPool.h"
#include "config_file.h"
#include "path_utils.h"
#include "mysql_connection.h"
#include "CoordinateTransformUtils.h"
#include "str_utils.h"
#include "time_utils.h"
#include "general.h"

std::list<CClient *> clientList;
CConnPool *connPool = CConnPool::GetInstance();

string serverIPAddress = "127.0.0.1";
int serverPort = 5400;
string whiteList4ClientIP = "";
SOCKET serverSocket = -1;
string dbSchemaName = "SeaWindNo1";
bool licenseOK = false;


mutex mutexClientList;
mutex mutexCond;
condition_variable condServer;
bool bServerRunning = false;
mutex mutexCache;
mutex				mutexDBOperate;
condition_variable	condDBOperate;

map<int, string> mapShipInfo;
map<int, int> mapShipInfoCategory;                // 船只类型map
map<int, string> mapNationality;
map<int, string> mapHelpInfo;
map<string, int> mapHelpDeviceIDToPersonID;
vector<AISInfo> vAISInfo;
int maxIDofShipInfo = 0;
int curWindFarmID = 0;
int maxIDofHelpInfo = 0;
int AISInitialID = 0;

double windFarm_longitude;
double windFarm_latitude;
double maxDistance2WindFarm = 50.0;
int windFarmID = 0;

using namespace std;
using namespace sql;

int main() {
	if (!InitMember()) {
		cout << "初始化全局变量失败" << endl;
		return false;
	}
	if(!InitSocket()){
		cout << "初始化Socket失败" << endl;
		return false;
	}
	if(!StartServer()){
		cout << "创建线程失败" << endl;
		return false;
	}
	StopService();
	ExitServer();
	return 0;
}

bool InitMember() {

	// 读conf文件到全局变量

	try {
		string configFilePath = path_utils::GetPath("/config/AisDaemon.conf");
		ConfigFile configFile(configFilePath, "=", "#", "EndConfigFile");

		dbSchemaName = configFile.read("dbName", dbSchemaName);
		serverIPAddress = configFile.read("ServerIPAddress", serverIPAddress);
		serverPort = configFile.read("serverPort", serverPort);
		windFarm_longitude = configFile.read("Longitude", windFarm_longitude);
		windFarm_latitude = configFile.read("Latitude", windFarm_latitude);
		windFarmID = configFile.read("WindFarmID", windFarmID);
		maxDistance2WindFarm = configFile.read("maxDistance2WindFarm", maxDistance2WindFarm);
		whiteList4ClientIP = configFile.read("ClientIPAddress", whiteList4ClientIP);

	}
	catch (...) {
		cout << "读取配置文件出错" << endl;
		return false;
	}

	// 获取数据库连接

	sql::Connection *conn;
	try {
		conn = connPool->GetConnection();
		if (conn == NULL) {
			cout << "数据库连接丢失，重新连接" << endl;

			connPool = CConnPool::GetInstance();
			conn = connPool->GetConnection();
		}
	}
	catch (...) {
		cout << "获取数据库连接失败" << endl;
		return false;
	}

	sql::Statement* state = conn->createStatement();
	try {
		string sql = "use " + dbSchemaName;
		state->execute(sql);
	}
	catch (sql::SQLException& e) {
		cout << e.what() << endl;
		return false;
	}
	catch (exception& e) {
		cout << e.what() << endl;
	}


	/*
	 * 初始化，读数据库信息
	 */

	// 验证证书

	try {
		sql::ResultSet *result;

		result = state->executeQuery("select ID, ExpiringDate, Remark from TLICENSE where ID=1");
		while (result->next()) {
			try {
				string remark = result->getString("Remark");
				if (remark == "OK") {
					licenseOK = true;
					cout << "验证证书通过" << endl;
				} else {
					licenseOK = false;
					cout << "验证证书失败" << endl;
					return false;
				}
			}
			catch (sql::SQLException &e) {
				cout << "Error while checking license" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// 初始化读取ShipInfo表，生成mapShipInfo和mapShipInfoCategory

		result = state->executeQuery("select ID, MMSI, Category, UpdateDate from ShipInfo order by ID");
		while (result->next()) {
			try {
				int mmsi = result->getInt("MMSI");
				string updateDate = result->getString("UpdateDate");
				int category = result->getInt("Category");

				mapShipInfo.insert(pair<int, string>(mmsi, updateDate));
				mapShipInfoCategory.insert(pair<int, int>(mmsi, category));
				maxIDofShipInfo = result->getInt("ID");

			}
			catch (sql::SQLException &e) {
				cout << "Error while reading ShinInfo" << endl;
				cout << e.what() << endl;
				return false;
			}
		}
		cout << "生成mapShipInfo成功" << endl;
		cout << "生成mapShipInfoCategory成功" << endl;

		// 初始化读取MID2Nationality表，生成mapNationality

		result = state->executeQuery("select MID, Nationality from MID2Nationality order by MID");
		while (result->next()) {
			try {
				int mid = result->getInt("MID");
				string nationality = result->getString("Nationality");

				mapNationality.insert(pair<int, string>(mid, nationality));
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading MID2Nationality" << endl;
				cout << e.what() << endl;
				return false;
			}
		}
		cout << "生成mapNationality成功" << endl;

		// 初始化读取WindFarm表，获取当前WindFarmID

		result = state->executeQuery("select ID from WindFarm where CurUsing=1");
		while (result->next()) {
			try {
				curWindFarmID = result->getInt("ID");

				cout << "获取curWindFarmID成功" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading WindFarm" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// 初始化读取HelpInfo表,获取HelpDeviceID

		result = state->executeQuery("select ID, HelpDeviceID, UpdateDate from Call4HelpInfo order by ID");
		while (result->next()) {
			try {
				int helpDeviceID = result->getInt("HelpDeviceID");
				string updateDate = result->getString("UpdateDate");

				mapHelpInfo.insert(pair<int, string>(helpDeviceID, updateDate));
				maxIDofHelpInfo = result->getInt("ID");

				cout << "生成mapHelpInfo成功" << endl;
				cout << "获取maxIDofHelpInfo成功" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading Call4HelpInfo" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// 初始化读取应急示位标map

		result = state->executeQuery("select CardNo, UserID from Card2User where CardType=3 order by ID");
		while (result->next()) {
			try {
				string helpDeviceID = result->getString("CardNo");
				int userID = result->getInt("UserID");

				mapHelpDeviceIDToPersonID.insert(pair<string, int>(helpDeviceID, userID));

				cout << "生成mapHelpDeviceIDToPersonID成功" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading Card2User" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// 初始化读取AISInfoInitial的最大ID

		result = state->executeQuery("select max(ID) from AISInfoInitial");
		while (result->next()) {
			try {
				AISInitialID = result->getInt(1);

				cout << "获取AISInitialID成功" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading AISInfoInitial" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

	}
	catch (sql::SQLException &e) {
		cout << "Error while reading database" << endl;
		cout << e.what() << endl;
		return false;
	}
	catch (...) {
		cout << "读取数据库信息错误" << endl;
	}
	/*
	* 读数据库完
	*/

	clientList.clear();
	return true;
}

bool InitSocket() {
#ifdef WIN32
	int retVal;
	WSADATA wsData;

	retVal = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (retVal != 0) {
		cout << "Can not find a usable Windows Sockets dll!" << endl;
		return false;
	}

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		cout << "Create socket failed!" << endl;
		return false;
	}

	unsigned long u1 = 1;
	retVal = ioctlsocket(serverSocket, FIONBIO, &u1);
	if (retVal == SOCKET_ERROR) {
		cout << "ioctlsocket failed!" << endl;
		return false;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	serverAddr.sin_addr.S_un.S_addr = inet_addr(serverIPAddress.c_str());
	retVal = ::bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	if (retVal == SOCKET_ERROR) {
		cout << "Bind server failed!" << endl;
		return false;
	}

	retVal = listen(serverSocket, SOMAXCONN);
	if (retVal == SOCKET_ERROR) {
		cout << "Listen socket failed!" << endl;
		return false;
	}

	return true;

#else
	int retVal;

	serverSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (INVALID_SOCKET == serverSocket) {
		cout << "Create socket failed!" << endl;
		perror("InitSocket");
		return false;
	}

	sockaddr_in  serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	serverAddr.sin_addr.s_addr = inet_addr(serverIPAddress.c_str());
	retVal = ::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (SOCKET_ERROR == retVal)
	{
		cout << "Bind socket failed!" << endl;
		perror("InitSocket");
		return false;
	}

	retVal = listen(serverSocket, SOMAXCONN);
	if (SOCKET_ERROR == retVal)
	{
		cout << "Listen socket failed!" << endl;
		perror("InitSocket");
		return false;
	}

	return true;

#endif

}

bool StartServer() {
	bServerRunning = true;

	thread accept(AcceptThread);
	thread clean(CleanThread);
	thread dbOperate(DBOperateThread);

	accept.detach();
	clean.detach();
	dbOperate.detach();

	return true;
}

void StopService(){
	char cInput;

	printf("输入e 或 E 退出系统\n");

	for (; bServerRunning;)
	{
		scanf("%c", &cInput);

		if (cInput == 'E' || cInput == 'e')
		{
			break;
		}
		else
		{
			sleep_(TIMEFOR_THREAD_EXIT);  //线程睡眠
		}
	}
	printf("服务器退出!\n");

	unique_lock<mutex> lock_(mutexCond);
	bServerRunning = false;
	//cout << "ready to wait" << endl;
	condServer.wait(lock_);
	condDBOperate.notify_all();
	sleep_(TIMEFOR_THREAD_EXIT);

	return;
}

void ExitServer(){

#ifdef WIN32
	closesocket(serverSocket);
	WSACleanup();
#else
	close(serverSocket);
#endif

}

void CleanThread(){
	for (; bServerRunning;)						//服务器正在运行
	{
        sleep_(TIMEFOR_THREAD_SLEEP);
		lock_guard<mutex> guard(mutexClientList);

		//清理已经断开的连接客户端内存空间
		auto iter = clientList.begin();

		for (; iter != clientList.end();)
		{
			CClient *pClient = (CClient*)*iter;

			if (pClient->IsExit())					//客户端线程已经退出
			{
				printf("客户端退出IP:%s\n", pClient->Getm_addr());

				clientList.erase(iter++);			//删除节点

				delete pClient;						//释放内存
				pClient = NULL;
			}
			else
			{
				iter++;								//指针下移
			}
		}
	}

	//服务器停止工作
	if (!bServerRunning)
	{
		//断开每个连接，线程退出
		unique_lock<mutex> lock(mutexClientList);
		list<CClient *>::iterator iter = clientList.begin();

		for (; iter != clientList.end();)
		{
			CClient *pClient = (CClient *)*iter;

			//如果客户端的连接还存在，则断开连接，线程退出
			if (pClient->IsConning())
			{
				pClient->DisConning();
			}

			++iter;
		}

		lock.unlock();
		//给连接客户端线程时间，使其自动退出
		sleep_(TIMEFOR_THREAD_SLEEP);

		lock.lock();
		//确保为每个客户端分配的内存空间都回收
		//如果不假如while 这层循环，可能存在这样的情况，当pClient->IsExit()时
		//该线程还没有退出
		//那么就需要从链表的开始部分重新判断。
		while (0 != clientList.size())
		{
			iter = clientList.begin();

			for (iter; iter != clientList.end();)
			{
				CClient *pClient = (CClient *)*iter;

				if (pClient->IsExit())				//客户端线程已经退出
				{
					clientList.erase(iter++);		//删除节点

					delete pClient;					//释放内存空间
					pClient = NULL;
				}
				else
				{
					iter++;							//指针下移
				}
			}

			//给连接客户端线程时间，使其自动退出
			sleep_(TIMEFOR_THREAD_SLEEP);
		}
		lock.unlock();

	}

	clientList.clear();								//清空链表

	unique_lock<mutex> lock(mutexCond);
	condServer.notify_all();
    //cout << "notify" << endl;
	return;
}

void AcceptThread(){

#ifdef WIN32
	//Windows下

	SOCKET sAccept;															//接受客户端连接的套接字
	sockaddr_in addrClient;													//客户端SOCKET地址

	for (; bServerRunning;)													//服务器状态
	{
		memset(&addrClient, 0, sizeof(sockaddr_in));                    //初始化

		int lenClient = sizeof(sockaddr_in);                                //地址长度
		sAccept = accept(serverSocket, (sockaddr *) &addrClient, &lenClient);    //接受客户端请求
		if (sAccept == INVALID_SOCKET) {
			int nErrCode = WSAGetLastError();
			if (nErrCode == WSAEWOULDBLOCK) {
				sleep_(TIMEFOR_THREAD_SLEEP);
			}
			else {
				return;
			}
		}
		else {
			CClient *pClient = new CClient(sAccept, addrClient);            //创建客户端对象
			string::size_type idx = whiteList4ClientIP.find(pClient->Getm_addr());

			if (idx == string::npos) {
				printf_s("accept请求失败，IP：%s 为非法请求！\n", pClient->Getm_addr());
				sleep_(TIMEFOR_THREAD_SLEEP);
				continue;                                                    //继续等待
			}

			printf_s("accept请求成功！IP：%s\n", pClient->Getm_addr());

			lock_guard<mutex> guard(mutexClientList);
			clientList.push_back(pClient);                                    //加入链表
			pClient->StartRuning();                                            //为接受的客户端建立接收数据和发送数据线程
		}
	}
#else
	//Linux下
	// TODO

    SOCKET sAccept;															//接受客户端连接的套接字
    sockaddr_in addrClient;													//客户端SOCKET地址

    for (; bServerRunning;)													//服务器状态
    {
        memset(&addrClient, 0, sizeof(sockaddr_in));						//初始化

        socklen_t lenClient = sizeof(sockaddr_in);								//地址长度
        sAccept = accept(serverSocket, (sockaddr*)&addrClient, &lenClient);		//接受客户端请求

        if (INVALID_SOCKET == sAccept)
        {
            if (errno == EWOULDBLOCK)								//无法立即完成一个非阻挡性套接字操作
            {
                sleep_(TIMEFOR_THREAD_SLEEP);
                continue;													//继续等待
            }
            else
            {
                cout << "Accept socket error:" << strerror(errno) << " (errno:)" << errno << endl;
                return;													//线程退出
            }
        }
        else																//接受客户端的请求
        {
            CClient* pClient = new CClient(sAccept, addrClient);			//创建客户端对象
            string::size_type idx = whiteList4ClientIP.find(pClient->Getm_addr());

            if (idx == string::npos)
            {
                printf("accept请求失败，IP：%s 为非法请求！\n", pClient->Getm_addr());

                sleep_(TIMEFOR_THREAD_SLEEP);

                continue;													//继续等待
            }

            printf("accept请求成功！IP：%s\n", pClient->Getm_addr());

            lock_guard<mutex> lock_(mutexClientList);
            clientList.push_back(pClient);									//加入链表
            pClient->StartRuning();											//为接受的客户端建立接收数据和发送数据线程
            //cout << "begin running" << endl;
        }
    }

    return;  //线程退出
#endif

}

void DBOperateThread() {
	sql::Connection *conn;
	try {
		conn = connPool->GetConnection();
		if (conn == NULL) {
			cout << "数据库连接丢失，重新连接" << endl;

			connPool = CConnPool::GetInstance();
			conn = connPool->GetConnection();
		}
	}
	catch (...) {
		cout << "获取数据库连接失败" << endl;
		return;
	}
	sql::Statement * state = conn->createStatement();
	state->execute("use " + dbSchemaName);

	while(bServerRunning){
		unique_lock<mutex> dbLock(mutexDBOperate);
		condDBOperate.wait(dbLock);

		lock_guard<mutex> lock_(mutexCache);
		try {
			for (auto iter = vAISInfo.begin(); iter != vAISInfo.end(); ++iter) {
				AisMessage aisMessage = iter->aisMessage;

				// 过滤数据之前，先把数据记录到AISInfoInitial
				try{
					char sql[1024] = {0};
					snprintf(sql, sizeof(sql),"insert into AISInfoInitial (ID, AISInitData, IDXDate) values "
											  "(%s,'%s',%s)",to_string(iter->id).c_str(),iter->aisInitial.c_str(),time_utils::NowDateString().c_str());

					if(conn != NULL && !conn->isClosed()){
						state->executeUpdate(sql);
					}
					else{
						cout << "插入表 AISInfoInitial 失败，连接丢失" << endl;
						continue;
					}
				}
				catch (sql::SQLException& e) {
					cout << "插入到 AISInfoInitial 错误" << endl;
					cout << e.what() << endl;
				}
				catch (...){
					cout << "插入到 AISInfoInitial 错误" << endl;
				}

				// 过滤无效数据
				if (aisMessage.getMMSI() == 0) {
					cout << "MMSI无效!" << endl;
					continue;
				}
				CoordinateTransformUtils transform;
				Coordinate lnglat = transform.wgs84todb09(aisMessage.getLON(), aisMessage.getLAT());

				// 计算距离，maxDistance2WindFarm km以外的消息丢弃不存入
				double real_distance =
						transform.RealDistance(windFarm_longitude, windFarm_latitude, lnglat.lng, lnglat.lat) /
						1000;

				if (real_distance >= maxDistance2WindFarm && (aisMessage.getMESSAGETYPE() == 1 ||
															  aisMessage.getMESSAGETYPE() == 2 ||
															  aisMessage.getMESSAGETYPE() == 3 ||
															  aisMessage.getMESSAGETYPE() == 4 ||
															  aisMessage.getMESSAGETYPE() == 9 ||
															  aisMessage.getMESSAGETYPE() == 18 ||
															  aisMessage.getMESSAGETYPE() == 19 ||
															  aisMessage.getMESSAGETYPE() == 24)) {
					cout << "距离" << maxDistance2WindFarm << "km以外的消息:" << real_distance << endl;

					continue;
				}

				char lon[16] = { 0 };
				char lat[16] = { 0 };
				char lon09[16] = { 0 };
				char lat09[16] = { 0 };

				snprintf(lon, sizeof(lon), "%3.8lf", aisMessage.getLON());
				snprintf(lon09, sizeof(lon), "%3.8lf", lnglat.lng);
				snprintf(lat, sizeof(lat), "%3.8lf", aisMessage.getLAT());
				snprintf(lat09, sizeof(lat), "%3.8lf", lnglat.lat);
				

				// 如果mapNationality没有这个MID对应国家，则插入格式为MID:xxx
				string Nationality;
				string mid = str_utils::toString(aisMessage.getMMSI()).substr(0, 3);
				int Mid = atoi(mid.c_str());

				try {
					if (mapNationality.count(Mid) == 1) {
						Nationality = mapNationality[Mid];
					} else {
						Nationality = "MID:" + str_utils::toString(Mid);
					}
				}
				catch (...) {
					Nationality = "Illegal";
				}

				string shipName = aisMessage.getVESSELNAME();

				// 便于sql语句插入
				str_utils::replace_all(shipName, "'", "-");
				str_utils::replace_all(shipName, "@", "");
				str_utils::replace_all(shipName, "%", "_");
				shipName = str_utils::trim(shipName, " ");
				
				// MMSI号以972开始时更新Call4HelpInfo表: 有则更新，无则插入，同步更新mapHelpInfo
				if (str_utils::startsWith(str_utils::toString(aisMessage.getMMSI()), "972")) {
					string helpDeviceID = str_utils::toString(aisMessage.getMMSI());
					string helpInfo = aisMessage.getSAFETYMESSAGE();
					string personnelID = "";

					if (mapHelpDeviceIDToPersonID.count(helpDeviceID) == 0) {
						personnelID = str_utils::toString(mapHelpDeviceIDToPersonID[helpDeviceID]);
					} else {
						personnelID = "-1";
					}

					// 无则插入
					if(mapHelpInfo.count(aisMessage.getMMSI()) == 0){
						char sqlData[1024] = { 0 };
						// 1类型的安全消息
						if(aisMessage.getMESSAGETYPE() == 1){
							snprintf(sqlData, sizeof(sqlData),"insert into Call4HelpInfo (ID, HelpType, LON, LAT, LON09, LAT09, WindFarmID, RecordStatus, ReadTime) "
											 "values (%s, 1, %s, %s, %s, %s, %s, 1, '%s')",
											 to_string(++maxIDofHelpInfo).c_str(),
											 lon,
											 lat,
											 lon09,
											 lat09,
											 to_string(windFarmID).c_str(),
											 time_utils::NowtimeString().c_str());

						}
						// 14类型的安全信息
						else if(aisMessage.getMESSAGETYPE() == 14) {
							snprintf(sqlData, sizeof(sqlData),"insert into Call4HelpInfo (ID, PersonnelID, HelpInfo, HelpDeviceID, WindFarmID, RecordStatus, ReadTime) "
											"values (%s, %s, '%s', '%s', %s, 1, '%s')",
									to_string(++maxIDofHelpInfo).c_str(),
									personnelID.c_str(),
									helpInfo.c_str(),
									helpDeviceID.c_str(),
									 to_string(windFarmID).c_str(),
									 time_utils::NowtimeString().c_str());
						}
						// 其他舍弃
						else{
							cout << "插入Call4HelpInfo表：无效的消息类型：" << aisMessage.getMESSAGETYPE() << endl;
							continue;
						}
						// 执行sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);

								mapHelpInfo.insert(pair<int, string>(aisMessage.getMMSI(), str_utils::toString(aisMessage.getDATETIME())));

								cout << "插入表Call4HelpInfo数据成功" << endl;
								continue;
							}
							else {
								cout << "插入表Call4HelpInfo失败，连接丢失" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
					}
					// 有则更新
					else {
						char sqlData[1024] = {0};
						// 1类型的安全消息
						if(aisMessage.getMESSAGETYPE() == 1){
							snprintf(sqlData, sizeof(sqlData),
									 "update Call4HelpInfo set HelpType='1', PersonnelID=%s, LON=%s, LAT=%s, LON09=%s, LAT09=%s, UpdateDate='%s', RecordStatus=1 where HelpDeviceID='%s'",
									 personnelID.c_str(),
									 lon,
									 lat,
									 lon09,
									 lat09,
									 time_utils::NowtimeString().c_str(),
									 helpDeviceID.c_str());
						}
						// 14类型的安全信息
						else if(aisMessage.getMESSAGETYPE() == 14){
							snprintf(sqlData, sizeof(sqlData),
									 "update Call4HelpInfo set PersonnelID=%s,HelpInfo='%s',UpdateDate='%s' where HelpDeviceID='%s'",
									 personnelID.c_str(),
									 helpInfo.c_str(),
									 time_utils::NowtimeString().c_str(),
									 helpDeviceID.c_str());
						}
						// 其他舍弃
						else{
							cout << "更新Call4HelpInfo表：无效的消息类型：" << aisMessage.getMESSAGETYPE() << endl;
							continue;
						}
						// 执行sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);
								mapHelpInfo[aisMessage.getMMSI()] = aisMessage.getDATETIME();
								cout << "更新表Call4HelpInfo数据成功" << endl;
							}
							else {
								cout << "更新表Call4HelpInfo失败，连接丢失" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
					}
				}

				// 常规MMSI号，更新ShipInfo表：有则更新，无则插入，并同步更新mapShipInfo
				else{
					// 无则插入
					if (mapShipInfo.count(aisMessage.getMMSI()) == 0) {
						char sqlData[1024] = {0};
						// 过滤掉经纬度为0，以下消息类型可通过
						if (aisMessage.getLON() >= 0.0 && aisMessage.getVESSELTYPEINT() >=0 &&
							(aisMessage.getMESSAGETYPE() == 1 || aisMessage.getMESSAGETYPE() == 2 ||
							 aisMessage.getMESSAGETYPE() == 3 || aisMessage.getMESSAGETYPE() == 5 ||
							 aisMessage.getMESSAGETYPE() == 14 || aisMessage.getMESSAGETYPE() == 18 ||
							 aisMessage.getMESSAGETYPE() == 19 || aisMessage.getMESSAGETYPE() == 24))
						{
							snprintf(sqlData, sizeof(sqlData),
									 "insert into ShipInfo (ID, MMSI, ShipName, Category, Nationality, ShipType, TrueHeading, ROT, SOG, ShipHead, NavStatus, WindFarmID, LON, LAT, LON09, LAT09, SafetyMessage, CreateDate, UpdateDate, Activing) values"
									 "(%s,%s,'%s',0,'%s',%s,%s,%s,%s,%s,'%s',%s,%s,%s,%s,%s,'%s','%s','%s',1)",
									 to_string(++maxIDofShipInfo).c_str(),
									 to_string(aisMessage.getMMSI()).c_str(),
									 shipName.c_str(),
									 Nationality.c_str(),
									 to_string(aisMessage.getVESSELTYPEINT()).c_str(),
									 to_string((int)aisMessage.getTRUE_HEADING()).c_str(),
									 to_string((int)aisMessage.getROT()).c_str(),
									 to_string(aisMessage.getSOG()).c_str(),
									 to_string((int)aisMessage.getCOG()).c_str(),
									 to_string(aisMessage.getNAVSTATUS()).c_str(),
									 to_string(curWindFarmID).c_str(),
									 lon,
									 lat,
									 lon09,
									 lat09,
									 aisMessage.getSAFETYMESSAGE(),
									 time_utils::NowtimeString().c_str(),
									 time_utils::NowtimeString().c_str());
						}
						else{
							cout << "插入表 ShipInfo 失败" << endl;
							cout << "无效经纬度：" << aisMessage.getLON() << "，或船舶类型：" << aisMessage.getVESSELTYPEINT() << "，或消息类型：" << aisMessage.getMESSAGETYPE() << endl;
							cout << iter->aisInitial << endl;
							continue;
						}
						// 执行sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);
								mapShipInfo.insert(pair<int, string>(aisMessage.getMMSI(), to_string(aisMessage.getDATETIME())));
								cout << "插入表 ShipInfo 数据成功" << endl;
							}
							else {
								cout << "插入表 ShipInfo 失败，连接丢失" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
						
					}
					// 有则更新
					else{
						char sqlData[1024] = {0};
						// 按消息类型插入
						if(aisMessage.getLON() >= 0.0 &&
						(aisMessage.getMESSAGETYPE() == 1 || aisMessage.getMESSAGETYPE() == 2 ||
						aisMessage.getMESSAGETYPE() == 3 || aisMessage.getMESSAGETYPE() == 18))
						{
							// todo: activing? 拼错了？
							snprintf(sqlData, sizeof(sqlData),
									 "update ShipInfo set TrueHeading=%s, ROT=%s, SOG=%s, ShipHead=%s, NavStatus='%s', LON=%s, LAT=%s, LON09=%s, LAT09=%s, UpdateDate='%s', Nationality='%s', ShipName='%s', Activing=1 where MMSI=%s",
									 to_string((int)aisMessage.getTRUE_HEADING()).c_str(),
									 to_string((int)aisMessage.getROT()).c_str(),
									 to_string(aisMessage.getSOG()).c_str(),
									 to_string((int)aisMessage.getCOG()).c_str(),
									 to_string(aisMessage.getNAVSTATUS()).c_str(),
									 lon,
									 lat,
									 lon09,
									 lat09,
									 time_utils::NowtimeString().c_str(),
									 Nationality.c_str(),
									 shipName.c_str(),
									 to_string(aisMessage.getMMSI()).c_str());
						}
						// 静态位置
						else if (aisMessage.getMESSAGETYPE() == 5){
							snprintf(sqlData, sizeof(sqlData),
									 "update ShipInfo set CallSign='%s', ShipName='%s', ShipType=%s, ETA='%s', Draught=%s, Destination='%s', UpdateDate='%s' where MMSI=%s",
									 aisMessage.getCALLSIGN(),
									 shipName.c_str(),
									 to_string(aisMessage.getVESSELTYPEINT()).c_str(),
									 to_string(aisMessage.getETA()).c_str(),
									 to_string(aisMessage.getDRAUGHT()).c_str(),
									 aisMessage.getDESTINATION(),
									 time_utils::NowtimeString().c_str(),
									 to_string(aisMessage.getMMSI()).c_str());
						}
						// 安全广播信息
						else if (aisMessage.getMESSAGETYPE() == 14){
							snprintf(sqlData, sizeof(sqlData),
									 "update ShipInfo set SafetyMessage='%s', UpdateDate='%s' where MMSI=%s",
									 aisMessage.getSAFETYMESSAGE(),
									 time_utils::NowtimeString().c_str(),
									 to_string(aisMessage.getMMSI()).c_str());
						}
						// 扩展B类设备位置报告
						else if(aisMessage.getMESSAGETYPE() == 19){
							snprintf(sqlData, sizeof(sqlData),
									 "update ShipInfo set SOG=%s, ShipHead=%s, LON=%s, LAT=%s, LON09=%s, LAT09=%s, TrueHeading=%s, ShipType=%s, UpdateDate='%s', ShipName='%s' where MMSI=%s",
									 to_string(aisMessage.getSOG()).c_str(),
									 to_string((int)aisMessage.getCOG()).c_str(),
									 lon,
									 lat,
									 lon09,
									 lat09,
									 to_string((int)aisMessage.getTRUE_HEADING()).c_str(),
									 to_string(aisMessage.getVESSELTYPEINT()).c_str(),
									 time_utils::NowtimeString().c_str(),
									 shipName.c_str(),
									 to_string(aisMessage.getMMSI()).c_str());
						}
						// 消息24：静态数据报告
						else if(aisMessage.getMESSAGETYPE() == 24){
							// 24 A类
							if(aisMessage.getPARTNUMBER() == 0){
								snprintf(sqlData, sizeof(sqlData),
										 "update ShipInfo set ShipName='%s', UpdateDate='%s' where MMSI=%s",
										 shipName.c_str(),
										 time_utils::NowtimeString().c_str(),
										 to_string(aisMessage.getMMSI()).c_str());
							}
							// 24 B类
							else{
								snprintf(sqlData,sizeof(sqlData),
										 "update ShipInfo set ShipType=%s, CallSign='%s', UpdateDate='%s' where MMSI=%s",
										 to_string(aisMessage.getVESSELTYPEINT()).c_str(),
										 aisMessage.getCALLSIGN(),
										 time_utils::NowtimeString().c_str(),
										 to_string(aisMessage.getMMSI()).c_str());
							}
						}
						else{
							cout << "更新 ShipInfo 表：无效的消息类型：" << aisMessage.getMESSAGETYPE() << endl;
							continue;
						}
						// 执行sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);
								mapShipInfo[aisMessage.getMMSI()] = to_string(aisMessage.getDATETIME());
								cout << "更新表 ShipInfo 数据成功" << endl;
							}
							else {
								cout << "更新表 ShipInfo 失败，连接丢失" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
						
					}
				}

				// 插入AISInfoCurDay和AISInfoHistory表
				try{
					char sqlCurDay[1024] = {0};
					char sqlHistory[1024] = {0};
					snprintf(sqlCurDay,sizeof(sqlCurDay),
							 "insert into AISInfoCurDay (MMSI, MsgType, ShipName, Category, ShipType, TrueHeading, ROT, SOG, ShipHead, NavStatus, Draught, Destination, ETA, Length, ShipWidth, Bow, Stern, Port, Starboard, LON, LAT, LON09, LAT09, CallSign, IMO, SafetyMessage, InitialID, IDXDate) values "
							 "(%s,%s,'%s',%s,%s,%s,%s,%s,%s,'%s',%s,'%s','%s',%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,'%s',%s,'%s',%s,%s)",
							 to_string(aisMessage.getMMSI()).c_str(),
							 to_string(aisMessage.getMESSAGETYPE()).c_str(),
							 shipName.c_str(),
							 to_string(mapShipInfoCategory[aisMessage.getMMSI()]).c_str(),
							 to_string(aisMessage.getVESSELTYPEINT()).c_str(),
							 to_string((int)aisMessage.getTRUE_HEADING()).c_str(),
							 to_string((int)aisMessage.getROT()).c_str(),
							 to_string(aisMessage.getSOG()).c_str(),
							 to_string((int)aisMessage.getCOG()).c_str(),
							 to_string(aisMessage.getNAVSTATUS()).c_str(),
							 to_string(aisMessage.getDRAUGHT()).c_str(),
							 aisMessage.getDESTINATION(),
							 to_string(aisMessage.getETA()).c_str(),
							 to_string(aisMessage.getSHIPLENGTH()).c_str(),
							 to_string(aisMessage.getSHIPWIDTH()).c_str(),
							 to_string(aisMessage.getBOW()).c_str(),
							 to_string(aisMessage.getSTERN()).c_str(),
							 to_string(aisMessage.getPORT()).c_str(),
							 to_string(aisMessage.getSTARBOARD()).c_str(),
							 lon,
							 lat,
							 lon09,
							 lat09,
							 aisMessage.getCALLSIGN(),
							 to_string(aisMessage.getIMO()).c_str(),
							 aisMessage.getSAFETYMESSAGE(),
							 to_string(iter->id).c_str(),
							 time_utils::NowDateString().c_str());
					snprintf(sqlHistory,sizeof(sqlHistory),
							 "insert into AISInfoHistory (MMSI, MsgType, ShipName, Category, ShipType, TrueHeading, ROT, SOG, ShipHead, NavStatus, Draught, Destination, ETA, Length, ShipWidth, Bow, Stern, Port, Starboard, LON, LAT, LON09, LAT09, CallSign, IMO, SafetyMessage, InitialID, IDXDate) values "
							 "(%s,%s,'%s',%s,%s,%s,%s,%s,%s,'%s',%s,'%s','%s',%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,'%s',%s,'%s',%s,%s)",
							 to_string(aisMessage.getMMSI()).c_str(),
							 to_string(aisMessage.getMESSAGETYPE()).c_str(),
							 shipName.c_str(),
							 to_string(mapShipInfoCategory[aisMessage.getMMSI()]).c_str(),
							 to_string(aisMessage.getVESSELTYPEINT()).c_str(),
							 to_string((int)aisMessage.getTRUE_HEADING()).c_str(),
							 to_string((int)aisMessage.getROT()).c_str(),
							 to_string(aisMessage.getSOG()).c_str(),
							 to_string((int)aisMessage.getCOG()).c_str(),
							 to_string(aisMessage.getNAVSTATUS()).c_str(),
							 to_string(aisMessage.getDRAUGHT()).c_str(),
							 aisMessage.getDESTINATION(),
							 to_string(aisMessage.getETA()).c_str(),
							 to_string(aisMessage.getSHIPLENGTH()).c_str(),
							 to_string(aisMessage.getSHIPWIDTH()).c_str(),
							 to_string(aisMessage.getBOW()).c_str(),
							 to_string(aisMessage.getSTERN()).c_str(),
							 to_string(aisMessage.getPORT()).c_str(),
							 to_string(aisMessage.getSTARBOARD()).c_str(),
							 lon,
							 lat,
							 lon09,
							 lat09,
							 aisMessage.getCALLSIGN(),
							 to_string(aisMessage.getIMO()).c_str(),
							 aisMessage.getSAFETYMESSAGE(),
							 to_string(iter->id).c_str(),
							 time_utils::NowDateString().c_str());

					// 执行sql
					try {
						if (conn != NULL && !conn->isClosed()) {
							state->executeUpdate(sqlCurDay);
							state->executeUpdate(sqlHistory);
						}
						else {
							cout << "插入表 AISInfo 失败，连接丢失" << endl;
							continue;
						}
					}
					catch (sql::SQLException& e) {
						cout << e.what() << endl;
					}
					
				}
				catch (...){
					cout << "插入到AisInfo错误" << endl;
				}
			}
			// end iterate vAISInfo
			//cout << "处理 " << vAISInfo.size() << " 条数据" << endl;
		}
		catch (...){
			cout << "Error while iterate vAISInfo" << endl;
		}
		vAISInfo.clear();
	}
	return;
}