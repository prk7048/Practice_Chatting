#pragma once
// =============================================================
//                       프로토콜 정의
// =============================================================
constexpr int NICKNAME_LENGTH = 32;
constexpr int CHAT_LENGTH = 256;
constexpr int ROOM_TITLE_LENGTH = 32;
constexpr int MAX_ROOM_COUNT = 50;

// 패킷의 종류를 구분하기 위한 열거형
enum class PacketType : short
{
    // 로그인 관련
    LOGIN_REQUEST = 1,
    LOGIN_RESPONSE = 2,

    // 채팅 관련
    CHAT_MESSAGE_REQUEST = 10,
    CHAT_MESSAGE_BROADCAST = 11,

    // 시스템 메시지 관련
    SYSTEM_MESSAGE_BROADCAST = 20,

    // --- 채팅방 관련 ---
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

// 로그인 요청 패킷 (Client -> Server)
struct LoginRequestPacket
{
    PacketHeader header;
    char nickname[NICKNAME_LENGTH];
};

// 로그인 응답 패킷 (Server -> Client)
struct LoginResponsePacket
{
    PacketHeader header;
    bool success; // true: 성공, false: 실패(중복 닉네임)
};

// 채팅 요청 패킷 (Client -> Server)
struct ChatMessageRequestPacket
{
    PacketHeader header;
    char message[CHAT_LENGTH];
};

// 채팅 브로드캐스트 패킷 (Server -> Client)
struct ChatMessageBroadcastPacket
{
    PacketHeader header;
    char nickname[NICKNAME_LENGTH];
    char message[CHAT_LENGTH];
};

// 시스템 메시지 브로드캐스트 패킷 (Server -> Client)
struct SystemMessageBroadcastPacket
{
    PacketHeader header;
    char message[CHAT_LENGTH];
};

// 방 정보 구조체 (ROOM_LIST_RESPONSE 에서 사용)
struct RoomInfo
{
    int roomId;
    char title[ROOM_TITLE_LENGTH];
    short userCount;
};

// 방 목록 응답 (Server -> Client)
struct RoomListResponsePacket
{
    PacketHeader header;
    short roomCount;
    RoomInfo rooms[MAX_ROOM_COUNT];
};

// 방 생성 요청 (Client -> Server)
struct CreateRoomRequestPacket
{
    PacketHeader header;
    char title[ROOM_TITLE_LENGTH];
};

// 방 생성 응답 (Server -> Client)
struct CreateRoomResponsePacket
{
    PacketHeader header;
    bool success;
    RoomInfo createdRoomInfo; // 성공 시 생성된 방 정보
};

// 방 입장 요청 (Client -> Server)
struct JoinRoomRequestPacket
{
    PacketHeader header;
    int roomId;
};

// 방 입장 응답 (Server -> Client)
struct JoinRoomResponsePacket
{
    PacketHeader header;
    bool success;
    RoomInfo joinedRoomInfo; // 성공 시 입장한 방 정보
};

#pragma pack(pop)
// =============================================================