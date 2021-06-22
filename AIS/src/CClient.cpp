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

using namespace std;




CClient::CClient(const SOCKET sClient, const sockaddr_in &addrClient) {
	//��ʼ������
	m_socket = sClient;
	m_addr = addrClient;
	m_bConning = FALSE;
	m_bExit = FALSE;

}

bool CClient::StartRuning() {
	if (!licenseOK){
		cout << "�Բ��𣬻�ȡ��Ȩ���󣬲�������������" << endl;
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

				if (nErrCode == WSAEWOULDBLOCK)						//�������ݻ�����������
				{
					sleep(TIMEFOR_THREAD_CLIENT);						//�߳�˯�ߵȴ��ͻ�������

					continue;											//����ѭ��
				}
				else if (WSAENETDOWN == nErrCode						//�ͻ��˹ر�������
						 || WSAETIMEDOUT == nErrCode
						 || WSAECONNRESET == nErrCode)
				{
					cout << "-------�ͻ��˹ر�������:" << pClient->Getm_addr() << "|nErrCode is:" << nErrCode << "-------" << endl;
					break;												//�߳��˳�
				}
			}
			//�ͻ��˹ر�������
			else if (retVal == 0) {
				cout << "-------�ͻ��˹ر�������retVal=0-------" << endl;
				break;
			}
			else {
				pClient->ParseData(buf);
			}

			sleep(TIMEFOR_THREAD_CLIENT);

#else
			// TODO check
			retVal = recv(pClient->m_socket, buf, MAX_NUM_BUF, MSG_DONTWAIT);
			buf[MAX_NUM_BUF] = '\0';
			if (retVal == 0)
			{
				cout << "-------�ͻ��˹ر�������:" << pClient->Getm_addr() << "------" <<endl;
				break; //�߳��˳�
			}
			else if(retVal == SOCKET_ERROR)
			{
				if (errno == EWOULDBLOCK)
				{
					usleep(1000 * TIMEFOR_THREAD_CLIENT);
					continue;
				}
				else {
					cout << "-------Client: TCP recv error------" << endl;
					perror("");
				}
			}
#endif

		}
		catch (...) {
			cout << "RecvDataThread���������..." << endl;;
		}
	}
	pClient->m_bConning = false;
	free(pClient);
	return;
}

bool CClient::ParseData(const char *pRecvData) {

	//���"byebye" ���� "Byebye"
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
				//���س����з�\r\n�ָ��ַ��������Ϊvector
				string strAisMessage(aisMessageStr);
				//ͳһ����ָ������\r\n
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

				// ���ڴ洢��Ҫƴ�ӵ��ַ���
				map<string, string> mapMultipleMessageClassA;
				map<string, string> mapMultipleMessageClassB;

				for (std::vector<string>::iterator it = vAisData.begin(); it != vAisData.end(); ++it)
				{
					if ((str_utils::startsWith(*it, "!AIVDM") || str_utils::startsWith(*it, "!AIVDO")) && strlen((*it).c_str()) > 32 && strlen((*it).c_str()) < 83)
					{
						// �ж��Ƿ���Ҫƴ��
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

						// ȡ��������ź������Ϣ����
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

						//У���ʽ!aaccc,x,y,z,u,c-c,v*hh,jΪ���ŵĸ���
						if (j != 6 || (*it).c_str()[len - 3] != '*')
						{
							continue;
						}

						messageStr[k] = '\0';

						// ������Ҫƴ�ӵ���Ϣ����
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

							// �ж��Ƿ�Ϊ���һ����䣬���Ǽ������գ�����ƴ�Ӳ����͵������߳�
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
								// ���뵽vAISInfo�������ݿ��߳�ͳһ����
								lock_guard<mutex> lockVector(mutexCache);
								vAISInfo.push_back({++AISInitialID, aisString, aisMessage});
								unique_lock<mutex> dbLock(mutexDBOperate);
								condDBOperate.notify_all();
							}


						}
						catch (...){
							cout << "parseMessage���������..." << endl;
							continue;
						}
					}
				}
			}
			//end iterate vAisdata

			catch (...){
				cout << "HandleData���������..." << endl;
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
	m_bExit = true;
}



