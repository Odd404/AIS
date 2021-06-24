//
// Created by Administrator on 2021/6/12.
//

#include <thread>
#include <vector>
#include "AIS.h"
#include "CClient.h"
#include "str_utils.h"
#include "AisMessage.h"
#include "AisMessageParser.h"
#include "CoordinateTransformUtils.h"
#include "general.h"

using namespace std;




CClient::CClient(const SOCKET sClient, const sockaddr_in &addrClient) {
	//初始化变量
	m_socket = sClient;
	m_addr = addrClient;
	m_bConning = false;
	m_bExit = false;

}

bool CClient::StartRuning() {
	if (!licenseOK){
		cout << "对不起，获取授权错误，不能启动！！！" << endl;
		return false;
	}
	else{
		m_bConning = true;

		thread recvThread(RecvDataThread,this);
		recvThread.detach();

		return true;
	}

}

void CClient::RecvDataThread(void *pParam) {
	auto *pClient = (CClient*) pParam;

	while(pClient->m_bConning){
		try{
			int retVal;
			char buf[MAX_NUM_BUF + 1] = { 0 };

#ifdef WIN32
			retVal = recv(pClient->m_socket, buf, MAX_NUM_BUF, 0);
			buf[MAX_NUM_BUF] = '\0';
			if (retVal == SOCKET_ERROR)
			{
				int nErrCode = WSAGetLastError();

				if (nErrCode == WSAEWOULDBLOCK)						//接收数据缓冲区不可用
				{
					sleep_(TIMEFOR_THREAD_CLIENT);						//线程睡眠等待客户端数据

					continue;											//继续循环
				}
				else if (WSAENETDOWN == nErrCode						//客户端关闭了连接
						 || WSAETIMEDOUT == nErrCode
						 || WSAECONNRESET == nErrCode)
				{
					cout << "-------客户端关闭了连接:" << pClient->Getm_addr() << "|nErrCode is:" << nErrCode << "-------" << endl;
					break;												//线程退出
				}
			}
			//客户端关闭了连接
			else if (retVal == 0) {
				cout << "-------客户端关闭了连接retVal=0-------" << endl;
				break;
			}
			else {
				pClient->ParseData(buf);
			}

			sleep_(TIMEFOR_THREAD_CLIENT);

#else
			// TODO check
			retVal = recv(pClient->m_socket, buf, MAX_NUM_BUF, MSG_DONTWAIT);
			buf[MAX_NUM_BUF] = '\0';
			if (retVal == 0)
			{
				cout << "-------客户端关闭了连接:" << pClient->Getm_addr() << "------" <<endl;
				break; //线程退出
			}
			else if(retVal == SOCKET_ERROR)
			{
				if (errno == EWOULDBLOCK)
				{
					sleep_(TIMEFOR_THREAD_CLIENT);
					continue;
				}
				else {
					cout << "-------Client: TCP recv error------" << endl;
					perror("");
				}
			}
			else {
                pClient->ParseData(buf);
			}
            sleep_(TIMEFOR_THREAD_CLIENT);
#endif

		}
		catch (...) {
			cout << "RecvDataThread捕获错误了..." << endl;;
		}
	}
	pClient->m_bConning = false;
    //cout << "CClient exit" << endl;
    pClient->m_bExit = true;
    //cout << "thread exit" << endl;
	return;
}

bool CClient::ParseData(const char *pRecvData) {

	//如果"byebye" 或者 "Byebye"
	if (pRecvData == "BYEBYE"){
		cout << "recieve byebye" << endl;
	}
	else{
		int nReceived = strlen(pRecvData);
		cout << "+++++++nReceived is:" << nReceived << endl;

		if(nReceived != -1){
			try{
				char aisMessageStr[MAX_NUM_BUF + 1] = { 0 };
				strncpy(aisMessageStr, pRecvData, sizeof(aisMessageStr));
				aisMessageStr[MAX_NUM_BUF] = '\0';
				//将回车换行符\r\n分隔字符串，结果为vector
				string strAisMessage(aisMessageStr);
				//统一处理分隔，添加\r\n
				strAisMessage += "\r\n";
				vector<string> vAisData;
				str_utils::SplitStringStd(strAisMessage, "\r\n", vAisData);

				char messageStr[64] = { 0 };
				string messageStrMultiple;
				int i = 0, j = 0, k = 0;
				int lenAisData = 0;
				int totalAisData = 0;
				int lenAisMessage = sizeof(AisMessage);

				AisMessageParser aisMessageParser;
				AisMessage aisMessage;

				// 用于存储需要拼接的字符串
				map<string, string> mapMultipleMessageClassA;
				map<string, string> mapMultipleMessageClassB;

				for (std::vector<string>::iterator it = vAisData.begin(); it != vAisData.end(); ++it)
				{
					if ((str_utils::startsWith(*it, "!AIVDM") || str_utils::startsWith(*it, "!AIVDO")) && strlen((*it).c_str()) > 32 && strlen((*it).c_str()) < 83)
					{
						// 判断是否需要拼接
						bool flag_multiple;
						if ((*it).c_str()[7] == '1')
						{
							flag_multiple = false;
						}
						else
						{
							flag_multiple = true;
						}

						int len = strlen((*it).c_str());
						memset(messageStr, 0, 64);

						// 取第五个逗号后面的信息数据
						for (i = 0, j = 0, k = 0; i < len; i++)
						{
							if ((*it).c_str()[i] == ',')
							{
								j++;
							}

							if (j == 5 && (*it).c_str()[i] != ',')
							{
								messageStr[k++] = (*it).c_str()[i];
							}
						}

						//校验格式!aaccc,x,y,z,u,c-c,v*hh,j为逗号的个数
						if (j != 6 || (*it).c_str()[len - 3] != '*')
						{
							continue;
						}

						messageStr[k] = '\0';

						// 存入需要拼接的消息内容
						if (flag_multiple)
						{
							if ((*it).c_str()[13] == 'A')
							{
								mapMultipleMessageClassA.insert(pair<string, string>((*it).substr(9, 3), messageStr));
							}
							else
							{
								mapMultipleMessageClassB.insert(pair<string, string>((*it).substr(9, 3), messageStr));
							}

							// 判断是否为最后一条语句，不是继续接收，是则拼接并发送到解析线程
							if ((int)(*it).c_str()[9] < (int)(*it).c_str()[7])
							{
								continue;
							}
							else
							{
								char sort = '1';

								for (int i = 0; i < (*it).c_str()[7] - '0'; i++)
								{
									string index = str_utils::toString(sort) + ',' + (*it).c_str()[11];

									if ((*it).c_str()[13] == 'A')
									{
										messageStrMultiple += mapMultipleMessageClassA[index];
										mapMultipleMessageClassA.erase(index);
									}
									else
									{
										messageStrMultiple += mapMultipleMessageClassB[index];
										mapMultipleMessageClassB.erase(index);
									}

									sort += 1 ;
								}
							}
						}

						try {
							aisMessageParser.clearData();
							string aisString = (*it).c_str();

							if (flag_multiple) {
								aisMessageParser.addData(messageStrMultiple);
								aisMessage = aisMessageParser.parseMessage();
								flag_multiple = false;
							}
							else {
								aisMessageParser.addData(messageStr);
								aisMessage = aisMessageParser.parseMessage();
							}
							if (aisMessage.getMMSI() == -1){
								continue;
							}

							if (aisMessage.getMESSAGETYPE() == 1 || aisMessage.getMESSAGETYPE() == 2 ||
								aisMessage.getMESSAGETYPE() == 3 || aisMessage.getMESSAGETYPE() == 5 ||
								aisMessage.getMESSAGETYPE() == 14 || aisMessage.getMESSAGETYPE() == 18 ||
								aisMessage.getMESSAGETYPE() == 19 || aisMessage.getMESSAGETYPE() == 24)
							{
								// 插入到vAISInfo，由数据库线程统一插入
								lock_guard<mutex> lockVector(mutexCache);
								vAISInfo.push_back({++AISInitialID, aisString, aisMessage});
							}
						}
						catch (...){
							cout << "parseMessage捕获错误了..." << endl;
							continue;
						}
					}
				}
                unique_lock<mutex> dbLock(mutexDBOperate);
                condDBOperate.notify_all();
			}
			//end iterate vAisdata

			catch (...){
				cout << "HandleData捕获错误了..." << endl;
			}
		}
	}
	return true;
}

CClient::~CClient() {
#ifdef WIN32
	closesocket(m_socket);
#else
	close(m_socket);
#endif
//cout << "destruction" << endl;
}



