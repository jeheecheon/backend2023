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
    string name = (char*)data; // 변경할 이름

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
        string SCSystemMessageString = buffer.GetString();

        char* msgToSend = nullptr; // 보낼 메시지
        short bytesToSend; // 보낼 데이터의 바이트 수

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(SCSystemMessageString.length());
        bytesToSend = SCSystemMessageString.length() + 2;
        msgToSend = new char[bytesToSend];
        memcpy(msgToSend, &msgBytesInBigEnndian, 2);
        memcpy(msgToSend + 2, SCSystemMessageString.c_str(), SCSystemMessageString.length());

        // 데이터 전송
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
        }

        delete[] msgToSend;
    } 
    // protobuf 포맷
    else {
        // 먼저 보낼 Type 메시지 생성
        mju::Type type;
        type.set_type(mju::Type::MessageType::Type_MessageType_SC_SYSTEM_MESSAGE);
        const string typeString = type.SerializeAsString();

        // 뒤 따라 보낼 SCSystemMessage 메시지 생성
        mju::SCSystemMessage sysMessage;
        sysMessage.set_text(text);
        const string sysMessageString = sysMessage.SerializeAsString();

        // 보낼 데이터 변수 선언
        char* dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

        // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
        short bytesInBigEndian = htons(typeString.length());
        memcpy(dataToSend, &bytesInBigEndian, 2);
        memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

        // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
        bytesInBigEndian = htons(sysMessageString.length());
        memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
        memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

        // 데이터 전송
        ChatServer::CustomSend(clientSock, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());

        delete[] dataToSend;
    }
}

void OnCsRooms(int clientSock, const void* data) {
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

                // roomsArray에 내용 추가
                for (auto& r : ChatServer::Rooms) {
                    // room 객체 선언
                    rapidjson::Value roomObject(kObjectType);
                    
                    // roomId 추가 
                    rapidjson::Value roomIdValue;
                    roomIdValue.SetInt(r->roomId);
                    roomObject.AddMember("roomId", roomIdValue, jsonDoc.GetAllocator());

                    // title 추가
                    rapidjson::Value titleString;
                    titleString.SetString(r->title.c_str(), jsonDoc.GetAllocator());
                    roomObject.AddMember("title", titleString, jsonDoc.GetAllocator());

                    // members 추가
                    rapidjson::Value membersArray(kArrayType);
                    for (auto& u : r->usersInThisRoom) {
                        rapidjson::Value memberName;
                        memberName.SetString(u->GetUserName().c_str(), jsonDoc.GetAllocator());

                        membersArray.PushBack(memberName, jsonDoc.GetAllocator());
                    }
                    roomObject.AddMember("members", membersArray, jsonDoc.GetAllocator());

                    // room 객체 추가
                    roomsArray.PushBack(roomObject, jsonDoc.GetAllocator());
                }
            }
        }
        // roomsArray를 json Document에 추가
        jsonDoc.AddMember("rooms", roomsArray, jsonDoc.GetAllocator());

        // document 를 json string으로 변환
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        jsonDoc.Accept(writer);
        string jsonString = buffer.GetString();

        char* msgToSend = nullptr; // 보낼 메시지
        short bytesToSend; // 보낼 데이터의 바이트 수

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonString.length());
        bytesToSend = jsonString.length() + 2;
        msgToSend = new char[bytesToSend];
        memcpy(msgToSend, &msgBytesInBigEnndian, 2);
        memcpy(msgToSend + 2, jsonString.c_str(), jsonString.length());
        
        // 데이터 전송
        ChatServer::CustomSend(clientSock, msgToSend, bytesToSend);

        delete[] msgToSend;
    }
    // protobuf 포맷
    else {
        // 먼저 보낼 Type 메시지 생성
        mju::Type type;
        type.set_type(mju::Type::MessageType::Type_MessageType_SC_ROOMS_RESULT);
        const string typeString = type.SerializeAsString();

        // 뒤 따라 보낼 SCSystemMessage 메시지 생성
        mju::SCRoomsResult roomsResult;
        {
            lock_guard<mutex> usersLock(ChatServer::UsersMutex);
            {
                lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

                // roomsResult 에 내용 추가
                for (auto& r : ChatServer::Rooms) {
                    // room 객체 선언
                    mju::SCRoomsResult::RoomInfo* room = roomsResult.add_rooms();

                    // roomId 추가 
                    room->set_roomid(r->roomId);

                    // title 추가
                    room->set_title(r->title);

                    // members 추가
                    for (auto& u : r->usersInThisRoom) {
                        string* member = room->add_members();
                        *member = u->GetUserName();
                    }
                }
            }
        }
        const string roomsResultString = roomsResult.SerializeAsString();

        // 보낼 데이터 변수 선언
        char* dataToSend = new char[2 + typeString.length() + 2 + roomsResultString.length()];

        // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
        short bytesInBigEndian = htons(typeString.length());
        memcpy(dataToSend, &bytesInBigEndian, 2);
        memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

        // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
        bytesInBigEndian = htons(roomsResultString.length());
        memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
        memcpy(dataToSend + 2 + typeString.length() + 2, roomsResultString.c_str(), roomsResultString.length());

        // 데이터 전송
        ChatServer::CustomSend(clientSock, dataToSend, 2 + typeString.length() + 2 + roomsResultString.length());

        delete[] dataToSend;
    }
}

void OnCsCreateRoom(int clientSock, const void* data) {
    string title = (char*)data; // 방제

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
        string text = "대화 방에 있을 때는 방을 개설 할 수 없습니다.";

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

            char* msgToSend = nullptr; // 보낼 메시지
            short bytesToSend; // 보낼 데이터의 바이트 수

            // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
            short msgBytesInBigEnndian = htons(jsonString.length());
            bytesToSend = jsonString.length() + 2;
            msgToSend = new char[bytesToSend];
            memcpy(msgToSend, &msgBytesInBigEnndian, 2);
            memcpy(msgToSend + 2, jsonString.c_str(), jsonString.length());

            // 데이터 전송
            ChatServer::CustomSend(clientSock, msgToSend, bytesToSend);

            delete[] msgToSend;
        } 
        // protobuf 포맷
        else {
            // 먼저 보낼 Type 메시지 생성
            mju::Type type;
            type.set_type(mju::Type::MessageType::Type_MessageType_SC_SYSTEM_MESSAGE);
            const string typeString = type.SerializeAsString();

            // 뒤 따라 보낼 SCSystemMessage 메시지 생성
            mju::SCSystemMessage sysMessage;
            sysMessage.set_text(text);
            const string sysMessageString = sysMessage.SerializeAsString();

            // 보낼 데이터 변수 선언
            char* dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

            // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
            short bytesInBigEndian = htons(typeString.length());
            memcpy(dataToSend, &bytesInBigEndian, 2);
            memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

            // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
            bytesInBigEndian = htons(sysMessageString.length());
            memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
            memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

            // 데이터 전송
            ChatServer::CustomSend(clientSock, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());

            delete[] dataToSend;
        }
    }
}

void OnCsJoinRoom(int clientSock, const void* data) {
    int roomId = *((int*)data); // 방 번호

    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    bool isUserInRoom; // 유저가 채팅방에 들어가 있는지
    bool roomFound = false; // 전달된 채팅방번호와 매칭되는 채팅방이 있는지
    string userName;
    string roomTitle;
    string textForOtherUsers; // 대화방에 있는 유저들에게 메시지
    string textForUserGettingIn; // join시도중인 유저에게 메시지
    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);

        isUserInRoom = user->roomThisUserIn != nullptr;
        //유저가 채팅방에 접속해 있지 않은 경우
        if (!isUserInRoom) {
            lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

            userName = user->GetUserName();

            // 입력된 채팅방 번호와 일치하는 방을 찾는다
            for (auto r : ChatServer::Rooms)
                if (r->roomId == roomId) {
                    r->usersInThisRoom.insert(user);
                    user->roomThisUserIn = r;
                    roomFound = true;
                    roomTitle = r->title;
                    break;
                }
        }
    }
    // 입력된 채팅방 번호와 일치하는 방이 없는 경우
    if (!roomFound)
        textForUserGettingIn = "대화방이 존재하지 않습니다."; 
    // 입력된 채팅방 번호와 일치하는 방이 있는 경우
    else {
        //유저가 채팅방에 이미 접속해 있는 경우
        if (isUserInRoom)
            textForUserGettingIn = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";  
        //유저가 채팅방에 접속해 있지 않은 경우 
        else {
            textForOtherUsers = user->GetUserName() + " 님이 입장했습니다.";
            textForUserGettingIn = "방제[" + roomTitle + "] 방에 입장했습니다.";
        }
    }

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

        char* msgToSendForUserGettingIn = nullptr; // 보낼 메시지
        short bytesToSendForUserGettingIn; // 보낼 데이터의 바이트 수

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonStringForUserGettingIn.length());
        bytesToSendForUserGettingIn = jsonStringForUserGettingIn.length() + 2;
        msgToSendForUserGettingIn = new char[bytesToSendForUserGettingIn];
        memcpy(msgToSendForUserGettingIn, &msgBytesInBigEnndian, 2);
        memcpy(msgToSendForUserGettingIn + 2, jsonStringForUserGettingIn.c_str(), jsonStringForUserGettingIn.length());

        // 데이터 전송
        ChatServer::CustomSend(clientSock, msgToSendForUserGettingIn, bytesToSendForUserGettingIn);
        
        delete[] msgToSendForUserGettingIn;

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

            char* msgToSendForOtherUsers = nullptr; // 보낼 메시지
            short bytesToSendForOtherUsers; // 보낼 데이터의 바이트 수

            // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
            msgBytesInBigEnndian = htons(jsonStringForOtherUsers.length());
            bytesToSendForOtherUsers = jsonStringForOtherUsers.length() + 2;
            msgToSendForOtherUsers = new char[bytesToSendForOtherUsers];
            memcpy(msgToSendForOtherUsers, &msgBytesInBigEnndian, 2);
            memcpy(msgToSendForOtherUsers + 2, jsonStringForOtherUsers.c_str(), jsonStringForOtherUsers.length());

            // 데이터 전송
            {
                lock_guard<mutex> usersLock(ChatServer::UsersMutex);
                
                if (user->roomThisUserIn != nullptr) {
                    lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

                    for (auto u : user->roomThisUserIn->usersInThisRoom) 
                        if (u->socketNumber != user->socketNumber) 
                            ChatServer::CustomSend(u->socketNumber, msgToSendForOtherUsers, bytesToSendForOtherUsers);
                }
            }
            delete[] msgToSendForOtherUsers;
        }
    } 
    // protobuf 포맷
    else {
        // 먼저 보낼 Type 메시지 생성
        mju::Type type;
        type.set_type(mju::Type::MessageType::Type_MessageType_SC_SYSTEM_MESSAGE);
        const string typeString = type.SerializeAsString();

        // 뒤 따라 보낼 SCSystemMessage 메시지 생성
        mju::SCSystemMessage sysMessage;
        sysMessage.set_text(textForUserGettingIn);
        string sysMessageString = sysMessage.SerializeAsString();

        // 보낼 데이터 변수 선언
        char* dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

        // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
        short bytesInBigEndian = htons(typeString.length());
        memcpy(dataToSend, &bytesInBigEndian, 2);
        memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

        // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
        bytesInBigEndian = htons(sysMessageString.length());
        memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
        memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

        // 데이터 전송
        ChatServer::CustomSend(clientSock, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());

        delete[] dataToSend;
        dataToSend = nullptr;

        if (!isUserInRoom) {
            sysMessage.set_text(textForOtherUsers);
            sysMessageString = sysMessage.SerializeAsString();

            // 보낼 데이터 변수 선언
            dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

            // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
            bytesInBigEndian = htons(typeString.length());
            memcpy(dataToSend, &bytesInBigEndian, 2);
            memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

            // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
            bytesInBigEndian = htons(sysMessageString.length());
            memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
            memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

            {
                lock_guard<mutex> usersLock(ChatServer::UsersMutex);
                
                if (user->roomThisUserIn != nullptr) {
                    lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

                    for (auto u : user->roomThisUserIn->usersInThisRoom)
                        if (u->socketNumber != user->socketNumber) 
                            ChatServer::CustomSend(u->socketNumber, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());
                }
            }
            delete[] dataToSend;
        }
    }
}

void OnCsLeaveRoom(int clientSock, const void* data) {
    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    bool isAnyoneInRoom = false; // 채팅방에 1명 이상 있는지
    const char* userName = nullptr; // 나간 유저 이름
    string textForUserExiting; // 나간 유저를 위한 메시지
    string textForOtherUsers; // 채팅방 모든 유저를 위한 메시지
    shared_ptr<Room> room = nullptr; // 유저가 나가는 방

    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);
        
        // 유저가 현재 채팅방에 접속 중인 경우
        if (user->roomThisUserIn != nullptr) {
            lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);
            
            // 유저 퇴장 메시지
            textForOtherUsers = user->GetUserName() + " 님이 퇴장했습니다.";
            room = user->roomThisUserIn; // 유저가 현재 속한 방 저장
            textForUserExiting = "방제[" + room->title + "] 대화 방에서 퇴장했습니다.";

            // 유저를 채팅방에서 내쫓음
            room->usersInThisRoom.erase(user);
            user->roomThisUserIn = nullptr;

            // 채팅방에 아무도 없다면 채팅방을 삭제
            if (room->usersInThisRoom.size() > 0)
                isAnyoneInRoom = true;
            else
                ChatServer::Rooms.erase(room);
        }
        // 유저가 채팅방에 접속 중이지 않은 경우
        else 
            textForUserExiting = "현재 대화방에 들어가 있지 않습니다.";
    }

    // json 포맷
    if (ChatServer::IsJson) {        
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
        string jsonStringForUserExiting = bufferForUserExiting.GetString();

        char* msgToSendForUserExiting = nullptr; // 보낼 메시지
        short bytesToSendForUserExiting; // 보낼 데이터의 바이트 수

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonStringForUserExiting.length());
        bytesToSendForUserExiting = jsonStringForUserExiting.length() + 2;
        msgToSendForUserExiting = new char[bytesToSendForUserExiting];
        memcpy(msgToSendForUserExiting, &msgBytesInBigEnndian, 2);
        memcpy(msgToSendForUserExiting + 2, jsonStringForUserExiting.c_str(), jsonStringForUserExiting.length());

        // 데이터 전송
        ChatServer::CustomSend(user->socketNumber, msgToSendForUserExiting, bytesToSendForUserExiting);

        delete[] msgToSendForUserExiting;

        // 채팅방에 유저 1명 이상 존재하는 경우 
        if (isAnyoneInRoom) {
            cout << "asdasdasdasd" << endl;
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
            string jsonStringForOtherUsers = bufferForOtherUsers.GetString();

            char* msgToSendForOtherUsers = nullptr; // 보낼 메시지
            short bytesToSendForOtherUsers; // 보낼 데이터의 바이트 수
            
            // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
            short msgBytesInBigEnndian = htons(jsonStringForOtherUsers.length());
            bytesToSendForOtherUsers = jsonStringForOtherUsers.length() + 2;
            msgToSendForOtherUsers = new char[bytesToSendForOtherUsers];
            memcpy(msgToSendForOtherUsers, &msgBytesInBigEnndian, 2);
            memcpy(msgToSendForOtherUsers + 2, jsonStringForOtherUsers.c_str(), jsonStringForOtherUsers.length());

            // 데이터 전송
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
    }
    // protobuf 포맷
    else {
        // 먼저 보낼 Type 메시지 생성
        mju::Type type;
        type.set_type(mju::Type::MessageType::Type_MessageType_SC_SYSTEM_MESSAGE);
        const string typeString = type.SerializeAsString();

        // 뒤 따라 보낼 SCSystemMessage 메시지 생성
        mju::SCSystemMessage sysMessage;
        sysMessage.set_text(textForUserExiting);
        string sysMessageString = sysMessage.SerializeAsString();

        // 보낼 데이터 변수 선언
        char* dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

        // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
        short bytesInBigEndian = htons(typeString.length());
        memcpy(dataToSend, &bytesInBigEndian, 2);
        memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

        // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
        bytesInBigEndian = htons(sysMessageString.length());
        memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
        memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

        // 데이터 전송
        ChatServer::CustomSend(clientSock, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());

        delete[] dataToSend;
        dataToSend = nullptr;

        if (isAnyoneInRoom) {
            sysMessage.set_text(textForOtherUsers);
            sysMessageString = sysMessage.SerializeAsString();

            // 보낼 데이터 변수 선언
            dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

            // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
            bytesInBigEndian = htons(typeString.length());
            memcpy(dataToSend, &bytesInBigEndian, 2);
            memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

            // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
            bytesInBigEndian = htons(sysMessageString.length());
            memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
            memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

            // 데이터 전송
            {
                lock_guard<mutex> usersLock(ChatServer::UsersMutex);
                {
                    lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);
                    if (room != nullptr) 
                        for (auto u : room->usersInThisRoom) 
                            ChatServer::CustomSend(u->socketNumber, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());
                }
            }
            
            delete[] dataToSend;
        }
    }

}

void OnCsChat(int clientSock, const void* data) {
    string text = (char*)data; // 전달된 채팅 내용

    // 해당 clientSock을 가지는 유저를 찾는다
    shared_ptr<User> user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }

    string userName; // 유저 이름
    bool isUserInRoom = false; // 유저가 채팅방에 있는지
    {
        lock_guard<mutex> usersLock(ChatServer::UsersMutex);
        isUserInRoom = user->roomThisUserIn != nullptr; // 유저의 채팅방 접속 여부를 구함
        userName = user->GetUserName(); // 유저이름을 구함
    }

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

        char* msgToSend = nullptr; // 보낼 메시지
        short bytesToSend; // 보낼 데이터의 바이트 수

        // json 앞에 json의 바이트수 추가하여 보낼 데이터 생성
        short msgBytesInBigEnndian = htons(jsonString.length());
        bytesToSend = jsonString.length() + 2;
        msgToSend = new char[bytesToSend];
        memcpy(msgToSend, &msgBytesInBigEnndian, 2);
        memcpy(msgToSend + 2, jsonString.c_str(), jsonString.length());

        // 데이터 전송
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
    // protobuf 포맷
    else {
        if (isUserInRoom) {
            // 먼저 보낼 Type 메시지 생성
            mju::Type type;
            type.set_type(mju::Type::MessageType::Type_MessageType_SC_CHAT);
            const string typeString = type.SerializeAsString();

            // 뒤 따라 보낼 SCChat 메시지 생성
            mju::SCChat chat;
            chat.set_text(text);
            chat.set_member(userName.c_str());
            const string chatString = chat.SerializeAsString();

            // 보낼 데이터 변수 선언
            char* dataToSend = new char[2 + typeString.length() + 2 + chatString.length()];

            // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
            short bytesInBigEndian = htons(typeString.length());
            memcpy(dataToSend, &bytesInBigEndian, 2);
            memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

            // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
            bytesInBigEndian = htons(chatString.length());
            memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
            memcpy(dataToSend + 2 + typeString.length() + 2, chatString.c_str(), chatString.length());

            // 데이터 전송
            {
                lock_guard<mutex> usersLock(ChatServer::UsersMutex);
                {
                    lock_guard<mutex> roomsLock(ChatServer::RoomsMutex);

                    if (isUserInRoom) {
                        for (auto& u : user->roomThisUserIn->usersInThisRoom)
                            if (u->socketNumber != clientSock)
                                ChatServer::CustomSend(u->socketNumber, dataToSend, 2 + typeString.length() + 2 + chatString.length());
                    }
                }
            }

            delete[] dataToSend;   
        }
        else {
            // 먼저 보낼 Type 메시지 생성
            mju::Type type;
            type.set_type(mju::Type::MessageType::Type_MessageType_SC_SYSTEM_MESSAGE);
            const string typeString = type.SerializeAsString();

            // 뒤 따라 보낼 SCSystemMessage 메시지 생성
            mju::SCSystemMessage sysMessage;
            sysMessage.set_text("현재 대화방에 들어가 있지 않습니다.");
            const string sysMessageString = sysMessage.SerializeAsString();

            // 보낼 데이터 변수 선언
            char* dataToSend = new char[2 + typeString.length() + 2 + sysMessageString.length()];

            // 먼저 보낼 Type 메시지를 먼저 dataToSend에 넣어줌
            short bytesInBigEndian = htons(typeString.length());
            memcpy(dataToSend, &bytesInBigEndian, 2);
            memcpy(dataToSend + 2, typeString.c_str(), typeString.length());

            // 뒤 따라 보낼 SCSystemMessage 메시지를 dataToSend에 넣어줌
            bytesInBigEndian = htons(sysMessageString.length());
            memcpy(dataToSend + 2 + typeString.length(), &bytesInBigEndian, 2);
            memcpy(dataToSend + 2 + typeString.length() + 2, sysMessageString.c_str(), sysMessageString.length());

            // 데이터 전송
            ChatServer::CustomSend(clientSock, dataToSend, 2 + typeString.length() + 2 + sysMessageString.length());

            delete[] dataToSend;   
        }
    }
}

#include <sys/ioctl.h>

void OnCsShutDown(int clientSock, const void* data) {
    send(clientSock, "test", 4, MSG_OOB);
    ChatServer::DestroySigleton();
}