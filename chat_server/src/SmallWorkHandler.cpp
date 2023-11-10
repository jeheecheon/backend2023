#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>
#include <mutex>

#include "include/GlobalVariables.h"
#include "include/Work.h"
#include "include/SmallWorkHandler.h"

#include "protobuf/message.pb.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace std;
using namespace rapidjson;


void HandleSmallWork() {
    cout << "메시지 작업 쓰레드 #" << this_thread::get_id() << " 생성" << endl;

    while (signalReceived.load() == false) {
        Work work;
        {
            unique_lock<mutex> messagesQueueLock(messagesQueueMutex);
            while (messagesQueue.empty())
                messagesQueueEdited.wait(messagesQueueLock);

            work = messagesQueue.front();
            messagesQueue.pop();
        }

        if (IsJson) {
            Document jsonDoc;
            jsonDoc.Parse(work.dataToParse);

            if (jsonDoc.HasParseError()) {
                cerr << "Failed to parse JSON data." << endl;
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            jsonDoc.Accept(writer);
            cout << "JSON Data: " << buffer.GetString() << endl;

            // // Json 데이터 Type 멤버의 값 접근
            // if (jsonDoc.HasMember("type") && jsonDoc["type"].IsString()) {
            //     const char* typeValue = jsonDoc["type"].GetString();
            //     // jsonHandlers[typeValue]();
            //     cout << typeValue << endl;
            // } else
            //     cerr << "Missing or invalid 'type' key in JSON data." <<
            //     endl;

        } else {  // protobuf로 처리중인 경우
            // protobuf 는 두 개의 메시지로 분리하여 전송 받음
            // 첫 번째 메시지가 MessageType
            // 두 번째 메시지는 세부 타입이다

            mju::Type type;
            type.ParseFromString(work.dataToParse);
            mju::Type::MessageType messageType(type.type());

            switch (messageType) {
                case 0:
                    mju::CSName* csName = new mju::CSName;
                    csName->ParseFromString(work.dataToParse);
                    cout << csName->name().length() << endl;
                    break;
            }

            protobufHandlers[messageType](work.dataToParse);
        }
        {
            lock_guard<mutex> socketsOnQueueLock(socketsOnQueueMutex);
            socketsOnQueue.erase(work.dataOwner);
        }
    }

    cout << "메시지 작업 쓰레드 #" << this_thread::get_id() << " 종료" << endl;

    return;
}