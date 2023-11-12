#include "../include/MessageHandlers.h"
#include "../include/ChatServer.h"

#include "../include/SmallWork.h"
#include "../include/rapidjson/document.h"
#include "../include/rapidjson/stringbuffer.h"
#include "../include/rapidjson/writer.h"

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
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);
        prevName = user->GetUserName();
        user->userName = name;
    }

    // 보낼 메시지 내용을 미리 만든다
    string text = prevName + " 의 이름이 " + name + " 으로 변경되었습니다.";

    char* msgToSend = nullptr; // 보낼 메시지
    short bytesToSend; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::IsJson) {
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
            lock_guard<mutex> usersLock(ChatServer::UsersMutex);
            {
                lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

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
    if (ChatServer::IsJson) { // SCRoomsResult
        // RapidJSON document 생성
        rapidjson::Document jsonDoc;
        jsonDoc.SetObject();

        // "type" : "SCRoomsResult" 추가
        rapidjson::Value typeValue;
        typeValue.SetString("SCRoomsResult", jsonDoc.GetAllocator());
        jsonDoc.AddMember("type", typeValue, jsonDoc.GetAllocator());

        rapidjson::Value roomsArray(kArrayType);
        {
            lock_guard<mutex> usersLock(ChatServer::UsersMutex);
            {
                lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

                for (auto& r : ChatServer::Rooms) {
                    // Room 1
                    rapidjson::Value roomObject(kObjectType);
                    
                    rapidjson::Value roomIdValue;
                    roomIdValue.SetInt(r->roomId);
                    roomObject.AddMember("roomId", roomIdValue, jsonDoc.GetAllocator());

                    rapidjson::Value titleString;
                    titleString.SetString(r->title.c_str(), jsonDoc.GetAllocator());
                    roomObject.AddMember("title", titleString, jsonDoc.GetAllocator());

                    rapidjson::Value membersArray(kArrayType);
                    for (auto& u : ChatServer::Users) {
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

    // 유저가 이미 접속한 방이 있는지 확인
    bool isUserInRoom;
    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);
        isUserInRoom = user->roomThisUserIn != nullptr;
        cout << isUserInRoom << endl;
    }

    int roomId;
    // 유저가 속한 방이 없는 경우 새로운 방을 생성
    if (!isUserInRoom) {
        shared_ptr<Room> newRoom;
        newRoom = make_shared<Room>(title);
        roomId = newRoom->roomId;

        ChatServer::RoomsMutex.lock();
        ChatServer::Rooms.insert(newRoom);
        ChatServer::RoomsMutex.unlock();

        // 생성한 방에 접속
        OnCsJoinRoom(clientSock, (const void*)&roomId);
    }
    // 유저가 속한 방이 이미 있는 경우
    else {
        string text = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
        char* msgToSend = nullptr; // 보낼 메시지
        short bytesToSend; // 보낼 데이터의 바이트 수

        if (ChatServer::IsJson) {
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
                ChatServer::CustomSend(clientSock, msgToSend, bytesToSend);
            
                delete[] msgToSend;
            }
        }
    }
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

    bool isUserInRoom; // 유저가 채팅방에 들어가 있는지
    string textForOtherUsers;
    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);

        //유저가 채팅방에 접속해 있지 않은 경우
        isUserInRoom = user->roomThisUserIn != nullptr;
        if (!isUserInRoom) {
            lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

            // 입력된 채팅방 번호와 일치하는 방을 찾아 접속 시킨다
            for (auto r : ChatServer::Rooms)
                if (r->roomId == roomId) {
                    r->usersInThisRoom.insert(user);
                    user->roomThisUserIn = r;
                    break;
                }
            
            // 보낼 메시지 내용을 미리 만든다
            textForOtherUsers = user->GetUserName() + " 님이 입장했습니다.";
        }
    }

    // 보낼 메시지 내용을 미리 만든다
    string textForUserGettingIn;
    {
        lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

        // 접속한 채팅방이 있으면
        if (isUserInRoom) 
            textForUserGettingIn = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
        
        // 접속한 채팅방이 없으면
        else {
            for (auto r : ChatServer::Rooms) 
                if (r->roomId == roomId) {
                    textForUserGettingIn = "방제[" + r->title + "] 방에 입장했습니다.";
                    break;
                }
        }
    }

    char* msgToSendForUserGettingIn = nullptr; // 보낼 메시지
    short bytesToSendForUserGettingIn; // 보낼 데이터의 바이트 수
    char* msgToSendForOtherUsers = nullptr; // 보낼 메시지
    short bytesToSendForOtherUsers; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::IsJson) {
        // RapidJSON document 생성
        rapidjson::Document jsonDocForUserGettingIn;
        jsonDocForUserGettingIn.SetObject();

        // "type" : "SCSystemMessage" 추가
        rapidjson::Value typeValueForUserGettingIn;
        typeValueForUserGettingIn.SetString("SCSystemMessage", jsonDocForUserGettingIn.GetAllocator());
        jsonDocForUserGettingIn.AddMember("type", typeValueForUserGettingIn, jsonDocForUserGettingIn.GetAllocator());

        // "text" : + 시스템 메시지 추가
        rapidjson::Value textValueForUserGettingIn;
        textValueForUserGettingIn.SetString(textForUserGettingIn.c_str(), jsonDocForUserGettingIn.GetAllocator());
        jsonDocForUserGettingIn.AddMember("text", textValueForUserGettingIn, jsonDocForUserGettingIn.GetAllocator());

        // document 를 json string으로 변환
        rapidjson::StringBuffer bufferForUserGettingIn;
        rapidjson::Writer<rapidjson::StringBuffer> writerForUserGettingIn(bufferForUserGettingIn);
        jsonDocForUserGettingIn.Accept(writerForUserGettingIn);
        string jsonStringForUserGettingIn = bufferForUserGettingIn.GetString();

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonStringForUserGettingIn.length());
        bytesToSendForUserGettingIn = jsonStringForUserGettingIn.length() + 2;
        msgToSendForUserGettingIn = new char[bytesToSendForUserGettingIn];
        memcpy(msgToSendForUserGettingIn, &msgBytesInBigEnndian, 2);
        memcpy(msgToSendForUserGettingIn + 2, jsonStringForUserGettingIn.c_str(), jsonStringForUserGettingIn.length());

        if (!isUserInRoom) {
            // 새로운 JSON 문자열을 만들기 위해 jsonDoc 초기화
            jsonDocForUserGettingIn.SetObject();

            // "type" : "SCSystemMessage" 추가
            rapidjson::Value typeValueForOtherUsers;
            typeValueForOtherUsers.SetString("SCSystemMessage", jsonDocForUserGettingIn.GetAllocator());
            jsonDocForUserGettingIn.AddMember("type", typeValueForOtherUsers, jsonDocForUserGettingIn.GetAllocator());

            // "text" : + 시스템 메시지 추가
            rapidjson::Value textValueForOtherUsers;
            textValueForOtherUsers.SetString(textForOtherUsers.c_str(), jsonDocForUserGettingIn.GetAllocator());
            jsonDocForUserGettingIn.AddMember("text", textValueForOtherUsers, jsonDocForUserGettingIn.GetAllocator());

            // document 를 json string으로 변환
            rapidjson::StringBuffer bufferForOtherUsers;
            rapidjson::Writer<rapidjson::StringBuffer> writerForOtherUsers(bufferForOtherUsers);
            jsonDocForUserGettingIn.Accept(writerForOtherUsers);
            string jsonStringForOtherUsers = bufferForOtherUsers.GetString();

            // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
            msgBytesInBigEnndian = htons(jsonStringForOtherUsers.length());
            bytesToSendForOtherUsers = jsonStringForOtherUsers.length() + 2;
            msgToSendForOtherUsers = new char[bytesToSendForOtherUsers];
            memcpy(msgToSendForOtherUsers, &msgBytesInBigEnndian, 2);
            memcpy(msgToSendForOtherUsers + 2, jsonStringForOtherUsers.c_str(), jsonStringForOtherUsers.length());
        }
    } 
    // protobuf 포맷
    else {

    }

    if (msgToSendForUserGettingIn != nullptr) {
        {
            lock_guard<mutex> usersLock(ChatServer::UsersMutex);
            ChatServer::CustomSend(user->socketNumber, msgToSendForUserGettingIn, bytesToSendForUserGettingIn);
        }
        delete[] msgToSendForUserGettingIn;
    }
    if (msgToSendForOtherUsers != nullptr) {
        if (!isUserInRoom) {
            lock_guard<mutex> usersLock(ChatServer::UsersMutex);
            
            if (user->roomThisUserIn != nullptr) {
                lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

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
    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    bool isUserInRoom = false;
    const char* userName = nullptr;
    string textForUserExiting;
    string textForOtherUsers;

    shared_ptr<Room> room = nullptr;

    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);
        textForOtherUsers = user->GetUserName() + " 님이 퇴장했습니다.";

        isUserInRoom = user->roomThisUserIn != nullptr;
        if (isUserInRoom) {
            lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

            room = user->roomThisUserIn; // 유저가 현재 속한 방 저장
            textForUserExiting = "방제[" + room->title + "] 대화 방에서 퇴장했습니다.";

            // 유저가 속한 방에서 유저를 찾는다
            auto it = find(room->usersInThisRoom.begin(), room->usersInThisRoom.end(), user);
            if (it != room->usersInThisRoom.end()) 
                room->usersInThisRoom.erase(it);  // 유저를 방 내역에서 삭제
            user->roomThisUserIn = nullptr; // 유저를 내보낸다

            // 채팅방에 아무도 없다면 채팅방을 삭제
            if (room->usersInThisRoom.size() == 0)
                ChatServer::Rooms.erase(room);
        }
    }

    char* msgToSendForUserExiting = nullptr; // 보낼 메시지
    short bytesToSendForUserExiting; // 보낼 데이터의 바이트 수
    char* msgToSendForOtherUsers = nullptr; // 보낼 메시지
    short bytesToSendForOtherUsers; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::IsJson) {
        string jsonStringForUserExiting;
        string jsonStringForOtherUsers;

        if (!isUserInRoom) {
            // RapidJSON document 생성
            rapidjson::Document jsonDocForUserExiting;
            jsonDocForUserExiting.SetObject();

            // "type" : "SCSystemMessage" 추가
            rapidjson::Value typeValueForUserExiting;
            typeValueForUserExiting.SetString("SCSystemMessage", jsonDocForUserExiting.GetAllocator());
            jsonDocForUserExiting.AddMember("type", typeValueForUserExiting, jsonDocForUserExiting.GetAllocator());

            // "text" : + 시스템 메시지 추가
            rapidjson::Value textValueForUserExiting;
            textValueForUserExiting.SetString(textForUserExiting.c_str(), jsonDocForUserExiting.GetAllocator());
            jsonDocForUserExiting.AddMember("text", textValueForUserExiting, jsonDocForUserExiting.GetAllocator());

            // document 를 json string으로 변환
            rapidjson::StringBuffer bufferForUserExiting;
            rapidjson::Writer<rapidjson::StringBuffer> writerForUserExiting(bufferForUserExiting);
            jsonDocForUserExiting.Accept(writerForUserExiting);
            jsonStringForUserExiting = bufferForUserExiting.GetString();
        } else {
            // RapidJSON document 생성
            rapidjson::Document jsonDocForUserExiting;
            jsonDocForUserExiting.SetObject();

            // "type" : "SCSystemMessage" 추가
            rapidjson::Value typeValueForUserExiting;
            typeValueForUserExiting.SetString("SCSystemMessage", jsonDocForUserExiting.GetAllocator());
            jsonDocForUserExiting.AddMember("type", typeValueForUserExiting, jsonDocForUserExiting.GetAllocator());

            // "text" : + 시스템 메시지 추가
            rapidjson::Value textValueForUserExiting;
            textValueForUserExiting.SetString(textForUserExiting.c_str(), jsonDocForUserExiting.GetAllocator());
            jsonDocForUserExiting.AddMember("text", textValueForUserExiting, jsonDocForUserExiting.GetAllocator());

            // document 를 json string으로 변환
            rapidjson::StringBuffer bufferForUserExiting;
            rapidjson::Writer<rapidjson::StringBuffer> writerForUserExiting(bufferForUserExiting);
            jsonDocForUserExiting.Accept(writerForUserExiting);
            jsonStringForUserExiting = bufferForUserExiting.GetString();

            // 새로운 JSON 문자열을 만들기 위해 jsonDoc 초기화
            rapidjson::Document jsonDocForOtherUsers;
            jsonDocForOtherUsers.SetObject();

            // "type" : "SCSystemMessage" 추가
            rapidjson::Value typeValueForOtherUsers;
            typeValueForOtherUsers.SetString("SCSystemMessage", jsonDocForOtherUsers.GetAllocator());
            jsonDocForOtherUsers.AddMember("type", typeValueForOtherUsers, jsonDocForOtherUsers.GetAllocator());

            // "text" : + 시스템 메시지 추가
            rapidjson::Value textValueForOtherUsers;
            textValueForOtherUsers.SetString(textForOtherUsers.c_str(), jsonDocForOtherUsers.GetAllocator());
            jsonDocForOtherUsers.AddMember("text", textValueForOtherUsers, jsonDocForOtherUsers.GetAllocator());

            // document 를 json string으로 변환
            rapidjson::StringBuffer bufferForOtherUsers;
            rapidjson::Writer<rapidjson::StringBuffer> writerForOtherUsers(bufferForOtherUsers);
            jsonDocForOtherUsers.Accept(writerForOtherUsers);
            jsonStringForOtherUsers = bufferForOtherUsers.GetString();

            // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
            short msgBytesInBigEnndian = htons(jsonStringForOtherUsers.length());
            bytesToSendForOtherUsers = jsonStringForOtherUsers.length() + 2;
            msgToSendForOtherUsers = new char[bytesToSendForOtherUsers];
            memcpy(msgToSendForOtherUsers, &msgBytesInBigEnndian, 2);
            memcpy(msgToSendForOtherUsers + 2, jsonStringForOtherUsers.c_str(), jsonStringForOtherUsers.length());
        }

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonStringForUserExiting.length());
        bytesToSendForUserExiting = jsonStringForUserExiting.length() + 2;
        msgToSendForUserExiting = new char[bytesToSendForUserExiting];
        memcpy(msgToSendForUserExiting, &msgBytesInBigEnndian, 2);
        memcpy(msgToSendForUserExiting + 2, jsonStringForUserExiting.c_str(), jsonStringForUserExiting.length());
    }

    // protobuf 포맷
    else {

    }

    if (msgToSendForUserExiting != nullptr) {
        ChatServer::CustomSend(user->socketNumber, msgToSendForUserExiting, bytesToSendForUserExiting);

        if (isUserInRoom) {
            {
                lock_guard<mutex> usersLock(ChatServer::UsersMutex);
                {
                    lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);
                    if (room != nullptr) 
                        for (auto u : room->usersInThisRoom) 
                            ChatServer::CustomSend(u->socketNumber, msgToSendForOtherUsers, bytesToSendForOtherUsers);
                }
            }

            delete[] msgToSendForOtherUsers;
        }
            
        delete[] msgToSendForUserExiting;
    }
}

void OnCsChat(int clientSock, const void* data) {
    string text = (char*)data;

    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    string userName;
    bool isUserInRoom = false;
    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);
        isUserInRoom = user->roomThisUserIn != nullptr;
        userName = user->GetUserName();
    }
    char* msgToSend = nullptr; // 보낼 메시지
    short bytesToSend; // 보낼 데이터의 바이트 수

    // json 포맷
    if (ChatServer::IsJson) {
        // RapidJSON document 생성
        rapidjson::Document jsonDoc;
        jsonDoc.SetObject();

        if (isUserInRoom) {
            // "type" : "SCSystemMessage" 추가
            rapidjson::Value typeValue;
            typeValue.SetString("SCChat", jsonDoc.GetAllocator());
            jsonDoc.AddMember("type", typeValue, jsonDoc.GetAllocator());

            // "member" : + 시스템 메시지 추가
            rapidjson::Value memberValue;
            memberValue.SetString(userName.c_str(), jsonDoc.GetAllocator());
            jsonDoc.AddMember("member", memberValue, jsonDoc.GetAllocator());

            // "text" : + 시스템 메시지 추가
            rapidjson::Value textValue;
            textValue.SetString(text.c_str(), jsonDoc.GetAllocator());
            jsonDoc.AddMember("text", textValue, jsonDoc.GetAllocator());
        } else {
            // "type" : "SCSystemMessage" 추가
            rapidjson::Value typeValue;
            typeValue.SetString("SCSystemMessage", jsonDoc.GetAllocator());
            jsonDoc.AddMember("type", typeValue, jsonDoc.GetAllocator());

            // "text" : + 시스템 메시지 추가
            rapidjson::Value textValue;
            textValue.SetString("현재 대화방에 들어가 있지 않습니다.", jsonDoc.GetAllocator());
            jsonDoc.AddMember("text", textValue, jsonDoc.GetAllocator());
        }

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

    }

    // 메시지 전송
    if (msgToSend != nullptr) {
        {   
            {
                lock_guard<mutex> usersLock(ChatServer::UsersMutex);
                {
                    lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

                    if (isUserInRoom) {
                        for (auto& u : user->roomThisUserIn->usersInThisRoom)
                            if (u->socketNumber != clientSock)
                                ChatServer::CustomSend(u->socketNumber, msgToSend, bytesToSend);
                    }
                }
            }

            if (!isUserInRoom)
                ChatServer::CustomSend(clientSock, msgToSend, bytesToSend);
            
            delete[] msgToSend;
        }
    }
}

void OnCsShutDown(int clientSock, const void* data) {
    ChatServer& server = ChatServer::CreateSingleton();
    server.Stop();
}