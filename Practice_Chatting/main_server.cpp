#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <vector>
#include <thread>
#include <process.h>

#include "protocol.h"
#pragma comment(lib,"ws2_32.lib")

// ����

// � I/O �۾����� �����ϱ� ���� ������
enum class EIoOperation
{
	IO_ACCEPT, // IOCP������ accept ���� �񵿱�� ó���� �� �ֽ��ϴ�. (��� ����)
	IO_RECV,
	IO_SEND
};

// I/O �۾��� �ʿ��� �����͸� ��� Ȯ�� Overlapped ����ü
struct OverlappedEx
{
	OVERLAPPED overlapped;      // ��� �񵿱� I/O�� �⺻�� �Ǵ� ����ü
	EIoOperation ioType;        // �� Overlapped ��ü�� � ������ �۾��� ���Ǿ����� ����
	void* session;              // �� I/O �۾��� � ���ǿ� �����ִ����� ����Ű�� ������

	// WSABUF�� ������ �ּҿ� ���̸� ��� ����ü�Դϴ�.
	// WSARecv, WSASend�� �� ������ ���� �����͸� �аų� ���ϴ�.
	WSABUF wsaBuf;
	char buffer[1024];
};

// --- Room ����ü�� RoomList�� ���� �߰��մϴ� ---
struct Room; // Session ����ü���� Room�� �����ؾ� �ϹǷ� ���� ����


// Ŭ���̾�Ʈ �ϳ��� ������ ��� ���� ����ü
struct Session
{
	SOCKET socket;
	OverlappedEx recvOverlapped;

	char recvBuffer[4096]; // �� ���Ǻ� ���� ����
	int recvBytes;         // ������� ������ ����Ʈ ��
	char nickname[NICKNAME_LENGTH];
	Room* pRoom; // --- �ڽ��� ���� ���� ����Ű�� ������ �߰� ---

	// �����ڿ��� ��� �������� �ʱ�ȭ�մϴ�.
	Session() : socket(INVALID_SOCKET), recvBytes(0), pRoom(nullptr) // pRoom �ʱ�ȭ �߰�
	{
		ZeroMemory(&recvOverlapped.overlapped, sizeof(recvOverlapped.overlapped));
		recvOverlapped.ioType = EIoOperation::IO_RECV;
		recvOverlapped.session = this;
		ZeroMemory(recvBuffer, sizeof(recvBuffer)); // ���� �ʱ�ȭ
		ZeroMemory(nickname, sizeof(nickname)); // �г��� �ʱ�ȭ �߰�
	}
};

// ä�ù� �ϳ��� ������ ��� ����ü
struct Room
{
	int roomId;
	char title[ROOM_TITLE_LENGTH];
	std::vector<Session*> sessions; // �� �濡 ���� ���ǵ��� ���

	Room() : roomId(-1)
	{
		ZeroMemory(title, sizeof(title));
	}
};

// Session List Vector
std::vector<Session*> SessionList; 
std::vector<Room*> RoomList; // --- RoomList ���� ���� �߰� ---
int g_nextRoomId = 1; // ������ ������ �濡 �ο��� ���� ID

// Lock
CRITICAL_SECTION g_cs;



unsigned int __stdcall WorkerThread(void* pArguments)
{
	HANDLE hIocp = (HANDLE)pArguments;

	while (true)
	{
		OverlappedEx* pOverlappedEx = nullptr;
		Session* pSession = nullptr;
		DWORD bytesTransferred = 0;

		BOOL ret = GetQueuedCompletionStatus(hIocp, &bytesTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlappedEx, INFINITE);

		// --- ���� ��� (���� ���� �Ǵ� ����) ---
		if (ret == FALSE || bytesTransferred == 0)
		{
			if (pSession != nullptr) // pSession�� NULL�� �ƴ� ���� ó��
			{
				std::cout << pSession->socket << " socket disconnected" << std::endl;

				// ������ �濡 �� �־��ٸ�, �濡�� ���� ó��
				if (pSession->pRoom != nullptr)
				{
					Room* pRoom = pSession->pRoom;
					EnterCriticalSection(&g_cs);
					// ���� ���� ��Ͽ��� �ش� ������ ã�� ����
					for (auto it = pRoom->sessions.begin(); it != pRoom->sessions.end(); ++it)
					{
						if (*it == pSession)
						{
							pRoom->sessions.erase(it);
							break;
						}
					}
					LeaveCriticalSection(&g_cs);
				}

				// ���� �˸� �޽����� ������ ���� �г����� �ִ��� Ȯ��
				if (strlen(pSession->nickname) > 0)
				{
					SystemMessageBroadcastPacket leavePacket;
					leavePacket.header.packetType = PacketType::SYSTEM_MESSAGE_BROADCAST;
					sprintf_s(leavePacket.message, "[SYSTEM] '%s'���� �����߽��ϴ�.", pSession->nickname);
					leavePacket.header.packetSize = sizeof(PacketHeader) + strlen(leavePacket.message) + 1;

					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						// ���� �����ϴ� ������ ������ ��� ���ǿ��� ������.
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							send(pOtherSession->socket, (const char*)&leavePacket, leavePacket.header.packetSize, 0);
						}
					}
					LeaveCriticalSection(&g_cs);
				}
				// 1. ���� ��Ͽ��� �ش� ������ ����
				EnterCriticalSection(&g_cs);
				for (auto it = SessionList.begin(); it != SessionList.end(); ++it)
				{
					if (*it == pSession)
					{
						SessionList.erase(it);
						break;
					}
				}
				LeaveCriticalSection(&g_cs);

				// 2. ������ �ݰ� ���� ��ü �޸� ����
				closesocket(pSession->socket);
				delete pSession;

			}
			continue; // ���� �۾��� ��ٸ�
		}


		

		if (pOverlappedEx->ioType == EIoOperation::IO_RECV)
		{
			// 1. ���� �����͸� ������ ���� ���� �ڿ� �߰��Ѵ�.
			memcpy(&pSession->recvBuffer[pSession->recvBytes], pOverlappedEx->buffer, bytesTransferred);
			pSession->recvBytes += bytesTransferred;

			int processedBytes = 0; // �̹��� ó���� �� ����Ʈ ��

			// 2. ���ۿ� ������ ��Ŷ�� �ִ��� �ݺ��ؼ� Ȯ��
			while (pSession->recvBytes > 0)
			{
				// 2-1. �ּ��� ����� ���� �� �ִ��� Ȯ��
				if (pSession->recvBytes < sizeof(PacketHeader))
					break;

				// ���� ó���� ��ġ���� ����� �о�´�.
				PacketHeader* header = (PacketHeader*)&pSession->recvBuffer[processedBytes];

				// 2-2. ������ ��Ŷ �ϳ��� �����ߴ��� Ȯ��
				if (pSession->recvBytes < header->packetSize)
					break;


				// 3. ��Ŷ ������ ���� ó�� �б�
				if (header->packetType == PacketType::LOGIN_REQUEST)
				{
					LoginRequestPacket* loginPacket = (LoginRequestPacket*)&pSession->recvBuffer[processedBytes];
					loginPacket->nickname[NICKNAME_LENGTH - 1] = '\0'; // �� ���� ����

					bool isNicknameDuplicate = false;

					// --- �г��� �ߺ� �˻� ---
					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						// �ڱ� �ڽ��� �ƴϰ�, �̹� �α��� �� ���� �߿���
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							if (strcmp(pOtherSession->nickname, loginPacket->nickname) == 0)
							{
								isNicknameDuplicate = true;
								break;
							}
						}
					}
					LeaveCriticalSection(&g_cs);

					// --- ����� ���� ���� ��Ŷ ���� ---
					LoginResponsePacket responsePacket;
					responsePacket.header.packetType = PacketType::LOGIN_RESPONSE;
					responsePacket.header.packetSize = sizeof(LoginResponsePacket);
					responsePacket.success = !isNicknameDuplicate;

					send(pSession->socket, (const char*)&responsePacket, responsePacket.header.packetSize, 0);

					// --- �������� ��쿡�� �ļ� ó�� ---
					if (responsePacket.success)
					{
						strcpy_s(pSession->nickname, loginPacket->nickname);
						std::cout << pSession->socket << " Logged in as: " << pSession->nickname << std::endl;

						// ���� �˸� ��ε�ĳ���� (���� �ڵ�)
						SystemMessageBroadcastPacket enterPacket;
						enterPacket.header.packetType = PacketType::SYSTEM_MESSAGE_BROADCAST;
						sprintf_s(enterPacket.message, "[SYSTEM] '%s'���� �����߽��ϴ�.", pSession->nickname);
						enterPacket.header.packetSize = sizeof(SystemMessageBroadcastPacket);

						EnterCriticalSection(&g_cs);
						for (Session* pOtherSession : SessionList)
						{
							if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
							{
								send(pOtherSession->socket, (const char*)&enterPacket, enterPacket.header.packetSize, 0);
							}
						}
						LeaveCriticalSection(&g_cs);
					}
					else
					{
						std::cout << pSession->socket << " Failed to login. Duplicate nickname: " << loginPacket->nickname << std::endl;
						// ���� �ÿ��� �ƹ��͵� �� �ϸ�, Ŭ���̾�Ʈ�� ������ ���� �����ϰ� �ȴ�.
					}
				}
				else if (header->packetType == PacketType::CHAT_MESSAGE_REQUEST)
				{
					ChatMessageRequestPacket* requestPacket = (ChatMessageRequestPacket*)&pSession->recvBuffer[processedBytes];
					std::cout << pSession->nickname << " Chat: " << requestPacket->message << std::endl;

					// 1. �ٸ� Ŭ���̾�Ʈ���� ��ε�ĳ������ ��Ŷ�� �����.
					ChatMessageBroadcastPacket broadcastPacket;
					broadcastPacket.header.packetType = PacketType::CHAT_MESSAGE_BROADCAST;

					// 2. [�ٽ�] ��Ŷ ũ�⸦ ����ü ��ü ũ��� �����Ѵ�.
					broadcastPacket.header.packetSize = sizeof(ChatMessageBroadcastPacket);

					// 3. �г��Ӱ� �޽����� �����Ѵ�.
					strcpy_s(broadcastPacket.nickname, pSession->nickname);
					strcpy_s(broadcastPacket.message, requestPacket->message);

					// 4. ��ε�ĳ���� ���� (������ ����)
					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							// ���� ���� ������ ũ��(sizeof)�� ������.
							send(pOtherSession->socket, (const char*)&broadcastPacket, broadcastPacket.header.packetSize, 0);
						}
					}
					LeaveCriticalSection(&g_cs);
				}
				// �� ��� ��û ó��
				else if (header->packetType == PacketType::ROOM_LIST_REQUEST)
				{
					RoomListResponsePacket responsePacket;
					responsePacket.header.packetType = PacketType::ROOM_LIST_RESPONSE;

					EnterCriticalSection(&g_cs);
					responsePacket.roomCount = (short)RoomList.size();
					for (int i = 0; i < responsePacket.roomCount; ++i)
					{
						responsePacket.rooms[i].roomId = RoomList[i]->roomId;
						strcpy_s(responsePacket.rooms[i].title, RoomList[i]->title);
						responsePacket.rooms[i].userCount = (short)RoomList[i]->sessions.size();
					}
					LeaveCriticalSection(&g_cs);

					responsePacket.header.packetSize = sizeof(PacketHeader) + sizeof(short) + (responsePacket.roomCount * sizeof(RoomInfo));
					send(pSession->socket, (const char*)&responsePacket, responsePacket.header.packetSize, 0);
				}

				// �� ���� ��û ó��
				else if (header->packetType == PacketType::CREATE_ROOM_REQUEST)
				{
					CreateRoomRequestPacket* requestPacket = (CreateRoomRequestPacket*)&pSession->recvBuffer[processedBytes];
					requestPacket->title[ROOM_TITLE_LENGTH - 1] = '\0'; // �� ���� ����

					// 1. ���ο� Room ��ü ���� �� �ʱ�ȭ
					Room* newRoom = new Room();
					EnterCriticalSection(&g_cs);
					newRoom->roomId = g_nextRoomId++;
					strcpy_s(newRoom->title, requestPacket->title);
					RoomList.push_back(newRoom);
					LeaveCriticalSection(&g_cs);

					// 2. ���� ������ ������ �ش� �濡 �ٷ� �����Ų��.
					// (�� �κ��� ���߿� '�� ����' ��� ���� �� �ߺ��ǹǷ� �Լ��� ����� ����)
					newRoom->sessions.push_back(pSession);
					pSession->pRoom = newRoom;

					// 3. Ŭ���̾�Ʈ���� ���� ���� ����
					CreateRoomResponsePacket responsePacket;
					responsePacket.header.packetType = PacketType::CREATE_ROOM_RESPONSE;
					responsePacket.header.packetSize = sizeof(CreateRoomResponsePacket);
					responsePacket.success = true;
					responsePacket.createdRoomInfo.roomId = newRoom->roomId;
					strcpy_s(responsePacket.createdRoomInfo.title, newRoom->title);
					responsePacket.createdRoomInfo.userCount = 1;

					send(pSession->socket, (const char*)&responsePacket, responsePacket.header.packetSize, 0);

					std::cout << pSession->nickname << " created room '" << newRoom->title << "'" << std::endl;
				}
				
				// 4. ó���� ��ŭ ��ġ�� ���� ������ ���� ����
				processedBytes += header->packetSize;
				pSession->recvBytes -= header->packetSize;
			}

			// 5. ó���� �����͸� ���ۿ��� ���� (���� �����͸� ���� ������ ��ܿ���)
			if (processedBytes > 0)
			{
				// ���� ó������ ���� �����Ͱ� �ִٸ�, ������ ���� ��ġ�� �����Ѵ�.
				if (pSession->recvBytes > 0)
				{
					memmove(pSession->recvBuffer, &pSession->recvBuffer[processedBytes], pSession->recvBytes);
				}
			}

			// 6. (�ſ� �߿�!) ���� ������ ���� WSARecv�� �ٽ� ȣ��
			DWORD recvNumBytes = 0;
			DWORD flags = 0;
			if (WSARecv(pSession->socket, &pOverlappedEx->wsaBuf, 1, &recvNumBytes, &flags, &pOverlappedEx->overlapped, NULL) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					std::cout << "WSARecv failed in worker" << std::endl;
				}
			}
		}
	}
	return 0;
}

int main(void)
{
	InitializeCriticalSection(&g_cs);

	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup err";
		return -1;
	}
	// 1. IOCP ��ü(Completion Port) ����
	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIocp == NULL)
	{
		std::cout << "CreateIoCompletionPort failed" << std::endl;
		return -1;
	}

	// 2. CPU �ھ� ������ŭ Worker ������ ����
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	int threadCount = sysInfo.dwNumberOfProcessors;

	for (int i = 0; i < threadCount; ++i)
	{
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, hIocp, 0, NULL);
		if (hThread != NULL)
		{
			CloseHandle(hThread);
		}
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << "listenSocket err";
		return -1;
	}

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htons(INADDR_ANY);
	serverAddr.sin_port = htons(8888);

	if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cout << "bind err";
		return -1;
	}

	if (listen(listenSocket, 10) == SOCKET_ERROR)
	{
		std::cout << "listen err";
		return -1;
	}

	std::cout << "Server Start!" << std::endl;

	// ���� ���� ������� �ƹ��͵� ���� �ʰ� ���α׷��� ������ �ʵ��� ���
	// ���� �� ������ ���ο� Ŭ���̾�Ʈ�� ������ �޾� IOCP�� ����ϴ� ������ �մϴ�.
	while (true)
	{
		SOCKADDR_IN clientAddr;
		int addrlen = sizeof(clientAddr);
		SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrlen);
		if (clientSocket == INVALID_SOCKET)
		{
			std::cout << "accept failed" << std::endl;
			continue;
		}

		// 1. ���ο� Ŭ���̾�Ʈ�� ���� ���� ��ü ����
		Session* pSession = new Session();
		pSession->socket = clientSocket;

		// 2. ���� ������ Ŭ���̾�Ʈ ������ IOCP ��ü�� ����
		//    �� ��° ����(pSession)�� �ٷ� CompletionKey�Դϴ�.
		//    ���߿� GQCS���� �� ���� �����͸� �״�� �����ް� �˴ϴ�.
		if (CreateIoCompletionPort((HANDLE)pSession->socket, hIocp, (ULONG_PTR)pSession, 0) == NULL)
		{
			std::cout << "CreateIoCompletionPort failed for client socket" << std::endl;
			delete pSession;
			continue;
		}

		EnterCriticalSection(&g_cs);
		SessionList.push_back(pSession);
		LeaveCriticalSection(&g_cs);

		// 3. �� Ŭ���̾�Ʈ�� ���� ù ��° �񵿱� ����(WSARecv)�� '��û'�մϴ�.
		//    OverlappedEx ����ü�� �����͸� �Ѱ��ִ� ���� �ٽ��Դϴ�.
		OverlappedEx* pOverlappedEx = &pSession->recvOverlapped;
		DWORD recvBytes = 0;
		DWORD flags = 0;

		// WSABUF ����
		pOverlappedEx->wsaBuf.buf = pOverlappedEx->buffer;
		pOverlappedEx->wsaBuf.len = sizeof(pOverlappedEx->buffer);

		if (WSARecv(pSession->socket, &pOverlappedEx->wsaBuf, 1, &recvBytes, &flags, &pOverlappedEx->overlapped, NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				std::cout << "WSARecv failed" << std::endl;
				// ���� ó�� (���� ���� ��)
			}
		}
	}

	DeleteCriticalSection(&g_cs);
	closesocket(listenSocket);
	WSACleanup();
}