#include "../include/MessageHandlers.h"
#include "../include/ChatServer.h"

#include "../include/SmallWork.h"
#include "../rapidjson/document.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"

#include <iostream>

using namespace rapidjson;
using namespace std;

void OnCsName(int clientSock, const void* data) {    
    string name = (char*)data; // Neither null nor empty.. Have Checked it already!!!!!!!

    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    // 찾은 유저의 이름을 가져온다
    string prevName;
    {
        lock_guard<mutex> usersLock(ChatServer::usersMutex);
        prevName = user->GetUserName();
        user->userName = name;
    }

    // 보낼 메시지 내용을 미리 만든다
    string text = prevName + " 의 이름이 " + name + " 으로 변경되었습니다.";

    char* msgToSend = nullptr; // 보낼 메시지
    short bytesToSend; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::isJson) {
        // RapidJSON document 생성
        rapidjson::Document jsonDoc;
        jsonDoc.SetObject();

        // "type" : "SCSystemMessage" 추가
        rapidjson::Value typeValue;
        typeValue.SetString("SCSystemMessage", jsonDoc.GetAllocator());
        jsonDoc.AddMember("type", typeValue, jsonDoc.GetAllocator());

        // "text" : + 시스템 메시지 추가
        rapidjson::Value textValue;
        textValue.SetString(text.c_str(), jsonDoc.GetAllocator());
        jsonDoc.AddMember("text", textValue, jsonDoc.GetAllocator());

        // document 를 json string으로 변환
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        jsonDoc.Accept(writer);
        string jsonString = buffer.GetString();

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonString.length());
        bytesToSend = jsonString.length() + 2;
        msgToSend = new char[bytesToSend];
        memcpy(msgToSend, &msgBytesInBigEnndian, 2);
        memcpy(msgToSend + 2, jsonString.c_str(), jsonString.length());
    } 
    // protobuf 포맷
    else {
        // TODO
    }

    // 메시지 전송
    if (msgToSend != nullptr) {
        {
            lock_guard<mutex> usersLock(ChatServer::usersMutex);
            {
                lock_guard<mutex> roomsLock(ChatServer::roomsMutex);

                if (user->roomThisUserIn == nullptr)
                    ChatServer::CustomSend(clientSock, msgToSend, bytesToSend);
                else {
                    for (auto& u : user->roomThisUserIn->usersInThisRoom) 
                        ChatServer::CustomSend(u->socketNumber, msgToSend, bytesToSend);
                }
            }
            
            delete[] msgToSend;
        }
    }
}

void OnCsRooms(int clientSock, const void* data) {

    char* msgToSend = nullptr; // 보낼 메시지
    short bytesToSend; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::isJson) { // SCRoomsResult
        // RapidJSON document 생성
        rapidjson::Document jsonDoc;
        jsonDoc.SetObject();

        // "type" : "SCRoomsResult" 추가
        rapidjson::Value typeValue;
        typeValue.SetString("SCRoomsResult", jsonDoc.GetAllocator());
        jsonDoc.AddMember("type", typeValue, jsonDoc.GetAllocator());

        rapidjson::Value roomsArray(kArrayType);
        {
            lock_guard<mutex> usersLock(ChatServer::usersMutex);
            {
                lock_guard<mutex> roomsLock(ChatServer::roomsMutex);

                for (auto& r : ChatServer::rooms) {
                    // Room 1
                    rapidjson::Value roomObject(kObjectType);
                    
                    rapidjson::Value roomIdValue;
                    roomIdValue.SetInt(r->roomId);
                    roomObject.AddMember("roomId", roomIdValue, jsonDoc.GetAllocator());

                    rapidjson::Value titleString;
                    titleString.SetString(r->title.c_str(), jsonDoc.GetAllocator());
                    roomObject.AddMember("title", titleString, jsonDoc.GetAllocator());

                    rapidjson::Value membersArray(kArrayType);
                    for (auto& u : ChatServer::users) {
                        rapidjson::Value memberName;
                        memberName.SetString(u->GetUserName().c_str(), jsonDoc.GetAllocator());

                        membersArray.PushBack(memberName, jsonDoc.GetAllocator());
                    }
                    roomObject.AddMember("members", membersArray, jsonDoc.GetAllocator());

                    roomsArray.PushBack(roomObject, jsonDoc.GetAllocator());
                }
            }
        }
        jsonDoc.AddMember("rooms", roomsArray, jsonDoc.GetAllocator());

        // Convert the document to a JSON string
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        jsonDoc.Accept(writer);
        string jsonString = buffer.GetString();
        cout << jsonString << endl;

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonString.length());
        bytesToSend = jsonString.length() + 2;
        msgToSend = new char[bytesToSend];
        memcpy(msgToSend, &msgBytesInBigEnndian, 2);
        memcpy(msgToSend + 2, jsonString.c_str(), jsonString.length());
    }
    // protobuf 포맷
    else {

    }
    // 메시지 전송
    if (msgToSend != nullptr) {
        ChatServer::CustomSend(clientSock, msgToSend, bytesToSend);
        delete[] msgToSend;
    }
}

void OnCsCreateRoom(int clientSock, const void* data) {
    string title = (char*)data; // Neither null nor empty.. Have Checked it already!!!!!!!

    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    shared_ptr<Room> newRoom;
    int roomId;
    {
        lock_guard<mutex> roomsLock(ChatServer::roomsMutex);
        newRoom = make_shared<Room>(title);
        ChatServer::rooms.insert(newRoom);
        roomId = newRoom->roomId;
    }

    OnCsJoinRoom(clientSock, (const void*)&roomId);
}

void OnCsJoinRoom(int clientSock, const void* data) {
    // 방번호는 클라이언트에서 확인을 거침

    int roomId = *((int*)data);


    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }
    string textForOtherUsers;
    {
        lock_guard<mutex> usersLock(ChatServer::usersMutex);
        {
            lock_guard<mutex> roomsLock(ChatServer::roomsMutex);

            for (auto r : ChatServer::rooms)
                if (r->roomId == roomId) {
                    r->usersInThisRoom.push_back(user);
                    user->roomThisUserIn = r;
                    break;
                }
        }
        // 보낼 메시지 내용을 미리 만든다
        textForOtherUsers = user->GetUserName() + " 님이 입장했습니다.";
    }


    // 보낼 메시지 내용을 미리 만든다
    string textForUserJustEntered;
    {
        lock_guard<mutex> roomsLock(ChatServer::roomsMutex);
        for (auto r : ChatServer::rooms) 
            if (r->roomId == roomId) {
                textForUserJustEntered = "방제[" + r->title + "] 방에 입장했습니다.";
                break;
            }
    }

    char* msgToSendForUserJustEntered = nullptr; // 보낼 메시지
    short bytesToSendForUserJustEntered; // 보낼 데이터의 바이트 수
    char* msgToSendForOtherUsers = nullptr; // 보낼 메시지
    short bytesToSendForOtherUsers; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::isJson) {
        // RapidJSON document 생성
        rapidjson::Document jsonDocForUserJustEntered;
        jsonDocForUserJustEntered.SetObject();

        // "type" : "SCSystemMessage" 추가
        rapidjson::Value typeValueForUserJustEntered;
        typeValueForUserJustEntered.SetString("SCSystemMessage", jsonDocForUserJustEntered.GetAllocator());
        jsonDocForUserJustEntered.AddMember("type", typeValueForUserJustEntered, jsonDocForUserJustEntered.GetAllocator());

        // "text" : + 시스템 메시지 추가
        rapidjson::Value textValueForUserJustEntered;
        textValueForUserJustEntered.SetString(textForUserJustEntered.c_str(), jsonDocForUserJustEntered.GetAllocator());
        jsonDocForUserJustEntered.AddMember("text", textValueForUserJustEntered, jsonDocForUserJustEntered.GetAllocator());

        // document 를 json string으로 변환
        rapidjson::StringBuffer bufferForUserJustEntered;
        rapidjson::Writer<rapidjson::StringBuffer> writerForUserJustEntered(bufferForUserJustEntered);
        jsonDocForUserJustEntered.Accept(writerForUserJustEntered);
        string jsonStringForUserJustEntered = bufferForUserJustEntered.GetString();

        // 새로운 JSON 문자열을 만들기 위해 jsonDoc 초기화
        jsonDocForUserJustEntered.SetObject();

        // "type" : "SCSystemMessage" 추가
        rapidjson::Value typeValueForOtherUsers;
        typeValueForOtherUsers.SetString("SCSystemMessage", jsonDocForUserJustEntered.GetAllocator());
        jsonDocForUserJustEntered.AddMember("type", typeValueForOtherUsers, jsonDocForUserJustEntered.GetAllocator());

        // "text" : + 시스템 메시지 추가
        rapidjson::Value textValueForOtherUsers;
        textValueForOtherUsers.SetString(textForOtherUsers.c_str(), jsonDocForUserJustEntered.GetAllocator());
        jsonDocForUserJustEntered.AddMember("text", textValueForOtherUsers, jsonDocForUserJustEntered.GetAllocator());

        // document 를 json string으로 변환
        rapidjson::StringBuffer bufferForOtherUsers;
        rapidjson::Writer<rapidjson::StringBuffer> writerForOtherUsers(bufferForOtherUsers);
        jsonDocForUserJustEntered.Accept(writerForOtherUsers);
        string jsonStringForOtherUsers = bufferForOtherUsers.GetString();


        cout << jsonStringForOtherUsers << endl << jsonStringForUserJustEntered << endl;

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonStringForUserJustEntered.length());
        bytesToSendForUserJustEntered = jsonStringForUserJustEntered.length() + 2;
        msgToSendForUserJustEntered = new char[bytesToSendForUserJustEntered];
        memcpy(msgToSendForUserJustEntered, &msgBytesInBigEnndian, 2);
        memcpy(msgToSendForUserJustEntered + 2, jsonStringForUserJustEntered.c_str(), jsonStringForUserJustEntered.length());

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        msgBytesInBigEnndian = htons(jsonStringForOtherUsers.length());
        bytesToSendForOtherUsers = jsonStringForOtherUsers.length() + 2;
        msgToSendForOtherUsers = new char[bytesToSendForOtherUsers];
        memcpy(msgToSendForOtherUsers, &msgBytesInBigEnndian, 2);
        memcpy(msgToSendForOtherUsers + 2, jsonStringForOtherUsers.c_str(), jsonStringForOtherUsers.length());
    } 
    // protobuf 포맷
    else {

    }

    if (msgToSendForUserJustEntered != nullptr) {
        {
            lock_guard<mutex> usersLock(ChatServer::usersMutex);
            ChatServer::CustomSend(user->socketNumber, msgToSendForUserJustEntered, bytesToSendForUserJustEntered);
        }
        delete[] msgToSendForUserJustEntered;
    }
    if (msgToSendForOtherUsers != nullptr) {
        {
            lock_guard<mutex> usersLock(ChatServer::usersMutex);
            
            if (user->roomThisUserIn != nullptr) {
                lock_guard<mutex> roomsLock(ChatServer::roomsMutex);

                for (auto u : user->roomThisUserIn->usersInThisRoom) {
                    if (u->socketNumber != user->socketNumber) {
                        cout << u->socketNumber << endl;
                        ChatServer::CustomSend(u->socketNumber, msgToSendForOtherUsers, bytesToSendForOtherUsers);
                    }

                }
            }
        }
        delete[] msgToSendForOtherUsers;
    }
}

void OnCsLeaveRoom(int clientSock, const void* data) {
    cout << "OnCsLeaveRoom" << endl;
}

void OnCsChat(int clientSock, const void* data) {


    cout << "OnCsChat" << (char*)data << endl;
}

void OnCsShutDown(int clientSock, const void* data) {
    cout << "OnCsShutDown" << endl;
}