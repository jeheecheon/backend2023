#pragma once
#ifndef GLOBAL_VARIABLES_H
#define GLOBAL_VARIABLES_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <atomic>
#include <unordered_map>
#include <iostream>

#include "../protobuf/message.pb.h"
#include "SmallWork.h"

using namespace std;

// 서버 옵션 설정.
extern bool IsJson;  // Json 또는 Protobuf

// 읽기 이벤트가 발생한 클라이언트 queue
extern queue<SmallWork> messagesQueue;
extern mutex messagesQueueMutex;
extern condition_variable messagesQueueEdited;

// messagesQueue에서 아직 연산을 기다리는 Sockets들을 담는 set
extern unordered_set<int> socketsOnQueue;
extern mutex socketsOnQueueMutex;

// 시그널이 수신되었는지를 나타내는 플래그
extern volatile atomic<bool> signalReceived;

// Worker Thread의 MessageHandlers
typedef void (*MessageHandler)(const char* data);
extern unordered_map<const char*, MessageHandler> jsonHandlers;
extern unordered_map<mju::Type::MessageType, MessageHandler> protobufHandlers;

#endif