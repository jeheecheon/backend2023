#ifndef SMALL_WORK_HANDLER_H
#define SMALL_WORK_HANDLER_H

#include <iostream>
#include <thread>
#include <mutex>

#include "include/GlobalVariables.h"
#include "include/Work.h"

#include "protobuf/message.pb.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

bool CustomReceive(int clientSock, void* buf, size_t size, int& numRecv);

void HandleSmallWork();

#endif  // WORK_HANDLER_H
