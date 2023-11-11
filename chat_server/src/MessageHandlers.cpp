#include "../include/MessageHandlers.h"
#include "../include/ChatServer.h"

#include <iostream>

using namespace std;

void OnCsName(const void* data) {

    cout << "OnCsName " << (char*)data << endl;
    
}

void OnCsRooms(const void* data) {
    cout << "OnCsRooms" << endl;
}

void OnCsCreateRoom(const void* data) {


    cout << "OnCsCreateRoom" << (char*)data << endl;
}

void OnCsJoinRoom(const void* data) {
    // 방번호는 클라이언트에서 확인을 거침
    // int
    cout << "OnCsJoinRoom" << *((int*)data) << endl;
}

void OnCsLeaveRoom(const void* data) {
    cout << "OnCsLeaveRoom" << endl;
}

void OnCsChat(const void* data) {


    cout << "OnCsChat" << (char*)data << endl;
}

void OnCsShutDown(const void* data) {
    cout << "OnCsShutDown" << endl;
}