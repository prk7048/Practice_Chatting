//#pragma once
//// =============================================================
////                       �������� ����
//// =============================================================
//
//// ��Ŷ�� ������ �����ϱ� ���� ������
//enum class PacketType : short
//{
//    LOGIN_REQUEST = 1,
//    CHAT_MESSAGE = 2,
//};
//
//// ��� ��Ŷ�� �� �տ� ���� ���
//// __pragma(pack(1))�� ����ü ��� ���̿� ������ ����(padding)�� ���� �ʵ��� �Ͽ�
//// ��Ʈ��ũ�� �������� �� �������� ũ�⸦ ���� �����ϰ� ����ϴ�.
//#pragma pack(push, 1)
//struct PacketHeader
//{
//    short packetSize;
//    PacketType packetType;
//};
//
//// ä�� �޽��� ��Ŷ ����ü
//struct ChatMessagePacket
//{
//    PacketHeader header;
//    // C++ string ��� ���� ũ�� �迭�� ����ؾ� ��Ŷ ũ�� ����� �����մϴ�.
//    char message[256];
//};
//#pragma pack(pop)
//
//// =============================================================

#pragma once
// =============================================================
//                       �������� ����
// =============================================================
constexpr int NICKNAME_LENGTH = 32;
constexpr int CHAT_LENGTH = 256;

// ��Ŷ�� ������ �����ϱ� ���� ������
enum class PacketType : short
{
    LOGIN_REQUEST = 1,
    LOGIN_RESPONSE = 2, // --- Ÿ���� �ϳ��� �����ϰ�, ����ü ���� �÷��׷� ����/���� ����

    CHAT_MESSAGE_REQUEST = 10,
    CHAT_MESSAGE_BROADCAST = 11,

    SYSTEM_MESSAGE_BROADCAST = 20,
};

#pragma pack(push, 1)
struct PacketHeader
{
    short packetSize;
    PacketType packetType;
};

// �α��� ��û ��Ŷ (Client -> Server)
struct LoginRequestPacket
{
    PacketHeader header;
    char nickname[NICKNAME_LENGTH];
};

// �α��� ���� ��Ŷ (Server -> Client)
struct LoginResponsePacket
{
    PacketHeader header;
    bool success; // true: ����, false: ����(�ߺ� �г���)
};
// ä�� ��û ��Ŷ (Client -> Server)
struct ChatMessageRequestPacket
{
    PacketHeader header;
    char message[CHAT_LENGTH];
};

// ä�� ��ε�ĳ��Ʈ ��Ŷ (Server -> Client)
struct ChatMessageBroadcastPacket
{
    PacketHeader header;
    char nickname[NICKNAME_LENGTH];
    char message[CHAT_LENGTH];
};

// �ý��� �޽��� ��ε�ĳ��Ʈ ��Ŷ (Server -> Client)
struct SystemMessageBroadcastPacket
{
    PacketHeader header;
    char message[CHAT_LENGTH];
};
#pragma pack(pop)

// =============================================================