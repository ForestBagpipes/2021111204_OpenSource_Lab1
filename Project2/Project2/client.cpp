#include <stdlib.h>
#include <WinSock2.h>
#include <time.h>
#include <stdio.h>
#include<fstream>
#include<sstream>
#include<cstdio>
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996) 
#define SERVER_PORT 12340 //�������ݵĶ˿ں�
#define SERVER_IP "127.0.0.1" // �������� IP ��ַ
using namespace std;
const int BUFFER_LENGTH = 1027;
const int SEQ_SIZE = 20;//���ն����кŸ�����Ϊ 1~20
BOOL ack[SEQ_SIZE];//�յ� ack �������Ӧ 0~19 �� ack
int curSeq;//��ǰ���ݰ��� seq
int curAck;//��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;//�յ��İ�������
int totalPacket;//��Ҫ���͵İ�����
int waitSeq;
const int SEND_WIND_SIZE = 10;
/****************************************************************/
/* -time �ӷ������˻�ȡ��ǰʱ��
-quit �˳��ͻ���
-testgbn [X] ���� GBN Э��ʵ�ֿɿ����ݴ���
[X] [0,1] ģ�����ݰ���ʧ�ĸ���
[Y] [0,1] ģ�� ACK ��ʧ�ĸ���
*/
/****************************************************************/
void printTips() {
	printf("| -time to get current time |\n");
	printf("| -quit to exit client |\n");
	printf("| gbn + [X] +[Y] + op  +filename |\n");
	printf("| sr + [X] +[Y] + op  +filename  |\n");
	printf("*****************************************\n");
}
//************************************
// Method: lossInLossRatio
// FullName: lossInLossRatio
// Access: public 
// Returns: BOOL
// Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻� FALSE
// Parameter: float lossRatio [0,1]
//************************************
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 100;
	if (r < lossBound) {
		return TRUE;
	}
	return FALSE;
}
//************************************
// Method: seqIsAvailable
// Access: public 
// Returns: BOOL
// Qualifier: �ж����к��Ƿ��ڴ����У�����TRUE��˵��������
// Parameter: nothing
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
// Access: public 
// Returns: void
// Qualifier: ��ʱ����seq�����ش���
// Parameter: nothing
//************************************
void timeoutHandler() {
	printf("Timer out error.\n");
	int index;
	for (int i = 0; i < (curSeq - curAck + SEQ_SIZE) % SEQ_SIZE; ++i) {
		index = (i + curAck) % SEQ_SIZE;
		ack[index] = TRUE;
	}
	totalSeq -= ((curSeq - curAck + SEQ_SIZE) % SEQ_SIZE);
	curSeq = curAck;
}
//************************************
// Method: ackHandler
// Access: public 
// Returns: void
// Qualifier: ���յ�ack������
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
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//���ջ�����
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//Ϊ�˲���������������ӣ�����ʹ�� -time ����ӷ������˻�õ�ǰʱ��
		//ʹ�� -testgbn [X] [Y] ���� GBN ����[X]��ʾ���ݰ���ʧ����
		// [Y]��ʾ ACK ��������
	int ret;
	int interval = 1;//�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ�������� ack��0 ���߸�������ʾ���еĶ������� ack
	char cmd[128];
	float packetLossRatio = 0.2; //Ĭ�ϰ���ʧ�� 0.2
	float ackLossRatio = 0.2; //Ĭ�� ACK ��ʧ�� 0.2
	char operation[10];
	char filename[100];
	int sendack = 0;
	int iMode = 0;
	int loct = 0;
	int waitCount = 0;

	srand((unsigned)time(NULL));
	while (true) {
		printTips();
		gets_s(buffer);
		ret = sscanf(buffer, "%s %f %f %s %s", &cmd, &packetLossRatio, &ackLossRatio, &operation, &filename);
		if (!strcmp(cmd, "sr")) {
			printf("%s\n", "Begin SR protocol, please don't abort the process");
			printf("The loss ratio of packet is %.2f,the loss ratio of ack is % .2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//״̬��
			unsigned short seq;//�������к�
			unsigned short recvSeq;//���մ��ڴ�СΪ 1����ȷ�ϵ����к�
			unsigned short waitSeq;//�ȴ������к�
			sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			if (!strcmp(operation, "download")) {
				//sr 0.2 0.2 download testdownload.txt
				char data[1024 * 113];
				BOOL recvd[20] = { FALSE };
				iMode = 0;
				ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);
				sendack = 0;
				while (true) {

					recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					switch (stage) {
					case 0://�ȴ����ֽ׶�
						u_code = (unsigned char)buffer[0];
						if ((unsigned char)buffer[0] == 205) {
							printf("Ready for file transmission\n");
							buffer[0] = 200;
							buffer[1] = '\0';
							sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
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
						sendto(socketClient, buffer, 3, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						printf("send a ack of %d\n", (unsigned char)buffer[0] - 1);
						break;
					}
					if (sendack == loct + 1) {
						printf("�������\n");
						break;
					}
					Sleep(20);
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
			else if (!(strcmp(operation, "upload"))) {
				iMode = 1;
				ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);
				std::ifstream fin;
				fin.open(filename, ios_base::in);
				if (!fin.is_open()) {
					printf("�޷����ļ�\n");
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
				fin.close();//read file
				totalPacket = loct;
				ZeroMemory(buffer, sizeof(buffer));
				int recvSize;
				int waitCounts[21] = { 0 };
				waitCount = 0;
				printf("Begain to test SR protocol,please don't abort the process\n");

				printf("Shake hands stage\n");
				int stage = 0;
				bool runFlag = true;
				while (runFlag) {
					switch (stage) {
					case 0://���� 205 �׶�
						buffer[0] = 205;
						sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						Sleep(100);
						stage = 1;
						break;
					case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrServer), &len);
						if (recvSize < 0) {
							++waitCount;
							if (waitCount > 20) {
								runFlag = false;
								printf("Timeout error\n");
								break;
							}
							Sleep(20);
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
							memcpy(&buffer[2], data + 1024 * totalSeq, 1024);
							printf("send a packet with a seq of %d\n", curSeq);
							sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							Sleep(20);
						}
						//�ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrServer), &len);
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
						//time out
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
									sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
									waitCounts[index] = 0;
								}
							}
						}
						Sleep(20);
						break;
					}
					if (curAck == curSeq && totalSeq == loct) break;
				}
				if (curAck == curSeq && totalSeq == loct) {
					printf("�������\n");
					ZeroMemory(buffer, sizeof(buffer));
					continue;
				}
			}
		}
		else if (!strcmp(cmd, "gbn")) {
			printf("%s\n", "Begin GBN protocol, please don't abort the process");
			printf("The loss ratio of packet is %.2f,the loss ratio of ack is % .2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//״̬��
			unsigned short seq;//�������к�
			unsigned short recvSeq;//���մ��ڴ�СΪ 1����ȷ�ϵ����к�
			unsigned short waitSeq;//�ȴ������к�
			unsigned short recvPacket;
			sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			if (!strcmp(operation, "download")) {
				char data[1024 * 113];
				loct = 0;
				iMode = 0;
				int flg = 1;
				ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);
				while (true) {
					//�ȴ� server �ظ����� UDP Ϊ����ģʽ
					recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					switch (stage) {
					case 0://�ȴ����ֽ׶�
						u_code = (unsigned char)buffer[0];
						if ((unsigned char)buffer[0] == 205) {
							printf("Ready for file transmission\n");
							buffer[0] = 200;
							buffer[1] = '\0';
							sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
							stage = 1;
							recvSeq = 0;
							waitSeq = 1;
							loct = 0;
						}
						break;
					case 1://�ȴ��������ݽ׶�
						seq = (unsigned short)buffer[0];
						//ģ���������
						b = lossInLossRatio(packetLossRatio);
						if (b) {
							printf("The packet with a seq of %d loss\n", seq - 1);
							continue;
						}
						printf("recv a packet with a seq of %d\n", seq - 1);
						//������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
						if (!(waitSeq - seq)) {
							memcpy(data + 1024 * loct, buffer + 2, 1024);
							if (buffer[1] == '0') flg = 0;
							++loct;
							++waitSeq;
							if (waitSeq == 21) {
								waitSeq = 1;
							}
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
						sendto(socketClient, buffer, 3, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						printf("send a ack of %d\n", (unsigned char)buffer[0] - 1);
						break;
					}
					if (flg == 0) {
						printf("�������\n");
						break;
					}
					Sleep(20);
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
			else if (!strcmp(operation, "upload")) {
				std::ifstream fin;
				fin.open(filename, ios_base::in);
				if (!fin.is_open()) {
					printf("�޷����ļ�");
					continue;
				}
				iMode = 1;
				ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);
				char buff[1024] = { 0 };
				char data[1024 * 113];
				loct = 0;
				int flg = 1;
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
				//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬���ʾ������׼�����ˣ����Է�������
					//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
					//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
				printf("Shake hands stage\n");
				int stage = 0;
				bool runFlag = true;
				while (runFlag) {
					switch (stage) {
					case 0://���� 205 �׶�
						buffer[0] = 205;
						sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						Sleep(100);
						stage = 1;
						break;
					case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrServer), &len);
						if (recvSize < 0) {
							++waitCount;
							if (waitCount > 20) {
								runFlag = false;
								printf("Timeout error\n");
								break;
							}
							Sleep(20);
							continue;
						}
						else {
							if ((unsigned char)buffer[0] == 200) {
								printf("Begin a file transfer\n");
								printf("File size is %dB, each packet is 1024B and packet total num is % d\n", totalPacket * 1024, totalPacket);
								//׼�����䣬��ʼ��
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
							memcpy(&buffer[2], data + 1024 * totalSeq, 1024);
							printf("send a packet with a seq of %d\n", curSeq);
							sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							Sleep(20);
						}
						//�ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrServer), &len);
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
						Sleep(20);
						break;
					}
					if (flg == 0) break;
				}
				if (flg == 0) {
					printf("�������\n");
					ZeroMemory(buffer, sizeof(buffer));
					continue;
				}
			}
		}
		sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!")) {
			break;
		}
	}
	//�ر��׽���
	closesocket(socketClient);
	WSACleanup();
	return 0;
}
