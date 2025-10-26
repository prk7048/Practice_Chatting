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

// --- Room 구조체와 RoomList를 새로 추가합니다 ---
struct Room; // Session 구조체에서 Room을 참조해야 하므로 전방 선언


// 클라이언트 하나의 정보를 담는 세션 구조체
struct Session
{
	SOCKET socket;
	OverlappedEx recvOverlapped;

	char recvBuffer[4096]; // 각 세션별 수신 버퍼
	int recvBytes;         // 현재까지 수신한 바이트 수
	char nickname[NICKNAME_LENGTH];
	Room* pRoom; // --- 자신이 속한 방을 가리키는 포인터 추가 ---

	// 생성자에서 멤버 변수들을 초기화합니다.
	Session() : socket(INVALID_SOCKET), recvBytes(0), pRoom(nullptr) // pRoom 초기화 추가
	{
		ZeroMemory(&recvOverlapped.overlapped, sizeof(recvOverlapped.overlapped));
		recvOverlapped.ioType = EIoOperation::IO_RECV;
		recvOverlapped.session = this;
		ZeroMemory(recvBuffer, sizeof(recvBuffer)); // 버퍼 초기화
		ZeroMemory(nickname, sizeof(nickname)); // 닉네임 초기화 추가
	}
};

// 채팅방 하나의 정보를 담는 구조체
struct Room
{
	int roomId;
	char title[ROOM_TITLE_LENGTH];
	std::vector<Session*> sessions; // 이 방에 속한 세션들의 목록

	Room() : roomId(-1)
	{
		ZeroMemory(title, sizeof(title));
	}
};

// Session List Vector
std::vector<Session*> SessionList; 
std::vector<Room*> RoomList; // --- RoomList 전역 변수 추가 ---
int g_nextRoomId = 1; // 다음에 생성될 방에 부여할 고유 ID

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

				// 세션이 방에 들어가 있었다면, 방에서 퇴장 처리
				if (pSession->pRoom != nullptr)
				{
					Room* pRoom = pSession->pRoom;
					EnterCriticalSection(&g_cs);
					// 방의 세션 목록에서 해당 세션을 찾아 제거
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


				// 3. 패킷 종류에 따라 처리 분기
				if (header->packetType == PacketType::LOGIN_REQUEST)
				{
					LoginRequestPacket* loginPacket = (LoginRequestPacket*)&pSession->recvBuffer[processedBytes];
					loginPacket->nickname[NICKNAME_LENGTH - 1] = '\0'; // 널 문자 보장

					bool isNicknameDuplicate = false;

					// --- 닉네임 중복 검사 ---
					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						// 자기 자신은 아니고, 이미 로그인 된 세션 중에서
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

					// --- 결과에 따른 응답 패킷 전송 ---
					LoginResponsePacket responsePacket;
					responsePacket.header.packetType = PacketType::LOGIN_RESPONSE;
					responsePacket.header.packetSize = sizeof(LoginResponsePacket);
					responsePacket.success = !isNicknameDuplicate;

					send(pSession->socket, (const char*)&responsePacket, responsePacket.header.packetSize, 0);

					// --- 성공했을 경우에만 후속 처리 ---
					if (responsePacket.success)
					{
						strcpy_s(pSession->nickname, loginPacket->nickname);
						std::cout << pSession->socket << " Logged in as: " << pSession->nickname << std::endl;

						// 입장 알림 브로드캐스팅 (기존 코드)
						SystemMessageBroadcastPacket enterPacket;
						enterPacket.header.packetType = PacketType::SYSTEM_MESSAGE_BROADCAST;
						sprintf_s(enterPacket.message, "[SYSTEM] '%s'님이 입장했습니다.", pSession->nickname);
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
						// 실패 시에는 아무것도 안 하면, 클라이언트가 접속을 끊고 종료하게 된다.
					}
				}
				else if (header->packetType == PacketType::CHAT_MESSAGE_REQUEST)
				{
					ChatMessageRequestPacket* requestPacket = (ChatMessageRequestPacket*)&pSession->recvBuffer[processedBytes];
					std::cout << pSession->nickname << " Chat: " << requestPacket->message << std::endl;

					// 1. 다른 클라이언트에게 브로드캐스팅할 패킷을 만든다.
					ChatMessageBroadcastPacket broadcastPacket;
					broadcastPacket.header.packetType = PacketType::CHAT_MESSAGE_BROADCAST;

					// 2. [핵심] 패킷 크기를 구조체 전체 크기로 고정한다.
					broadcastPacket.header.packetSize = sizeof(ChatMessageBroadcastPacket);

					// 3. 닉네임과 메시지를 복사한다.
					strcpy_s(broadcastPacket.nickname, pSession->nickname);
					strcpy_s(broadcastPacket.message, requestPacket->message);

					// 4. 브로드캐스팅 로직 (기존과 동일)
					EnterCriticalSection(&g_cs);
					for (Session* pOtherSession : SessionList)
					{
						if (pOtherSession != pSession && strlen(pOtherSession->nickname) > 0)
						{
							// 보낼 때도 고정된 크기(sizeof)로 보낸다.
							send(pOtherSession->socket, (const char*)&broadcastPacket, broadcastPacket.header.packetSize, 0);
						}
					}
					LeaveCriticalSection(&g_cs);
				}
				// 방 목록 요청 처리
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

				// 방 생성 요청 처리
				else if (header->packetType == PacketType::CREATE_ROOM_REQUEST)
				{
					CreateRoomRequestPacket* requestPacket = (CreateRoomRequestPacket*)&pSession->recvBuffer[processedBytes];
					requestPacket->title[ROOM_TITLE_LENGTH - 1] = '\0'; // 널 문자 보장

					// 1. 새로운 Room 객체 생성 및 초기화
					Room* newRoom = new Room();
					EnterCriticalSection(&g_cs);
					newRoom->roomId = g_nextRoomId++;
					strcpy_s(newRoom->title, requestPacket->title);
					RoomList.push_back(newRoom);
					LeaveCriticalSection(&g_cs);

					// 2. 방을 생성한 유저를 해당 방에 바로 입장시킨다.
					// (이 부분은 나중에 '방 퇴장' 기능 구현 시 중복되므로 함수로 만들면 좋다)
					newRoom->sessions.push_back(pSession);
					pSession->pRoom = newRoom;

					// 3. 클라이언트에게 성공 응답 전송
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