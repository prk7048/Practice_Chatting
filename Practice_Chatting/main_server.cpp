#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <vector>
#include <thread>
#include <process.h>

#include "protocol.h"
#pragma comment(lib,"ws2_32.lib")

// 서버

// 어떤 I/O 작업인지 구분하기 위한 열거형
enum class EIoOperation
{
	IO_ACCEPT, // IOCP에서는 accept 또한 비동기로 처리할 수 있습니다. (고급 주제)
	IO_RECV,
	IO_SEND
};

// I/O 작업에 필요한 데이터를 담는 확장 Overlapped 구조체
struct OverlappedEx
{
	OVERLAPPED overlapped;      // 모든 비동기 I/O의 기본이 되는 구조체
	EIoOperation ioType;        // 이 Overlapped 객체가 어떤 종류의 작업에 사용되었는지 구분
	void* session;              // 이 I/O 작업이 어떤 세션에 속해있는지를 가리키는 포인터

	// WSABUF는 버퍼의 주소와 길이를 담는 구조체입니다.
	// WSARecv, WSASend는 이 정보를 보고 데이터를 읽거나 씁니다.
	WSABUF wsaBuf;
	char buffer[1024];
};

//// 클라이언트 하나의 정보를 담는 세션 구조체
//struct Session
//{
//	SOCKET socket;
//	OverlappedEx recvOverlapped; // 각 세션은 자신만의 '수신'용 OverlappedEx를 가집니다.
//
//	// 생성자에서 멤버 변수들을 초기화합니다.
//	Session() : socket(INVALID_SOCKET)
//	{
//		ZeroMemory(&recvOverlapped.overlapped, sizeof(recvOverlapped.overlapped));
//		recvOverlapped.ioType = EIoOperation::IO_RECV;
//		recvOverlapped.session = this; // 자기 자신을 가리키도록 설정
//	}
//};

// 클라이언트 하나의 정보를 담는 세션 구조체
struct Session
{
	SOCKET socket;
	OverlappedEx recvOverlapped;

	char recvBuffer[4096]; // 각 세션별 수신 버퍼
	int recvBytes;         // 현재까지 수신한 바이트 수
	char nickname[NICKNAME_LENGTH];

	// 생성자에서 멤버 변수들을 초기화합니다.
	Session() : socket(INVALID_SOCKET), recvBytes(0) // recvBytes 초기화 추가
	{
		ZeroMemory(&recvOverlapped.overlapped, sizeof(recvOverlapped.overlapped));
		recvOverlapped.ioType = EIoOperation::IO_RECV;
		recvOverlapped.session = this;
		ZeroMemory(recvBuffer, sizeof(recvBuffer)); // 버퍼 초기화
		ZeroMemory(nickname, sizeof(nickname)); // 닉네임 초기화 추가
	}
};

// Session List Vector
std::vector<Session*> SessionList;

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

		// --- 실패 경로 (접속 종료 또는 에러) ---
		if (ret == FALSE || bytesTransferred == 0)
		{
			if (pSession != nullptr) // pSession이 NULL이 아닐 때만 처리
			{
				std::cout << pSession->socket << " socket disconnected" << std::endl;

				// 퇴장 알림 메시지를 보내기 전에 닉네임이 있는지 확인
				if (strlen(pSession->nickname) > 0)
				{
					SystemMessageBroadcastPacket leavePacket;
					leavePacket.header.packetType = PacketType::SYSTEM_MESSAGE_BROADCAST;
					sprintf_s(leavePacket.message, "[SYSTEM] '%s'님이 퇴장했습니다.", pSession->nickname);
					leavePacket.header.packetSize = sizeof(PacketHeader) + strlen(leavePacket.message) + 1;

					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						// 접속 종료하는 세션을 제외한 모든 세션에게 보낸다.
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							send(pOtherSession->socket, (const char*)&leavePacket, leavePacket.header.packetSize, 0);
						}
					}
					LeaveCriticalSection(&g_cs);
				}
				// 1. 세션 목록에서 해당 세션을 제거
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

				// 2. 소켓을 닫고 세션 객체 메모리 해제
				closesocket(pSession->socket);
				delete pSession;

			}
			continue; // 다음 작업을 기다림
		}


		//// WorkerThread 함수 안의 if (pOverlappedEx->ioType == EIoOperation::IO_RECV) 블록

		//// --- 성공 경로 (I/O 작업 완료) ---
		//if (pOverlappedEx->ioType == EIoOperation::IO_RECV)
		//{
		//	// 1. 받은 데이터를 PacketHeader로 캐스팅하여 어떤 종류의 패킷인지 확인한다.
		//	PacketHeader* header = (PacketHeader*)pOverlappedEx->buffer;

		//	// 2. 패킷 종류에 따라 처리를 분기한다.
		//	if (header->packetType == PacketType::CHAT_MESSAGE)
		//	{
		//		// 받은 패킷이 ChatMessagePacket이라고 확신하고 캐스팅한다.
		//		ChatMessagePacket* chatPacket = (ChatMessagePacket*)pOverlappedEx->buffer;

		//		std::cout << pSession->socket << " socket Received Chat: " << chatPacket->message << std::endl;

		//		// 3. 브로드캐스팅 로직: 받은 패킷 그대로를 다른 클라이언트에게 전달한다.
		//		EnterCriticalSection(&g_cs);
		//		for (Session* pOtherSession : SessionList)
		//		{
		//			if (pOtherSession != pSession)
		//			{
		//				// 받은 패킷의 크기(header->packetSize)만큼만 정확히 보낸다.
		//				send(pOtherSession->socket, (const char*)chatPacket, header->packetSize, 0);
		//			}
		//		}
		//		LeaveCriticalSection(&g_cs);
		//	}
		//	// else if (header->packetType == PacketType::LOGIN_REQUEST) { ... }
		//	// 나중에 다른 종류의 패킷 처리 로직을 여기에 추가할 수 있다.


		//	// 4. (매우 중요!) 다음 수신을 위해 WSARecv를 다시 호출
		//	DWORD recvBytes = 0;
		//	DWORD flags = 0;
		//	if (WSARecv(pSession->socket, &pOverlappedEx->wsaBuf, 1, &recvBytes, &flags, &pOverlappedEx->overlapped, NULL) == SOCKET_ERROR)
		//	{
		//		if (WSAGetLastError() != WSA_IO_PENDING)
		//		{
		//			std::cout << "WSARecv failed in worker" << std::endl;
		//			// 실제로는 여기서도 접속 종료 처리를 해줘야 합니다.
		//		}
		//	}
		//}


		if (pOverlappedEx->ioType == EIoOperation::IO_RECV)
		{
			// 1. 받은 데이터를 세션의 수신 버퍼 뒤에 추가한다.
			memcpy(&pSession->recvBuffer[pSession->recvBytes], pOverlappedEx->buffer, bytesTransferred);
			pSession->recvBytes += bytesTransferred;

			int processedBytes = 0; // 이번에 처리한 총 바이트 수

			// 2. 버퍼에 완전한 패킷이 있는지 반복해서 확인
			while (pSession->recvBytes > 0)
			{
				// 2-1. 최소한 헤더는 읽을 수 있는지 확인
				if (pSession->recvBytes < sizeof(PacketHeader))
					break;

				// 현재 처리할 위치에서 헤더를 읽어온다.
				PacketHeader* header = (PacketHeader*)&pSession->recvBuffer[processedBytes];

				// 2-2. 완전한 패킷 하나가 도착했는지 확인
				if (pSession->recvBytes < header->packetSize)
					break;

				//// 3. 패킷 종류에 따라 처리 분기
				//if (header->packetType == PacketType::CHAT_MESSAGE)
				//{
				//	ChatMessagePacket* chatPacket = (ChatMessagePacket*)&pSession->recvBuffer[processedBytes];
				//	std::cout << pSession->socket << " socket Received Chat: " << chatPacket->message << std::endl;

				//	// 3-1. 브로드캐스팅 로직
				//	EnterCriticalSection(&g_cs);
				//	for (Session* pOtherSession : SessionList)
				//	{
				//		if (pOtherSession != pSession)
				//		{
				//			send(pOtherSession->socket, (const char*)chatPacket, header->packetSize, 0);
				//		}
				//	}
				//	LeaveCriticalSection(&g_cs);

				//}

				// 3. 패킷 종류에 따라 처리 분기
				if (header->packetType == PacketType::LOGIN_REQUEST)
				{
					LoginRequestPacket* loginPacket = (LoginRequestPacket*)&pSession->recvBuffer[processedBytes];

					// 세션에 닉네임 저장
					strcpy_s(pSession->nickname, loginPacket->nickname);
					std::cout << pSession->socket << " Logged in as: " << pSession->nickname << std::endl;

					// 모든 클라이언트에게 입장 알림 메시지 브로드캐스팅
					SystemMessageBroadcastPacket enterPacket;
					enterPacket.header.packetType = PacketType::SYSTEM_MESSAGE_BROADCAST;
					sprintf_s(enterPacket.message, "[SYSTEM] '%s'님이 입장했습니다.", pSession->nickname);
					enterPacket.header.packetSize = sizeof(PacketHeader) + strlen(enterPacket.message) + 1;

					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						// 자기 자신을 제외한 다른 로그인 된 사용자에게만 보낸다.
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							send(pOtherSession->socket, (const char*)&enterPacket, enterPacket.header.packetSize, 0);
						}
					}
					LeaveCriticalSection(&g_cs);
				
				}
				else if (header->packetType == PacketType::CHAT_MESSAGE_REQUEST)
				{
					ChatMessageRequestPacket* requestPacket = (ChatMessageRequestPacket*)&pSession->recvBuffer[processedBytes];
					std::cout << pSession->nickname << " Chat: " << requestPacket->message << std::endl;

					// 3-1. 다른 클라이언트에게 브로드캐스팅할 패킷을 새로 만든다.
					ChatMessageBroadcastPacket broadcastPacket;
					broadcastPacket.header.packetType = PacketType::CHAT_MESSAGE_BROADCAST;
					strcpy_s(broadcastPacket.nickname, pSession->nickname);
					strcpy_s(broadcastPacket.message, requestPacket->message);
					broadcastPacket.header.packetSize = sizeof(PacketHeader) + strlen(broadcastPacket.nickname) + 1 + strlen(broadcastPacket.message) + 1;

					// 3-2. 브로드캐스팅 로직
					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						// 로그인 된 다른 사용자에게만 보낸다. (자기 자신 제외)
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							send(pOtherSession->socket, (const char*)&broadcastPacket, broadcastPacket.header.packetSize, 0);
						}
					}
					LeaveCriticalSection(&g_cs);
				}
				// 4. 처리한 만큼 위치와 남은 데이터 양을 갱신
				processedBytes += header->packetSize;
				pSession->recvBytes -= header->packetSize;
			}

			// 5. 처리한 데이터를 버퍼에서 제거 (남은 데이터를 버퍼 앞으로 당겨오기)
			if (processedBytes > 0)
			{
				// 아직 처리되지 않은 데이터가 있다면, 버퍼의 시작 위치로 복사한다.
				if (pSession->recvBytes > 0)
				{
					memmove(pSession->recvBuffer, &pSession->recvBuffer[processedBytes], pSession->recvBytes);
				}
			}

			// 6. (매우 중요!) 다음 수신을 위해 WSARecv를 다시 호출
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
	// 1. IOCP 객체(Completion Port) 생성
	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIocp == NULL)
	{
		std::cout << "CreateIoCompletionPort failed" << std::endl;
		return -1;
	}

	// 2. CPU 코어 개수만큼 Worker 스레드 생성
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

	// 이제 메인 스레드는 아무것도 하지 않고 프로그램이 끝나지 않도록 대기
	// 이제 이 루프는 새로운 클라이언트의 접속을 받아 IOCP에 등록하는 역할을 합니다.
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

		// 1. 새로운 클라이언트를 위한 세션 객체 생성
		Session* pSession = new Session();
		pSession->socket = clientSocket;

		// 2. 새로 생성된 클라이언트 소켓을 IOCP 객체와 연결
		//    세 번째 인자(pSession)가 바로 CompletionKey입니다.
		//    나중에 GQCS에서 이 세션 포인터를 그대로 돌려받게 됩니다.
		if (CreateIoCompletionPort((HANDLE)pSession->socket, hIocp, (ULONG_PTR)pSession, 0) == NULL)
		{
			std::cout << "CreateIoCompletionPort failed for client socket" << std::endl;
			delete pSession;
			continue;
		}

		EnterCriticalSection(&g_cs);
		SessionList.push_back(pSession);
		LeaveCriticalSection(&g_cs);

		// 3. 이 클라이언트에 대한 첫 번째 비동기 수신(WSARecv)을 '요청'합니다.
		//    OverlappedEx 구조체의 포인터를 넘겨주는 것이 핵심입니다.
		OverlappedEx* pOverlappedEx = &pSession->recvOverlapped;
		DWORD recvBytes = 0;
		DWORD flags = 0;

		// WSABUF 설정
		pOverlappedEx->wsaBuf.buf = pOverlappedEx->buffer;
		pOverlappedEx->wsaBuf.len = sizeof(pOverlappedEx->buffer);

		if (WSARecv(pSession->socket, &pOverlappedEx->wsaBuf, 1, &recvBytes, &flags, &pOverlappedEx->overlapped, NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				std::cout << "WSARecv failed" << std::endl;
				// 에러 처리 (세션 삭제 등)
			}
		}
	}

	DeleteCriticalSection(&g_cs);
	closesocket(listenSocket);
	WSACleanup();
}