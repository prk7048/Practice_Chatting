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

// 스레드가 실행할 함수의 원형
// 반환 타입: unsigned int, 호출 규약: __stdcall, 인자: void 포인터 하나
unsigned int __stdcall ThreadMain(void* pArguments)
{
	std::cout << "접속성공";
	// 1. main 스레드로부터 넘겨받은 인자(클라이언트 소켓)를 원래 타입으로 되돌린다.
	Session* clientSession = (Session*)pArguments;

	// 2. 이 스레드는 이제 이 clientSocket하고만 통신하며 자기 할 일을 한다.
	// (예: recv, send 반복...)
	while (true)
	{
		int recvsize = recv(clientSession->socket, clientSession->buf, sizeof(clientSession->buf), NULL);

		// 클라 접속 종료
		if (recvsize <= 0)
		{
			std::cout << "클라 접속 종료" << std::endl;
			break;
		}
		// --- Lock Start (다른 클라이언트들에게 메시지를 뿌리기 위해 목록을 읽어야 하므로) ---
		EnterCriticalSection(&g_cs);

		// SessionList를 순회하면서
		for (Session* otherSession : SessionList)
		{
			// 메시지를 보낸 자신을 제외한 다른 모든 클라이언트에게
			if (otherSession != clientSession)
			{
				// 받은 메시지를 그대로 전달한다.
				send(otherSession->socket, clientSession->buf, recvsize, NULL);
			}
		}

		// --- Lock End ---
		LeaveCriticalSection(&g_cs);
	}


	// 3. 스레드가 종료되기 전에 자원을 정리한다.
	closesocket(clientSession->socket);
	// lock
	EnterCriticalSection(&g_cs);
	// SessionList에서 현재 스레드가 담당하던 세션을 찾아 제거
	for (auto it = SessionList.begin(); it != SessionList.end(); ++it)
	{
		if (*it == clientSession)
		{
			SessionList.erase(it);
			break;
		}
	}
	// unlock
	LeaveCriticalSection(&g_cs);

	delete clientSession; // 동적 할당된 메모리 해제
	// 4. 스레드를 종료한다.
	return 0;
}

// Worker 스레드가 실행할 함수
unsigned int __stdcall WorkerThread(void* pArguments)
{
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
	while (true)
	{
		Sleep(1000); // 1초에 한 번씩 깨어나서 멍때리기
	}

	DeleteCriticalSection(&g_cs);
	closesocket(listenSocket);
	WSACleanup();
}