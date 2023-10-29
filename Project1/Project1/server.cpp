#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include<windows.h>
#include <fstream>
#include<sstream>
#include<cstdio>
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996) 
#define SERVER_PORT 12340 //�˿ں�
#define SERVER_IP "0.0.0.0" //IP ��ַ
using namespace std;
const int BUFFER_LENGTH = 1027; //��������С������̫���� UDP ������֡�а�����ӦС�� 1480 �ֽڣ�
const int SEND_WIND_SIZE = 10;//���ʹ��ڴ�СΪ 10��GBN ��Ӧ���� W + 1 <= N��W Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����
//����ȡ���к� 0...19 �� 20 ��
//��������ڴ�С��Ϊ 1����Ϊͣ-��Э��
const int SEQ_SIZE = 20; //���кŵĸ������� 0~19 ���� 20 ��
//���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
//��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ
BOOL ack[SEQ_SIZE];//ack����
int curSeq;//��ǰ���ݰ��� seq
int curAck;//��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;//�յ��İ�������
int totalPacket;//��Ҫ���͵İ�����
int waitSeq;
//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: ��ȡ��ǰϵͳʱ�䣬������� ptime ��
// Parameter: char * ptime
//************************************
void getCurTime(char* ptime) {
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	SYSTEMTIME sys;
	GetLocalTime(&sys);
	sprintf_s(buffer, "%4d/%02d/%02d %02d:%02d:%02d",
		sys.wYear,
		sys.wMonth,
		sys.wDay,
		sys.wHour,
		sys.wMinute,
		sys.wSecond);
	strcpy_s(ptime, sizeof(buffer), buffer);
}
//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public 
// Returns: bool
// Qualifier: ��ǰ���к� curSeq �Ƿ����
//************************************
bool seqIsAvailable() {
	int step;
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//���к��Ƿ��ڵ�ǰ���ʹ���֮��
	if (step >= SEND_WIND_SIZE) {
		return false;
	}
	if (ack[curSeq]) {
		return true;
	}
	return false;
}
//************************************
// Method: timeoutHandler
// FullName: timeoutHandler
// Access: public
// Returns: void
// Qualifier: ��ʱ�ش������������������ڵ�����֡��Ҫ�ش�
//************************************
void timeoutHandler() {
	printf("Timer out error.\n");
	int index;
	for (int i = 0; i < (curSeq - curAck + SEQ_SIZE) % SEQ_SIZE; ++i) {
		index = (i + curAck) % SEQ_SIZE;
		ack[index] = TRUE;
	}
	totalSeq = totalSeq - ((curSeq - curAck + SEQ_SIZE) % SEQ_SIZE);
	curSeq = curAck;
}
//************************************
// Method: ackHandler
// FullName: ackHandler
// Access: public 
// Returns: void
// Qualifier: �յ� ack���ۻ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ�
//���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0��ASCII��ʱ����ʧ�ܣ���˼�һ�ˣ��˴���Ҫ��һ��ԭ
// Parameter: char c
//************************************
void ackHandler(char c) {
	unsigned char index = (unsigned char)c - 1; //���кż�һ
	printf("Recv a ack of %d\n", index);
	if (curAck <= index) {
		for (int i = curAck; i <= index; ++i) {
			ack[i] = TRUE;
		}
		curAck = (index + 1) % SEQ_SIZE;
	}
	else {
		//ack ���������ֵ���ص��� curAck �����
		for (int i = curAck; i < SEQ_SIZE; ++i) {
			ack[i] = TRUE;
		}
		for (int i = 0; i <= index; ++i) {
			ack[i] = TRUE;
		}
		curAck = index + 1;
	}
}
//************************************
// Method: lossInLossRatio
// Access: public 
// Returns: BOOL
// Qualifier: ģ���������������TRUE��ִ�ж���
// Parameter: float lossRatio
//************************************
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 100;
	if (r < lossBound) {
		return TRUE;
	}
	return FALSE;
}
//������
int main()
{
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//�����׽���Ϊ������ģʽ
	int iMode; //1����������0������
	SOCKADDR_IN addrServer; //��������ַ
	//addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//���߾���
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		printf("Could not bind the port %d for socket.Error code is % d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	SOCKADDR_IN addrClient; //�ͻ��˵�ַ
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //���ݷ��ͽ��ջ�����
	ZeroMemory(buffer, sizeof(buffer));
	//���������ݶ����ڴ�
	int recvSize;
	int loct = 0;
	int waitCount = 0;
	float packetLossRatio = 0.2; //Ĭ�ϰ���ʧ�� 0.2
	float ackLossRatio = 0.2; //Ĭ�� ACK ��ʧ�� 0.2
	srand((unsigned)time(NULL));
	while (true) {
		//���������գ���û���յ����ݣ�����ֵΪ-1
		recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0) {
			getCurTime(buffer);
		}
		else if (strcmp(buffer, "-quit") == 0) {
			strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
		}
		else {
			char filename[100];
			char operation[10];
			char cmd[10];
			int ret;
			unsigned char u_code;//״̬��
			unsigned short seq;//�������к�
			unsigned short recvSeq;//���մ��ڴ�СΪ 1����ȷ�ϵ����к�
			unsigned short waitSeq;//�ȴ������к�
			unsigned short recvPacket;
			int sendack = 0;
			int stage = 0;
			ret = sscanf(buffer, "%s %f %f %s %s", &cmd, &packetLossRatio, &ackLossRatio, &operation, &filename);
			if (!strcmp(cmd, "gbn")) {
				if (!strcmp(operation, "download")) {
					iMode = 1;
					int flg = 1;
					ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);
					std::ifstream fin;
					fin.open(filename, ios_base::in);
					if (!fin.is_open()) {
						printf("�޷����ļ�\n");
						iMode = 0;
						ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);
						continue;
					}
					char buff[1024] = { 0 };
					char data[1024 * 113];
					loct = 0;
					while (fin.getline(buff, sizeof(buff))) {
						if (buff[0] == '0') break;
						memcpy(data + 1024 * loct, buff, 1024);
						++loct;
					}
					fin.close();
					totalPacket = loct;
					ZeroMemory(buffer, sizeof(buffer));
					int recvSize;
					waitCount = 0;
					printf("Begain to test GBN protocol,please don't abort the process\n");
					//������һ�����ֽ׶�
					//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
						//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
						//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
					printf("Shake hands stage\n");
					int stage = 0;
					bool runFlag = true;
					while (runFlag) {
						switch (stage) {
						case 0://���� 205 �׶�
							buffer[0] = 205;
							sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							Sleep(100);
							stage = 1;
							break;
						case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
							recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
							if (recvSize < 0) {
								++waitCount;
								if (waitCount > 20) {
									runFlag = false;
									printf("Timeout error\n");
									break;
								}
								Sleep(500);
								continue;
							}
							else {
								if ((unsigned char)buffer[0] == 200) {
									printf("Begin a file transfer\n");
									printf("File size is %dB, each packet is 1024B and packet total num is % d\n", totalPacket * 1024, totalPacket);
									curSeq = 0;                                           
									curAck = 0;
									totalSeq = 0;
									waitCount = 0;
									stage = 2;
									for (int i = 0; i < SEQ_SIZE; ++i) {
										ack[i] = TRUE;
									}
								}
							}
							break;
						case 2://���ݴ���׶�
							if (seqIsAvailable() && totalSeq < loct) {
								//���͸��ͻ��˵����кŴ� 1 ��ʼ
								buffer[0] = curSeq + 1;
								if (totalSeq == loct - 1) buffer[1] = '0';
								else buffer[1] = '1';
								ack[curSeq] = FALSE;
								//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������	
								memcpy(&buffer[2], data + 1024 * totalSeq, 1024);
								printf("send a packet with a seq of %d\n", curSeq);
								sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
								++curSeq;
								curSeq %= SEQ_SIZE;
								++totalSeq;
								Sleep(500);
							}
							//�ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
							recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
							if (recvSize < 0) {
								waitCount++;
								//20 �εȴ� ack ��ʱ�ش�
								if (waitCount > 20) {
									timeoutHandler();
									waitCount = 0;
								}
							}
							else {
								//�յ� ack
								if (buffer[1] == '0')
								{
									flg = 0;
									break;
								}
								ackHandler(buffer[0]);
								waitCount = 0;
							}
							Sleep(500);
							break;
						}
						if (flg == 0) break;
					}
					if (flg == 0) {
						printf("�������\n");
						iMode = 0;
						ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);
						ZeroMemory(buffer, sizeof(buffer));
						continue;
					}
				}
				else if (!strcmp(operation, "upload")) {
					char data[1024 * 113];
					loct = 0;
					int flg = 1;
					BOOL b;
					//gbn 0 0 download test.txt
					while (true) {
						//�ȴ� server �ظ����� UDP Ϊ����ģʽ
						recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, &length);
						switch (stage) {
						case 0://�ȴ����ֽ׶�
							u_code = (unsigned char)buffer[0];
							if ((unsigned char)buffer[0] == 205) {
								printf("Ready for file transmission\n");
								buffer[0] = 200;
								buffer[1] = '\0';
								sendto(sockServer, buffer, 2, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
								stage = 1;
								recvSeq = 0;
								waitSeq = 1;
								loct = 0;
							}
							break;
						case 1://�ȴ��������ݽ׶�
							seq = (unsigned short)buffer[0];
							b = lossInLossRatio(packetLossRatio);
							if (b) {
								printf("The packet with a seq of %d loss\n", seq - 1);
								continue;
							}
							printf("recv a packet with a seq of %d\n", seq - 1);
							//������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
							if (!(waitSeq - seq)) {
								if (buffer[1] == '0') flg = 0;
								memcpy(data + 1024 * loct, buffer + 2, 1024);
								++loct;
								++waitSeq;
								if (waitSeq == 21) {
									waitSeq = 1;
								}
								//�������
								//printf("%s\n",&buffer[1]);
								buffer[0] = seq;
								recvSeq = seq;
								recvPacket = (unsigned short)buffer[1];
								buffer[2] = '\0';
							}
							else {
								//�����ǰһ������û���յ�����ȴ� Seq Ϊ 1 �����ݰ��������򲻷��� ACK����Ϊ��û����һ����ȷ�� ACK��
								if (!recvSeq) {
									continue;
								}
								buffer[0] = recvSeq;
								buffer[1] = recvPacket;
								buffer[2] = '\0';
							}
							b = lossInLossRatio(ackLossRatio);
							if (b) {
								printf("The ack of %d loss\n", (unsigned char)buffer[0] - 1);
								continue;
							}
							sendto(sockServer, buffer, 3, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							printf("send a ack of %d\n", (unsigned char)buffer[0] - 1);
							break;
						}
						if (flg == 0) {
							printf("�������\n");
							break;
						}
						Sleep(500);
					}
					ofstream ofs;
					ofs.open(filename, ios::out);
					char buff[1300];
					printf("%d", loct);
					for (int i = 0; i < loct; ++i) {
						memcpy(buff, data + 1024 * i, 1024);
						ofs << buff << endl;
					}
					ofs.close();
					if (flg == 0) {
						ZeroMemory(buffer, sizeof(buffer));
						continue;
					}
				}
			}
			else if (!strcmp(cmd, "sr"))
			{
				if (!strcmp(operation, "download")) {
					iMode = 1;
					ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);
					std::ifstream fin;
					fin.open(filename, ios_base::in);
					if (!fin.is_open()) {
						printf("�޷����ļ�");
						iMode = 0;
						ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);
						continue;
					}
					char buff[1024] = { 0 };
					char data[1024 * 113];
					loct = 0;
					while (fin.getline(buff, sizeof(buff))) {
						if (buff[0] == '0') break;
						memcpy(data + 1024 * loct, buff, 1024);
						++loct;
					}
					fin.close();
					totalPacket = loct;
					ZeroMemory(buffer, sizeof(buffer));
					int recvSize;
					int waitCounts[21] = { 0 };
					waitCount = 0;
					printf("Begain to test SR protocol,please don't abort the process\n");
					//������һ�����ֽ׶�
					//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬��,��ʾ������׼�����ˣ����Է�������
						//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
						//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
					printf("Shake hands stage\n");
					int stage = 0;
					bool runFlag = true;
					while (runFlag) {
						switch (stage) {
						case 0://���� 205 �׶�
							buffer[0] = 205;
							sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							Sleep(100);
							stage = 1;
							break;
						case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
							recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
							if (recvSize < 0) {
								++waitCount;
								if (waitCount > 20) {
									runFlag = false;
									printf("Timeout error\n");
									break;
								}
								Sleep(500);
								continue;
							}
							else {
								if ((unsigned char)buffer[0] == 200) {
									printf("Begin a file transfer\n");
									printf("File size is %dB, each packet is 1024B and packet total num is % d\n", totalPacket * 1024, totalPacket);
									curSeq = 0;
									curAck = 0;
									totalSeq = 0;
									waitCount = 0;
									waitSeq = 0;
									stage = 2;
									for (int i = 0; i < SEQ_SIZE; ++i) {
										ack[i] = TRUE;
									}
								}
							}
							break;
						case 2:
							if (seqIsAvailable() && totalSeq < loct) {

								//���͸��ͻ��˵����кŴ� 1 ��ʼ
								buffer[0] = curSeq + 1;
								if (totalSeq == loct - 1) buffer[1] = '0';
								else buffer[1] = '1';
								ack[curSeq] = FALSE;
								//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������	
								memcpy(&buffer[2], data + 1024 * totalSeq, 1024);
								printf("send a packet with a seq of %d\n", curSeq);
								sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
								++curSeq;
								curSeq %= SEQ_SIZE;
								++totalSeq;
								Sleep(500);
							}
							//�ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
							recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
							if (recvSize >= 0) {
								//�յ� ack
								int i = (int)(buffer[0]) - 1;
								ack[i] = TRUE;
								waitCounts[i] = 0;
								printf("Recv a ack of %d\n", i);
								if (i == curAck) {
									if (curSeq < curAck) {
										for (; curAck < SEQ_SIZE;) {
											if (ack[curAck]) ++curAck;
											else break;
										}
										if (curAck == SEQ_SIZE) {
											for (curAck = 0; curAck < curSeq;) {
												if (ack[curAck]) ++curAck;
												else break;
											}
										}
									}
									else {
										for (; curAck < curSeq;) {
											if (ack[curAck]) ++curAck;
											else break;
										}
									}
								}
								if (curAck == curSeq && totalSeq == loct) break;
							}
							int index;
							for (int i = 0; i < (curSeq - curAck + SEQ_SIZE) % SEQ_SIZE; ++i) {
								index = (i + curAck) % SEQ_SIZE;
								if (!ack[index]) {
									++waitCounts[index];
									if (waitCounts[index] > 20) {
										buffer[0] = index + 1;
										if (totalSeq - ((curSeq - curAck + SEQ_SIZE) % SEQ_SIZE) + i == loct - 1) buffer[1] = '0';
										else buffer[1] = '1';
										memcpy(&buffer[2], data + 1024 * (totalSeq - ((curSeq - curAck + SEQ_SIZE) % SEQ_SIZE) + i), 1024);
										printf("send a packet with a seq of %d\n", index);
										sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
										waitCounts[index] = 0;
									}
								}
							}
							Sleep(500);
							break;
						}
						if (curAck == curSeq && totalSeq == loct) break;
					}
					if (curAck == curSeq && totalSeq == loct) {
						printf("�������\n");
						iMode = 0;
						ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);
						ZeroMemory(buffer, sizeof(buffer));
						continue;
					}
				}
				else if (!strcmp(operation, "upload")) {
					//sr 0 0 download test.txt
					char data[1024 * 113];
					BOOL recvd[20] = { FALSE };
					sendack = 0;
					BOOL b;
					while (true) {
						//�ȴ� server �ظ����� UDP Ϊ����ģʽ
						recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, &length);
						switch (stage) {
						case 0://�ȴ����ֽ׶�
							u_code = (unsigned char)buffer[0];
							if ((unsigned char)buffer[0] == 205) {
								printf("Ready for file transmission\n");
								buffer[0] = 200;
								buffer[1] = '\0';
								sendto(sockServer, buffer, 2, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
								stage = 1;
								recvSeq = 0;
								waitSeq = 0;
								totalSeq = 0;
								loct = -2;
							}
							break;
						case 1://�ȴ��������ݽ׶�
							seq = (unsigned short)buffer[0];
							//�����ģ����Ƿ�ʧ
							b = lossInLossRatio(packetLossRatio);
							if (b) {
								printf("The packet with a seq of %d loss\n", seq - 1);
								continue;
							}
							printf("recv a packet with a seq of %d\n", seq - 1);
							//������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
							seq -= 1;
							if (!(waitSeq - seq)) {
								recvd[waitSeq] = TRUE;
								memcpy(data + 1024 * totalSeq, buffer + 2, 1024);
								if (buffer[1] == '0') loct = totalSeq;
								int cnt = 10;
								while (cnt--) {
									if (recvd[waitSeq]) {
										recvd[waitSeq] = FALSE;
										++waitSeq;
										++totalSeq;
										if (waitSeq == 20) waitSeq = 0;
									}
									else break;
								}
							}
							else {
								int index = (seq + SEQ_SIZE - waitSeq) % SEQ_SIZE;
								if (index < 10 && !recvd[seq]) {
									recvd[seq] = TRUE;
									memcpy(data + 1024 * (totalSeq + index), buffer + 2, 1024);
									if (buffer[1] == '0') loct = totalSeq + index;
								}
							}
							buffer[0] = (char)(seq + 1);
							buffer[2] = '\0';
							b = lossInLossRatio(ackLossRatio);
							if (b) {
								printf("The ack of %d loss\n", (unsigned char)buffer[0] - 1);
								continue;
							}
							++sendack;
							sendto(sockServer, buffer, 3, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							printf("send a ack of %d\n", (unsigned char)buffer[0] - 1);
							break;
						}
						if (sendack == loct + 1) {
							printf("�������\n");
							break;
						}
						Sleep(500);
					}
					char buff[1300];
					ofstream ofs;
					ofs.open(filename, ios::out);
					for (int i = 0; i <= loct; ++i) {
						memcpy(buff, data + 1024 * i, 1024);
						ofs << buff << endl;
					}
					ofs.close();
					if (sendack == loct + 1) {
						ZeroMemory(buffer, sizeof(buffer));
						continue;
					}
				}
			}
		}
		
		sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
		Sleep(500);
	}
	//�ر��׽��֣�ж�ؿ�
	closesocket(sockServer);
	WSACleanup();
	return 0;
}
