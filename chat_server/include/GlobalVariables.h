#pragma once

// 서버 옵션 설정.
extern bool IsJson = true; // Json 또는 Protobuf
extern int Port = -1; // Active Socket Port 번호


// 읽기 이벤트가 발생한 클라이언트 queue
extern queue<Work> messagesQueue;
extern mutex messagesQueueMutex;
extern condition_variable messagesQueueEdited;


// messagesQueue에서 아직 연산을 기다리는 Sockets들을 담는 set
extern unordered_set<int> socketsOnQueue;
extern mutex socketsOnQueueMutex;


// 종료할 클라이언트 소켓 set
unordered_set<int> willClose;


// 전역 변수로서 시그널이 수신되었는지를 나타내는 플래그
volatile atomic<bool> signalReceived(false);


// Worker Thread의 MessageHandlers
typedef void (*MessageHandler)(const char* data);
unordered_map<const char*, MessageHandler> jsonHandlers;
unordered_map<mju::Type::MessageType, MessageHandler> protobufHandlers;