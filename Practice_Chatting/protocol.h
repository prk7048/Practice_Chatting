#pragma once
// =============================================================
//                       �������� ����
// =============================================================
constexpr int NICKNAME_LENGTH = 32;
constexpr int CHAT_LENGTH = 256;
constexpr int ROOM_TITLE_LENGTH = 32;
constexpr int MAX_ROOM_COUNT = 50;

// ��Ŷ�� ������ �����ϱ� ���� ������
enum class PacketType : short
{
    // �α��� ����
    LOGIN_REQUEST = 1,
    LOGIN_RESPONSE = 2,

    // ä�� ����
    CHAT_MESSAGE_REQUEST = 10,
    CHAT_MESSAGE_BROADCAST = 11,

    // �ý��� �޽��� ����
    SYSTEM_MESSAGE_BROADCAST = 20,

    // --- ä�ù� ���� ---
    ROOM_LIST_REQUEST = 30,
    ROOM_LIST_RESPONSE = 31,

    CREATE_ROOM_REQUEST = 32,
    CREATE_ROOM_RESPONSE = 33,

    JOIN_ROOM_REQUEST = 34,
    JOIN_ROOM_RESPONSE = 35,
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

// �� ���� ����ü (ROOM_LIST_RESPONSE ���� ���)
struct RoomInfo
{
    int roomId;
    char title[ROOM_TITLE_LENGTH];
    short userCount;
};

// �� ��� ���� (Server -> Client)
struct RoomListResponsePacket
{
    PacketHeader header;
    short roomCount;
    RoomInfo rooms[MAX_ROOM_COUNT];
};

// �� ���� ��û (Client -> Server)
struct CreateRoomRequestPacket
{
    PacketHeader header;
    char title[ROOM_TITLE_LENGTH];
};

// �� ���� ���� (Server -> Client)
struct CreateRoomResponsePacket
{
    PacketHeader header;
    bool success;
    RoomInfo createdRoomInfo; // ���� �� ������ �� ����
};

// �� ���� ��û (Client -> Server)
struct JoinRoomRequestPacket
{
    PacketHeader header;
    int roomId;
};

// �� ���� ���� (Server -> Client)
struct JoinRoomResponsePacket
{
    PacketHeader header;
    bool success;
    RoomInfo joinedRoomInfo; // ���� �� ������ �� ����
};

#pragma pack(pop)
// =============================================================