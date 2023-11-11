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

    User user;
    if (!ChatServer::FindUserBySocketNum(clientSock, user)) {
        cerr << "Failed to find the matching user with sock num....!!!!!";
        return;
    }
    {
        lock_guard<mutex> usersLock(ChatServer::usersMutex);
        user.userName = name;
    }

    if (ChatServer::isJson) {
        // Create a RapidJSON document
        rapidjson::Document doc;
        doc.SetObject();

        // Add key-value pairs to the document
        rapidjson::Value typeValue;
        typeValue.SetString("SCSystemMessage", doc.GetAllocator());
        doc.AddMember("type", typeValue, doc.GetAllocator());

        rapidjson::Value textValue;
        textValue.SetString("asdasd", doc.GetAllocator());
        doc.AddMember("text", textValue, doc.GetAllocator());

        // Convert the document to a JSON string
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        string jsonString = buffer.GetString();
        
        short bytesToSend = jsonString.length() + 2;

        char* data = new char[bytesToSend];
        short bytesInBigEnndian = htons(bytesToSend);

        memcpy(data, &bytesInBigEnndian, 2);
        memcpy(data + 2, jsonString.c_str(), jsonString.length());
        
        ChatServer::CustomSend(clientSock, (void*)data, sizeof(short));
        ChatServer::CustomSend(clientSock, (void*)(data + 2), bytesToSend - 2);


        delete[] data;



    } else {

    }
}

void OnCsRooms(int clientSock, const void* data) {
    cout << "OnCsRooms" << endl;
}

void OnCsCreateRoom(int clientSock, const void* data) {


    cout << "OnCsCreateRoom" << (char*)data << endl;
}

void OnCsJoinRoom(int clientSock, const void* data) {
    // 방번호는 클라이언트에서 확인을 거침
    // int
    cout << "OnCsJoinRoom" << *((int*)data) << endl;
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