#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <vector>
#include <thread>
#include <process.h>

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
};

// Ŭ���̾�Ʈ �ϳ��� ������ ��� ���� ����ü
struct Session
{
	SOCKET socket;
	OverlappedEx recvOverlapped; // �� ������ �ڽŸ��� '����'�� OverlappedEx�� �����ϴ�.

	// �����ڿ��� ��� �������� �ʱ�ȭ�մϴ�.
	Session() : socket(INVALID_SOCKET)
	{
		ZeroMemory(&recvOverlapped.overlapped, sizeof(recvOverlapped.overlapped));
		recvOverlapped.ioType = EIoOperation::IO_RECV;
		recvOverlapped.session = this; // �ڱ� �ڽ��� ����Ű���� ����
	}
};

// Session List Vector
std::vector<Session*> SessionList;

// Lock
CRITICAL_SECTION g_cs;

// �����尡 ������ �Լ��� ����
// ��ȯ Ÿ��: unsigned int, ȣ�� �Ծ�: __stdcall, ����: void ������ �ϳ�
unsigned int __stdcall ThreadMain(void* pArguments)
{
	std::cout << "���Ӽ���";
	// 1. main ������κ��� �Ѱܹ��� ����(Ŭ���̾�Ʈ ����)�� ���� Ÿ������ �ǵ�����.
	Session* clientSession = (Session*)pArguments;

	// 2. �� ������� ���� �� clientSocket�ϰ� ����ϸ� �ڱ� �� ���� �Ѵ�.
	// (��: recv, send �ݺ�...)
	while (true)
	{
		int recvsize = recv(clientSession->socket, clientSession->buf, sizeof(clientSession->buf), NULL);

		// Ŭ�� ���� ����
		if (recvsize <= 0)
		{
			std::cout << "Ŭ�� ���� ����" << std::endl;
			break;
		}
		// --- Lock Start (�ٸ� Ŭ���̾�Ʈ�鿡�� �޽����� �Ѹ��� ���� ����� �о�� �ϹǷ�) ---
		EnterCriticalSection(&g_cs);

		// SessionList�� ��ȸ�ϸ鼭
		for (Session* otherSession : SessionList)
		{
			// �޽����� ���� �ڽ��� ������ �ٸ� ��� Ŭ���̾�Ʈ����
			if (otherSession != clientSession)
			{
				// ���� �޽����� �״�� �����Ѵ�.
				send(otherSession->socket, clientSession->buf, recvsize, NULL);
			}
		}

		// --- Lock End ---
		LeaveCriticalSection(&g_cs);
	}


	// 3. �����尡 ����Ǳ� ���� �ڿ��� �����Ѵ�.
	closesocket(clientSession->socket);
	// lock
	EnterCriticalSection(&g_cs);
	// SessionList���� ���� �����尡 ����ϴ� ������ ã�� ����
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

	delete clientSession; // ���� �Ҵ�� �޸� ����
	// 4. �����带 �����Ѵ�.
	return 0;
}

// Worker �����尡 ������ �Լ�
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
	while (true)
	{
		Sleep(1000); // 1�ʿ� �� ���� ����� �۶�����
	}

	DeleteCriticalSection(&g_cs);
	closesocket(listenSocket);
	WSACleanup();
}