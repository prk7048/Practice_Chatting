#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <vector>
#include <thread>
#include <process.h>

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

// 클라이언트 하나의 정보를 담는 세션 구조체
struct Session
{
	SOCKET socket;
	OverlappedEx recvOverlapped; // 각 세션은 자신만의 '수신'용 OverlappedEx를 가집니다.

	// 생성자에서 멤버 변수들을 초기화합니다.
	Session() : socket(INVALID_SOCKET)
	{
		ZeroMemory(&recvOverlapped.overlapped, sizeof(recvOverlapped.overlapped));
		recvOverlapped.ioType = EIoOperation::IO_RECV;
		recvOverlapped.session = this; // 자기 자신을 가리키도록 설정
	}
};

// Session List Vector
std::vector<Session*> SessionList;

// Lock
CRITICAL_SECTION g_cs;



// Worker 스레드가 실행할 함수
unsigned int __stdcall WorkerThread(void* pArguments)
{
	HANDLE hIocp = (HANDLE)pArguments;

	while (true)
	{
		OverlappedEx* pOverlappedEx = nullptr;
		Session* pSession = nullptr;
		DWORD bytesTransferred = 0;

		// 1. 완료된 I/O 작업이 큐에 생길 때까지 무한 대기 (은행원이 번호표 알림을 기다림)
		BOOL ret = GetQueuedCompletionStatus(hIocp, &bytesTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlappedEx, INFINITE);

		// ret이 FALSE거나 bytesTransferred가 0이면 클라이언트 접속이 끊긴 것
		if (ret == FALSE || bytesTransferred == 0)
		{
			std::cout << pSession->socket << " socket disconnected" << std::endl;

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

			// 2. 완료된 작업의 종류를 확인하고 처리
			if (pOverlappedEx->ioType == EIoOperation::IO_RECV)
			{
				std::cout << pSession->socket << " socket Received data: " << bytesTransferred << " bytes" << std::endl;

				// --- 여기에 브로드캐스팅 로직을 추가합니다 ---
				EnterCriticalSection(&g_cs);

				for (Session* pOtherSession : SessionList)
				{
					// 메시지를 보낸 자신을 제외한 다른 모든 클라이언트에게
					if (pOtherSession != pSession)
					{
						// 중요: 이제 send도 비동기로 처리해야 합니다.
						// 하지만 지금은 개념 이해를 위해 동기 send를 잠시 사용하겠습니다.
						send(pOtherSession->socket, pOverlappedEx->buffer, bytesTransferred, 0);
					}
				}

				LeaveCriticalSection(&g_cs);

				// 2. 소켓을 닫고 세션 객체 메모리 해제
				closesocket(pSession->socket);
				delete pSession;

				continue; // 다음 작업을 기다림
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