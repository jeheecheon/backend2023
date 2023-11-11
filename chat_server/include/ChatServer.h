// ChatServer.h
#pragma once

#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <arpa/inet.h>
#include <sys/socket.h>

#include <set>
#include <thread>
#include <unordered_set>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <condition_variable>

#include "SmallWork.h"
#include "../protobuf/message.pb.h"
#include "../include/MessageHandlers.h"
#include "Room.h"

using namespace std;

class ChatServer {
private:
    int _serverSocket; // 서버 소켓 번호
    int _port; // 서버 소켓 port 번호
    bool _isBinded; // BindServerSocket 가 호출되어 성공되었는지

    set<int> _clients; // 연결된 클라이언트들

    set<unique_ptr<thread>> workers; // Worker Threads
    
    unordered_set<int> _willClose; // 종료할 클라이언트 소켓 set

    static ChatServer* _instance; // Singleton 인스턴

public:
    static bool isJson; // JSON 포맷 = true, Protobuf 포맷 = false
    
    // ----------------------------------------------
    // 현재 존재하는 방들 
    static set<shared_ptr<Room>> rooms; // 방 목록
    static mutex roomsMutex;
    // ----------------------------------------------


    // ----------------------------------------------
    // 연결된 유저들  
    static set<shared_ptr<User>> users; 
    static mutex usersMutex;
    // ----------------------------------------------


    // ----------------------------------------------
    // 읽기 이벤트가 발생한 클라이언트 queue
    static queue<SmallWork> messagesQueue;
    static mutex messagesQueueMutex;
    static condition_variable messagesQueueEdited;
    // ----------------------------------------------


    // ----------------------------------------------
    // messagesQueue에서 아직 연산을 기다리는 Sockets들을 담는 set
    static unordered_set<int> socketsOnQueue;
    static mutex socketsOnQueueMutex;
    // ----------------------------------------------


    // ----------------------------------------------
    // Worker Thread의 MessageHandlers
    typedef void (*MessageHandler)(int clientSock, const void* data);
    static unordered_map<string, MessageHandler> jsonHandlers;
    static unordered_map<mju::Type::MessageType, MessageHandler> protobufHandlers;
    // ----------------------------------------------

private:
    ChatServer();

    // recv 함수를 대신 호출한다. 예외처리를 대신 처리함
    // clientSock - 값을 확인할 클라이언트 소켓 번호
    // buf - 값을 담을 메모리 영역
    // size - buf의 크기
    // numRecv - 전달받은 값을 저장할 변수
    // 반환값은 성공 여부 true or false
    bool CustomReceive(int clientSock, void* buf, size_t size, int& numRecv);

public:
    ~ChatServer();

    // setter of IsJson
    void SetIsJson(bool b); 

    // 서버 소캣 생성
    // 반환값은 성공 여부 true or false
    bool OpenServerSocket();

    // 전달된 포트번호와 아이피로 서버소켓을 바인드
    // 반환값은 성공 여부 true or false
    bool BindServerSocket(int port, in_addr_t addr);

    // 채팅 서버 시작
    // 반환값은 성공 여부 true or false
    bool Start(int numOfWorkerThreads);

    // 핸들러들을 설정
    // ConfigureHandlers(true); // JSON 핸들러 설정
    // ConfigureHandlers(false); // Protobuf 핸들러 설정
    void ConfigureMsgHandlers();

    // 채팅 서버 종료.
    void TerminateServer();
public:
    //
    static void HandleSmallWork(); // Worker Thread의 Entry Point

    // 전달된 소켓 번호와 일치하는 유저를 찾는다.
    // 찾은 결과는 user 변수에
    // 반환값은 성공 여부 true or false
    // ! 주의: usersMutex 를 사용 중
    static bool FindUserBySocketNum(int sock, shared_ptr<User>& user);

    static void CustomSend(int sock, void* dataToSend, int bytesToSend);

    static ChatServer& CreateSingleton();

private:
    void Cleanup();
};

#endif  // CHAT_SERVER_H
