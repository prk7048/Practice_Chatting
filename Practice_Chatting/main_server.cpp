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
	char buffer[1024];
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



// Worker �����尡 ������ �Լ�
unsigned int __stdcall WorkerThread(void* pArguments)
{
	HANDLE hIocp = (HANDLE)pArguments;

	while (true)
	{
		OverlappedEx* pOverlappedEx = nullptr;
		Session* pSession = nullptr;
		DWORD bytesTransferred = 0;

		// 1. �Ϸ�� I/O �۾��� ť�� ���� ������ ���� ��� (������� ��ȣǥ �˸��� ��ٸ�)
		BOOL ret = GetQueuedCompletionStatus(hIocp, &bytesTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlappedEx, INFINITE);

		// ret�� FALSE�ų� bytesTransferred�� 0�̸� Ŭ���̾�Ʈ ������ ���� ��
		if (ret == FALSE || bytesTransferred == 0)
		{
			std::cout << pSession->socket << " socket disconnected" << std::endl;

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

			// 2. �Ϸ�� �۾��� ������ Ȯ���ϰ� ó��
			if (pOverlappedEx->ioType == EIoOperation::IO_RECV)
			{
				std::cout << pSession->socket << " socket Received data: " << bytesTransferred << " bytes" << std::endl;

				// --- ���⿡ ��ε�ĳ���� ������ �߰��մϴ� ---
				EnterCriticalSection(&g_cs);

				for (Session* pOtherSession : SessionList)
				{
					// �޽����� ���� �ڽ��� ������ �ٸ� ��� Ŭ���̾�Ʈ����
					if (pOtherSession != pSession)
					{
						// �߿�: ���� send�� �񵿱�� ó���ؾ� �մϴ�.
						// ������ ������ ���� ���ظ� ���� ���� send�� ��� ����ϰڽ��ϴ�.
						send(pOtherSession->socket, pOverlappedEx->buffer, bytesTransferred, 0);
					}
				}

				LeaveCriticalSection(&g_cs);

				// 2. ������ �ݰ� ���� ��ü �޸� ����
				closesocket(pSession->socket);
				delete pSession;

				continue; // ���� �۾��� ��ٸ�
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