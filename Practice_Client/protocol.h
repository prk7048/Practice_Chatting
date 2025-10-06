#pragma once
// =============================================================
//                       프로토콜 정의
// =============================================================

// 패킷의 종류를 구분하기 위한 열거형
enum class PacketType : short
{
    LOGIN_REQUEST = 1,
    CHAT_MESSAGE = 2,
};

// 모든 패킷의 맨 앞에 붙을 헤더
// __pragma(pack(1))은 구조체 멤버 사이에 여분의 공간(padding)을 넣지 않도록 하여
// 네트워크로 전송했을 때 데이터의 크기를 예측 가능하게 만듭니다.
#pragma pack(push, 1)
struct PacketHeader
{
    short packetSize;
    PacketType packetType;
};

// 채팅 메시지 패킷 구조체
struct ChatMessagePacket
{
    PacketHeader header;
    // C++ string 대신 고정 크기 배열을 사용해야 패킷 크기 계산이 용이합니다.
    char message[256];
};
#pragma pack(pop)

// =============================================================