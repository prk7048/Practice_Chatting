#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <vector>
#include <thread>
#include <process.h>

#pragma comment(lib,"ws2_32.lib")

// ����

struct Session
{
	SOCKET socket;
	char buf[1024];
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

		int sendsize = send(clientSession->socket, clientSession->buf, recvsize, NULL);
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

int main(void)
{
	InitializeCriticalSection(&g_cs);

	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup err";
		return -1;
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

	while (1)
	{
		SOCKADDR_IN clientAddr;
		int addrlen = sizeof(clientAddr);
		memset(&clientAddr, 0, sizeof(clientAddr));
		SOCKET ClientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrlen);
		if (ClientSocket == INVALID_SOCKET)
		{
			std::cout << "Accept Error: " << WSAGetLastError() << std::endl;
			closesocket(listenSocket);
			WSACleanup();
			return -1;
		}

		Session* newSession = new Session;
		newSession->socket = ClientSocket;

		// Lock
		EnterCriticalSection(&g_cs);
		SessionList.push_back(newSession);
		// unlock 
		LeaveCriticalSection(&g_cs);

		// Session�� thread �ϳ��� �Ҵ�
		HANDLE hThread = (HANDLE)_beginthreadex(
			NULL,             // ���� �Ӽ� (���� NULL)
			0,                // ���� ũ�� (0 = �⺻��)
			ThreadMain,       // �����尡 ������ �Լ� �ּ�
			(void*)newSession, // ������ �Լ��� ������ ����
			0,                // ���� �÷��� (0 = ��� ����)
			NULL              // ������ ID�� ���� ���� �ּ� (���� NULL)
		);

		//while�� ������ ������ ���� �ؾ��ұ�?
	}

	DeleteCriticalSection(&g_cs);
	closesocket(listenSocket);
	WSACleanup();
}