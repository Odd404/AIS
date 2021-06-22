// AIS.cpp: ����Ӧ�ó������ڵ㡣
//

#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
//���粿�֣���ͬ����������

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef size_t SOCKET
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
map<int, int> mapShipInfoCategory;                // ��ֻ����map
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
		cout << "��ʼ��ȫ�ֱ���ʧ��" << endl;
		return false;
	}
	if(!InitSocket()){
		cout << "��ʼ��Socketʧ��" << endl;
		return false;
	}
	if(!StartServer()){
		cout << "�����߳�ʧ��" << endl;
		return false;
	}
	StopService();
	ExitServer();
	return 0;
}

bool InitMember() {

	// ��conf�ļ���ȫ�ֱ���

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
		cout << "��ȡ�����ļ�����" << endl;
		return false;
	}

	// ��ȡ���ݿ�����

	sql::Connection *conn;
	try {
		conn = connPool->GetConnection();
		if (conn == NULL) {
			cout << "���ݿ����Ӷ�ʧ����������" << endl;

			connPool = CConnPool::GetInstance();
			conn = connPool->GetConnection();
		}
	}
	catch (...) {
		cout << "��ȡ���ݿ�����ʧ��" << endl;
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
	 * ��ʼ���������ݿ���Ϣ
	 */

	// ��֤֤��

	try {
		sql::ResultSet *result;

		result = state->executeQuery("select ID, ExpiringDate, Remark from TLICENSE where ID=1");
		while (result->next()) {
			try {
				string remark = result->getString("Remark");
				if (remark == "OK") {
					licenseOK = true;
					cout << "��֤֤��ͨ��" << endl;
				} else {
					licenseOK = false;
					cout << "��֤֤��ʧ��" << endl;
					return false;
				}
			}
			catch (sql::SQLException &e) {
				cout << "Error while checking license" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// ��ʼ����ȡShipInfo������mapShipInfo��mapShipInfoCategory

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
		cout << "����mapShipInfo�ɹ�" << endl;
		cout << "����mapShipInfoCategory�ɹ�" << endl;

		// ��ʼ����ȡMID2Nationality������mapNationality

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
		cout << "����mapNationality�ɹ�" << endl;

		// ��ʼ����ȡWindFarm����ȡ��ǰWindFarmID

		result = state->executeQuery("select ID from WindFarm where CurUsing=1");
		while (result->next()) {
			try {
				curWindFarmID = result->getInt("ID");

				cout << "��ȡcurWindFarmID�ɹ�" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading WindFarm" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// ��ʼ����ȡHelpInfo��,��ȡHelpDeviceID

		result = state->executeQuery("select ID, HelpDeviceID, UpdateDate from Call4HelpInfo order by ID");
		while (result->next()) {
			try {
				int helpDeviceID = result->getInt("HelpDeviceID");
				string updateDate = result->getString("UpdateDate");

				mapHelpInfo.insert(pair<int, string>(helpDeviceID, updateDate));
				maxIDofHelpInfo = result->getInt("ID");

				cout << "����mapHelpInfo�ɹ�" << endl;
				cout << "��ȡmaxIDofHelpInfo�ɹ�" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading Call4HelpInfo" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// ��ʼ����ȡӦ��ʾλ��map

		result = state->executeQuery("select CardNo, UserID from Card2User where CardType=3 order by ID");
		while (result->next()) {
			try {
				string helpDeviceID = result->getString("CardNo");
				int userID = result->getInt("UserID");

				mapHelpDeviceIDToPersonID.insert(pair<string, int>(helpDeviceID, userID));

				cout << "����mapHelpDeviceIDToPersonID�ɹ�" << endl;
			}
			catch (sql::SQLException &e) {
				cout << "Error while reading Card2User" << endl;
				cout << e.what() << endl;
				return false;
			}
		}

		// ��ʼ����ȡAISInfoInitial�����ID

		result = state->executeQuery("select max(ID) from AISInfoInitial");
		while (result->next()) {
			try {
				AISInitialID = result->getInt(1);

				cout << "��ȡAISInitialID�ɹ�" << endl;
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
		cout << "��ȡ���ݿ���Ϣ����" << endl;
	}
	/*
	* �����ݿ���
	*/

	clientList.clear();
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
	if (INVALID_SOCKET == sServer) {
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
}

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

	printf("����e �� E �˳�ϵͳ\n");

	for (; bServerRunning;)
	{
		scanf("%c", &cInput);

		if (cInput == 'E' || cInput == 'e')
		{
			break;
		}
		else
		{
			sleep(TIMEFOR_THREAD_EXIT);  //�߳�˯��
		}
	}
	printf("�������˳�!\n");

	unique_lock<mutex> lock_(mutexCond);
	bServerRunning = false;
	condServer.wait(lock_);
	sleep(TIMEFOR_THREAD_EXIT);

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
	for (; bServerRunning;)						//��������������
	{
		lock_guard<mutex> guard(mutexClientList);

		//�����Ѿ��Ͽ������ӿͻ����ڴ�ռ�
		auto iter = clientList.begin();

		for (; iter != clientList.end();)
		{
			CClient *pClient = (CClient*)*iter;

			if (pClient->IsExit())					//�ͻ����߳��Ѿ��˳�
			{
				printf("�ͻ����˳�IP:%s\n", pClient->Getm_addr());

				clientList.erase(iter++);			//ɾ���ڵ�

				delete pClient;						//�ͷ��ڴ�
				pClient = NULL;
			}
			else
			{
				iter++;								//ָ������
			}
		}

		sleep(TIMEFOR_THREAD_SLEEP);
	}

	//������ֹͣ����
	if (!bServerRunning)
	{
		//�Ͽ�ÿ�����ӣ��߳��˳�
		unique_lock<mutex> lock(mutexClientList);
		list<CClient *>::iterator iter = clientList.begin();

		for (; iter != clientList.end();)
		{
			CClient *pClient = (CClient *)*iter;

			//����ͻ��˵����ӻ����ڣ���Ͽ����ӣ��߳��˳�
			if (pClient->IsConning())
			{
				pClient->DisConning();
			}

			++iter;
		}

		lock.unlock();
		//�����ӿͻ����߳�ʱ�䣬ʹ���Զ��˳�
		sleep(TIMEFOR_THREAD_SLEEP);

		lock.lock();
		//ȷ��Ϊÿ���ͻ��˷�����ڴ�ռ䶼����
		//���������while ���ѭ�������ܴ����������������pClient->IsExit()ʱ
		//���̻߳�û���˳�
		//��ô����Ҫ������Ŀ�ʼ���������жϡ�
		while (0 != clientList.size())
		{
			iter = clientList.begin();

			for (iter; iter != clientList.end();)
			{
				CClient *pClient = (CClient *)*iter;

				if (pClient->IsExit())				//�ͻ����߳��Ѿ��˳�
				{
					clientList.erase(iter++);		//ɾ���ڵ�

					delete pClient;					//�ͷ��ڴ�ռ�
					pClient = NULL;
				}
				else
				{
					iter++;							//ָ������
				}
			}

			//�����ӿͻ����߳�ʱ�䣬ʹ���Զ��˳�
			sleep(TIMEFOR_THREAD_SLEEP);
		}
		lock.unlock();

	}

	clientList.clear();								//�������

	unique_lock<mutex> lock(mutexCond);
	condServer.notify_all();

	return;
}

void AcceptThread(){

#ifdef WIN32
	//Windows��

	SOCKET sAccept;															//���ܿͻ������ӵ��׽���
	sockaddr_in addrClient;													//�ͻ���SOCKET��ַ

	for (; bServerRunning;)													//������״̬
	{
		memset(&addrClient, 0, sizeof(sockaddr_in));                    //��ʼ��

		int lenClient = sizeof(sockaddr_in);                                //��ַ����
		sAccept = accept(serverSocket, (sockaddr *) &addrClient, &lenClient);    //���ܿͻ�������
		if (sAccept == INVALID_SOCKET) {
			int nErrCode = WSAGetLastError();
			if (nErrCode == WSAEWOULDBLOCK) {
				sleep(TIMEFOR_THREAD_SLEEP);
			}
			else {
				return;
			}
		}
		else {
			CClient *pClient = new CClient(sAccept, addrClient);            //�����ͻ��˶���
			string::size_type idx = whiteList4ClientIP.find(pClient->Getm_addr());

			if (idx == string::npos) {
				printf_s("accept����ʧ�ܣ�IP��%s Ϊ�Ƿ�����\n", pClient->Getm_addr());
				sleep(TIMEFOR_THREAD_SLEEP);
				continue;                                                    //�����ȴ�
			}

			printf_s("accept����ɹ���IP��%s\n", pClient->Getm_addr());

			lock_guard<mutex> guard(mutexClientList);
			clientList.push_back(pClient);                                    //��������
			pClient->StartRuning();                                            //Ϊ���ܵĿͻ��˽����������ݺͷ��������߳�
		}
	}
#else
	//Linux��
	// TODO


#endif

}

void DBOperateThread() {
	sql::Connection *conn;
	try {
		conn = connPool->GetConnection();
		if (conn == NULL) {
			cout << "���ݿ����Ӷ�ʧ����������" << endl;

			connPool = CConnPool::GetInstance();
			conn = connPool->GetConnection();
		}
	}
	catch (...) {
		cout << "��ȡ���ݿ�����ʧ��" << endl;
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

				// ��������֮ǰ���Ȱ����ݼ�¼��AISInfoInitial
				try{
					char sql[1024] = {0};
					snprintf(sql, sizeof(sql),"insert into AISInfoInitial (ID, AISInitData, IDXDate) values "
											  "(%s,'%s',%s)",to_string(iter->id).c_str(),iter->aisInitial.c_str(),time_utils::NowDateString().c_str());

					if(conn != NULL && !conn->isClosed()){
						state->executeUpdate(sql);
					}
					else{
						cout << "����� AISInfoInitial ʧ�ܣ����Ӷ�ʧ" << endl;
						continue;
					}
				}
				catch (sql::SQLException& e) {
					cout << "���뵽 AISInfoInitial ����" << endl;
					cout << e.what() << endl;
				}
				catch (...){
					cout << "���뵽 AISInfoInitial ����" << endl;
				}

				// ������Ч����
				if (aisMessage.getMMSI() == 0) {
					cout << "MMSI��Ч!" << endl;
					continue;
				}
				CoordinateTransformUtils transform;
				Coordinate lnglat = transform.wgs84todb09(aisMessage.getLON(), aisMessage.getLAT());

				// ������룬maxDistance2WindFarm km�������Ϣ����������
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
					cout << "����" << maxDistance2WindFarm << "km�������Ϣ:" << real_distance << endl;

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
				

				// ���mapNationalityû�����MID��Ӧ���ң�������ʽΪMID:xxx
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

				// ����sql������
				str_utils::replace_all(shipName, "'", "-");
				str_utils::replace_all(shipName, "@", "");
				str_utils::replace_all(shipName, "%", "_");
				shipName = str_utils::trim(shipName, " ");
				
				// MMSI����972��ʼʱ����Call4HelpInfo��: ������£�������룬ͬ������mapHelpInfo
				if (str_utils::startsWith(str_utils::toString(aisMessage.getMMSI()), "972")) {
					string helpDeviceID = str_utils::toString(aisMessage.getMMSI());
					string helpInfo = aisMessage.getSAFETYMESSAGE();
					string personnelID = "";

					if (mapHelpDeviceIDToPersonID.count(helpDeviceID) == 0) {
						personnelID = str_utils::toString(mapHelpDeviceIDToPersonID[helpDeviceID]);
					} else {
						personnelID = "-1";
					}

					// �������
					if(mapHelpInfo.count(aisMessage.getMMSI()) == 0){
						char sqlData[1024] = { 0 };
						// 1���͵İ�ȫ��Ϣ
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
						// 14���͵İ�ȫ��Ϣ
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
						// ��������
						else{
							cout << "����Call4HelpInfo����Ч����Ϣ���ͣ�" << aisMessage.getMESSAGETYPE() << endl;
							continue;
						}
						// ִ��sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);

								mapHelpInfo.insert(pair<int, string>(aisMessage.getMMSI(), str_utils::toString(aisMessage.getDATETIME())));

								cout << "�����Call4HelpInfo���ݳɹ�" << endl;
								continue;
							}
							else {
								cout << "�����Call4HelpInfoʧ�ܣ����Ӷ�ʧ" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
					}
					// �������
					else {
						char sqlData[1024] = {0};
						// 1���͵İ�ȫ��Ϣ
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
						// 14���͵İ�ȫ��Ϣ
						else if(aisMessage.getMESSAGETYPE() == 14){
							snprintf(sqlData, sizeof(sqlData),
									 "update Call4HelpInfo set PersonnelID=%s,HelpInfo='%s',UpdateDate='%s' where HelpDeviceID='%s'",
									 personnelID.c_str(),
									 helpInfo.c_str(),
									 time_utils::NowtimeString().c_str(),
									 helpDeviceID.c_str());
						}
						// ��������
						else{
							cout << "����Call4HelpInfo����Ч����Ϣ���ͣ�" << aisMessage.getMESSAGETYPE() << endl;
							continue;
						}
						// ִ��sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);
								mapHelpInfo[aisMessage.getMMSI()] = aisMessage.getDATETIME();
								cout << "���±�Call4HelpInfo���ݳɹ�" << endl;
							}
							else {
								cout << "���±�Call4HelpInfoʧ�ܣ����Ӷ�ʧ" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
					}
				}

				// ����MMSI�ţ�����ShipInfo��������£�������룬��ͬ������mapShipInfo
				else{
					// �������
					if (mapShipInfo.count(aisMessage.getMMSI()) == 0) {
						char sqlData[1024] = {0};
						// ���˵���γ��Ϊ0��������Ϣ���Ϳ�ͨ��
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
							cout << "����� ShipInfo ʧ��" << endl;
							cout << "��Ч��γ�ȣ�" << aisMessage.getLON() << "���򴬲����ͣ�" << aisMessage.getVESSELTYPEINT() << "������Ϣ���ͣ�" << aisMessage.getMESSAGETYPE() << endl;
							cout << iter->aisInitial << endl;
							continue;
						}
						// ִ��sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);
								mapShipInfo.insert(pair<int, string>(aisMessage.getMMSI(), to_string(aisMessage.getDATETIME())));
								cout << "����� ShipInfo ���ݳɹ�" << endl;
							}
							else {
								cout << "����� ShipInfo ʧ�ܣ����Ӷ�ʧ" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
						
					}
					// �������
					else{
						char sqlData[1024] = {0};
						// ����Ϣ���Ͳ���
						if(aisMessage.getLON() >= 0.0 &&
						(aisMessage.getMESSAGETYPE() == 1 || aisMessage.getMESSAGETYPE() == 2 ||
						aisMessage.getMESSAGETYPE() == 3 || aisMessage.getMESSAGETYPE() == 18))
						{
							// todo: activing? ƴ���ˣ�
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
						// ��̬λ��
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
						// ��ȫ�㲥��Ϣ
						else if (aisMessage.getMESSAGETYPE() == 14){
							snprintf(sqlData, sizeof(sqlData),
									 "update ShipInfo set SafetyMessage='%s', UpdateDate='%s' where MMSI=%s",
									 aisMessage.getSAFETYMESSAGE(),
									 time_utils::NowtimeString().c_str(),
									 to_string(aisMessage.getMMSI()).c_str());
						}
						// ��չB���豸λ�ñ���
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
						// ��Ϣ24����̬���ݱ���
						else if(aisMessage.getMESSAGETYPE() == 24){
							// 24 A��
							if(aisMessage.getPARTNUMBER() == 0){
								snprintf(sqlData, sizeof(sqlData),
										 "update ShipInfo set ShipName='%s', UpdateDate='%s' where MMSI=%s",
										 shipName.c_str(),
										 time_utils::NowtimeString().c_str(),
										 to_string(aisMessage.getMMSI()).c_str());
							}
							// 24 B��
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
							cout << "���� ShipInfo ����Ч����Ϣ���ͣ�" << aisMessage.getMESSAGETYPE() << endl;
							continue;
						}
						// ִ��sql
						try {
							if (conn != NULL && !conn->isClosed()) {
								state->executeUpdate(sqlData);
								mapShipInfo[aisMessage.getMMSI()] = to_string(aisMessage.getDATETIME());
								cout << "���±� ShipInfo ���ݳɹ�" << endl;
							}
							else {
								cout << "���±� ShipInfo ʧ�ܣ����Ӷ�ʧ" << endl;
								continue;
							}
						}
						catch (sql::SQLException& e) {
							cout << e.what() << endl;
						}
						
					}
				}

				// ����AISInfoCurDay��AISInfoHistory��
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

					// ִ��sql
					try {
						if (conn != NULL && !conn->isClosed()) {
							state->executeUpdate(sqlCurDay);
							state->executeUpdate(sqlHistory);
						}
						else {
							cout << "����� AISInfo ʧ�ܣ����Ӷ�ʧ" << endl;
							continue;
						}
					}
					catch (sql::SQLException& e) {
						cout << e.what() << endl;
					}
					
				}
				catch (...){
					cout << "���뵽AisInfo����" << endl;
				}
			}
			// end iterate vAISInfo
			//cout << "���� " << vAISInfo.size() << " ������" << endl;
		}
		catch (...){
			cout << "Error while iterate vAISInfo" << endl;
		}
		vAISInfo.clear();
	}
}