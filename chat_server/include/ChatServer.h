// ChatServer.h
#pragma once

#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <arpa/inet.h>
#include <sys/socket.h>

#include <set>
#include <thread>

#include "../include/MessageHandlers.h"
#include "GlobalVariables.h"
#include "Client.h"

using namespace std;

class ChatServer {
private:
    int _serverSocket; // 서버 소켓 번호
    int _port; // 서버 소켓 port 번호
    bool _isBinded; // BindServerSocket 가 호출되어 성공되었는지

    std::set<std::unique_ptr<std::thread>> workers; // Worker Threads
    unordered_set<int> _willClose; // 종료할 클라이언트 소켓 set

public:
    static unique_ptr<ChatServer> instance;

    static set<Client> clients; // 연결된 클라이언트들 
    static vector<Client> rooms; // 방 목록

private:
    ChatServer();

public:
    ~ChatServer();


    // 서버 소캣 생성
    // 반환값은 성공 여부 true or false
    bool OpenServerSocket();

    // 전달된 포트번호와 아이피로 서버소켓을 바인드
    // 반환값은 성공 여부 true or false
    bool BindServerSocket(int port, in_addr_t addr);

    // 채팅 서버 시작
    // 반환값은 성공 여부 true or false
    bool Start(int numOfWorkerThreads);

    // recv 함수를 대신 호출한다. 예외처리를 대신 처리함
    // clientSock - 값을 확인할 클라이언트 소켓 번호
    // buf - 값을 담을 메모리 영역
    // size - buf의 크기
    // numRecv - 전달받은 값을 저장할 변수
    // 반환값은 성공 여부 true or false
    bool CustomReceive(int clientSock, void* buf, size_t size, int& numRecv);

    // 핸들러들을 설정
    // ConfigureHandlers(true); // JSON 핸들러 설정
    // ConfigureHandlers(false); // Protobuf 핸들러 설정
    void ConfigureMsgHandlers(bool IsJson);

public:
    //
    static void HandleSmallWork(); // Worker Thread의 Entry Point

    static ChatServer& CreateInstance();

private:
    void Cleanup();
};

#endif  // CHAT_SERVER_H
