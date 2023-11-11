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

// 시그널이 수신되었는지를 나타내는 플래그
extern volatile atomic<bool> signalReceived;

#endif