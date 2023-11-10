#include <iostream>
#include <MessageHandlers.h>

using namespace std;

void OnCsName(const char* data) {
    cout << "OnCsName" << endl;
}

void OnCsRooms(const char* data) {
    cout << "OnCsRooms" << endl;
}

void OnCsCreateRoom(const char* data) {
    cout << "OnCsCreateRoom" << endl;
}

void OnCsJoinRoom(const char* data) {
    cout << "OnCsJoinRoom" << endl;
}

void OnCsLeaveRoom(const char* data) {
    cout << "OnCsLeaveRoom" << endl;
}

void OnCsChat(const char* data) {
    cout << "OnCsChat" << endl;
}

void OnCsShutDown(const char* data) {
    cout << "OnCsShutDown" << endl;
}