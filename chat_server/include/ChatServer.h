/**
 * @file ChatServer.h
 * @brief 채팅 서버 클래스 헤더 파일
 */

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
#include "message.pb.h"
#include "../include/MessageHandlers.h"
#include "Room.h"

using namespace std;

/**
 * @class ChatServer
 * @brief 채팅 서버 클래스
 *
 * 채팅 서버의 주요 기능과 멤버 변수를 정의한 클래스입니다.
*/
class ChatServer {
private:
    int _serverSocket; // 서버 소켓 번호
    int _port; // 서버 port 번호
    bool _isOpened; // 서버소켓 오픈 여부
    bool _isBinded; // 소켓 바인드 여부
    unordered_set<int> _clients; // 서버와 연결된 클라이언트
    vector<thread> _workers; ///< Worker Threads
    unordered_set<int> _willClose; // 종료 예정인 클라이언트 소켓 set

    static ChatServer* _Instance; // Singleton 인스턴스
public:
    static bool IsJson; // 서버 입출력 JSON 포맷 = true, Protobuf 포맷 = false

    static volatile atomic<bool> TerminateSignal; // 서버 종료 시그널

    static volatile atomic<bool> IsRunning; // 서버가 실행중인지

    // ----------------------------------------------
    // 채팅방 
    static unordered_set<shared_ptr<Room>> Rooms;
    static mutex RoomsMutex;
    // ----------------------------------------------

    // ----------------------------------------------
    // 유저
    static unordered_set<shared_ptr<User>> Users; 
    static mutex UsersMutex;
    // ----------------------------------------------

    // ----------------------------------------------
    // Worker Threads 의 작업 Queue
    static queue<SmallWork> SmallWorkQueue;
    static mutex SmallWorkQueueMutex;
    static condition_variable SmallWorkAdded;
    // ----------------------------------------------

    // ----------------------------------------------
    // 처리 되지 않은 메시지가 남아있는 클아이언트 Queue
    static unordered_set<int> SocketsOnQueue;
    static mutex SocketsOnQueueMutex;
    static condition_variable SocketsOnQueueAddable;
    // ----------------------------------------------

    // ----------------------------------------------
    // MessageHandlers
    typedef void (*MessageHandler)(int clientSock, const void* data);
    static unordered_map<string, MessageHandler> JsonHandlers;
    static unordered_map<mju::Type::MessageType, MessageHandler> ProtobufHandlers;
    // ----------------------------------------------

private:
    /**
     * @brief ChatServer 생성자 (private로 선언하여 Singleton 패턴 구현)
    */
    ChatServer();

    /**
     * @brief recv 함수를 대신 호출하고 예외를 처리하는 함수
     *
     * @param clientSock 확인할 클라이언트 소켓 번호
     * @param buf 값을 담을 메모리 영역
     * @param size buf의 크기
     * @param numRecv 전달받은 값을 저장할 변수
     * @return 성공 여부 true or false
    */
    bool CustomReceive(int clientSock, void* buf, size_t size, int& numRecv);

    /**
     * @brief 채팅 서버 자원 정리하는 함수
    */
public:
    
    void TerminateServer();

    /**
     * @brief ChatServer 소멸자
    */
    ~ChatServer();

    /**
     * @brief IsJson Setter
     *
     * @param b JSON 포맷 여부
    */
    void SetIsJson(bool isJson); 

    /**
     * @brief 서버 소켓 생성
     *
     * @return 성공 여부
    */
    bool OpenServerSocket();

    /**
     * @brief 전달된 포트번호와 아이피로 서버소켓을 바인드
     *
     * @param port 포트 번호
     * @param addr IP 주소
     * @return 성공 여부
    */
    bool BindServerSocket(int port, in_addr_t addr);

    /**
     * @brief 채팅 서버 시작
     *
     * @param numOfWorkerThreads 작업 스레드 수
     * @return 성공 여부
    */
    bool Start(int numOfWorkerThreads);

    /**
     * @brief 메시지 핸들러들 설정
    */
    void ConfigureMsgHandlers();

    /**
     * @brief 채팅 서버 종료
    */
    void Stop();

public:
    /**
     * @brief ChatServer의 Singletone 인스턴스가 있는지 확인하는 함수
     *
     * @return Singleton Instance의 존재 유무를 bool로 반환한다.
    */
    static bool HasInstance();

    /**
     * @brief Worker Thread의 Entry Point
    */
    static void HandleSmallWork();

    /**
     * @brief 소켓 번호를 기반으로 사용자를 찾는 함수
     *
     * @param sock 소켓 번호
     * @param user 찾은 사용자에 대한 shared_ptr, 찾지 못한 경우 nullptr
     * @return 찾은 사용자의 여부 (true: 찾음, false: 찾지 못함)
    */
    static bool FindUserBySocketNum(int sock, shared_ptr<User>& user); // 주의: usersMutex 사용 중

    /**
     * @brief TCP 포트로 데이터를 전송하는 함수
     *
     * @param sock 소켓 번호
     * @param dataToSend 전송할 데이터
     * @param bytesToSend 전송할 바이트 수
    */
    static void CustomSend(int sock, void* dataToSend, int bytesToSend);

    /**
     * @brief ChatServer의 싱글톤 인스턴스를 생성하는 함수
     *
     * @return ChatServer 싱글톤 인스턴스에 대한 참조
    */
    static ChatServer& CreateSingleton();
};

#endif  // CHAT_SERVER_H
