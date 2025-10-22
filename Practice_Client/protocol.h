//#pragma once
//// =============================================================
////                       프로토콜 정의
//// =============================================================
//
//// 패킷의 종류를 구분하기 위한 열거형
//enum class PacketType : short
//{
//    LOGIN_REQUEST = 1,
//    CHAT_MESSAGE = 2,
//};
//
//// 모든 패킷의 맨 앞에 붙을 헤더
//// __pragma(pack(1))은 구조체 멤버 사이에 여분의 공간(padding)을 넣지 않도록 하여
//// 네트워크로 전송했을 때 데이터의 크기를 예측 가능하게 만듭니다.
//#pragma pack(push, 1)
//struct PacketHeader
//{
//    short packetSize;
//    PacketType packetType;
//};
//
//// 채팅 메시지 패킷 구조체
//struct ChatMessagePacket
//{
//    PacketHeader header;
//    // C++ string 대신 고정 크기 배열을 사용해야 패킷 크기 계산이 용이합니다.
//    char message[256];
//};
//#pragma pack(pop)
//
//// =============================================================

#pragma once
// =============================================================
//                       프로토콜 정의
// =============================================================
constexpr int NICKNAME_LENGTH = 32;
constexpr int CHAT_LENGTH = 256;

// 패킷의 종류를 구분하기 위한 열거형
enum class PacketType : short
{
    LOGIN_REQUEST = 1,
    LOGIN_RESPONSE = 2, // --- 타입을 하나로 통합하고, 구조체 내부 플래그로 성공/실패 구분

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
#pragma pack(pop)

// =============================================================