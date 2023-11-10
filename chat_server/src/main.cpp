#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <iostream>
#include <queue>
#include <thread>

#include "include/User.h"
#include "include/Room.h"
#include "include/Work.h"
#include "include/MessageHandlers.h"
#include "include/GlobalVariables.h"
#include "include/SmallWorkHandler.h"

#include "protobuf/message.pb.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace std;
using namespace rapidjson;

bool CustomReceive(int clientSock, void* buf, size_t size, int& numRecv) {
    numRecv = recv(clientSock, buf, size, 0);

    if (numRecv == 0) {
        cout << "Socket closed: " << clientSock << endl;
        {
            willClose.insert(clientSock);
            return false;
        }
    } else if (numRecv < 0) {
        cerr << "recv() failed: " << strerror(errno)
             << ", clientSock: " << clientSock << endl;
        {
            willClose.insert(clientSock);
            return false;
        }
    }
    cout << "Received: " << numRecv << " bytes, clientSock: " << clientSock
         << endl;

    return true;
}

int main(int argc, char* argv[]) {
    // option 입력 방법 출력
    cout << endl <<
"--format: <json|protobuf>: 메시지 포맷\n\
    (default: 'json')\n\
--Port: 서버 Port 번호\n\
    (an integer)\n\
--workers: 작업 쓰레드 숫자\n\
    (default: '2')\n\
    (an integer)\n";
    
    int numOfWorkerThreads = 2;

    // main 으로 전달된 arguments handling
    bool isInputError = false;
    for (int i = 1; i < argc; ++i) {
        // "--"로 시작하는 옵션인 경우
        if ((strlen(argv[i]) >= 3) && (argv[i][0] == '-') &&
            (argv[i][1] == '-')) {
            if (i + 1 < argc) {  // 입력된 옵션에 대응되는 value가 있는 경우
                // port번호 옵션 handling
                if (strcmp(argv[i], "--port") == 0) {
                    try {
                        Port = stoi(argv[i + 1]);
                    } catch (const exception& e) {
                        isInputError = true;
                        break; 
                    }
                    ++i;
                }
                // worker thread number옵션 handling
                else if (strcmp(argv[i], "--workers") == 0) {
                    try {
                        numOfWorkerThreads = stoi(argv[i + 1]);
                    } catch (const exception& e) {
                        isInputError = true;
                        break;
                    }
                    ++i;
                }
                else if (strcmp(argv[i], "--format") == 0) {
                    if (strcmp(argv[i + 1], "json") == 0) 
                        IsJson = true;
                    else if (strcmp(argv[i + 1], "protobuf") == 0) {
                        IsJson = false;
                    } else {
                        isInputError = true;
                        break;
                    }
                    ++i;
                }
                // 지원하지 않는 옵션이 입력된 경우 입력오류로 처리
                else {
                    isInputError = true;
                    break;
                }
            } else {  // 입력된 옵션에 대응되는 value가 없는 경우 입력오류로
                      // 처리
                isInputError = true;
                break;
            }
        } else {  // "--"로 시작하는 스트링이 아닌 경우 입력오류로 처리
            isInputError = true;
            break;
        }
    }
    // 입력오류인 경우
    if (isInputError || Port == -1) {
        cerr << "FATAL Flags parsing error: flag --Port=None: Flag --Port must have a value other than None." << endl;
        return -1;
    }

    // Open 서버 소켓
    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Port를 재사용할 수 있도록 옵션 설정
    int on = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        cerr << "setsockopt() failed: " << strerror(errno) << endl;
        return 1;
    }
    if (setsockopt(serverSock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0) 
        cerr << "Failed to turn off the Nagle’s algorithm...";
    

    // 특정 TCP Port 에 bind
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(Port);
    if (bind(serverSock, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        cerr << "bind() failed: " << strerror(errno) << endl;
        return -1;
    }

    // 서버 소켓을 passive socket 으로 변경
    if (listen(serverSock, 10) < 0) {
        cerr << "listen() failed: " << strerror(errno) << endl;
        return -1;
    }

    cout << "Port 번호 " << Port << "에서 서버 동작 중" << endl;

    if (IsJson) {
        // JSON handler 등록
        jsonHandlers["CSName"] = OnCsName;
        jsonHandlers["CSRooms"] = OnCsRooms;
        jsonHandlers["CSCreateRoom"] = OnCsCreateRoom;
        jsonHandlers["CSJoinRoom"] = OnCsJoinRoom;
        jsonHandlers["CSLeaveRoom"] = OnCsLeaveRoom;
        jsonHandlers["CSChat"] = OnCsChat;
        jsonHandlers["CSShutdown"] = OnCsShutDown;
    } else {
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_NAME] = OnCsName;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_ROOMS] = OnCsRooms;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CREATE_ROOM] = OnCsCreateRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_JOIN_ROOM] = OnCsJoinRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_LEAVE_ROOM] = OnCsLeaveRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CHAT] = OnCsChat;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_SHUTDOWN] = OnCsShutDown;
    }

    set<unique_ptr<thread>> workers;
    for (int i = 0; i < numOfWorkerThreads; ++i)
        workers.insert(make_unique<thread>(HandleSmallWork));

    set<int> clientSocks;

    while (true) {
        fd_set rset;

        FD_ZERO(&rset);            
        FD_SET(serverSock, &rset); 

        int maxFd = serverSock;

        for (int clientSock : clientSocks) {
            FD_SET(clientSock, &rset);

            if (clientSock > maxFd) 
                maxFd = clientSock;
        }
        

        int numReady = select(maxFd + 1, &rset, NULL, NULL, NULL);
        if (numReady < 0) {
            cerr << "select() failed: " << strerror(errno) << endl;
            continue;
        } else if (numReady == 0)
            continue;

        if (FD_ISSET(serverSock, &rset)) {
            memset(&sin, 0, sizeof(sin));
            unsigned int sin_len = sizeof(sin);

            int clientSock = accept(serverSock, (struct sockaddr*)&sin, &sin_len);

            if (clientSock < 0)
                cerr << "accept() failed: " << strerror(errno) << endl;
            else {
                clientSocks.insert(clientSock);

                cout << "새로운 클라이언트 접속 [(" << inet_ntoa(sin.sin_addr) << ", " <<
                ntohs(sin.sin_port) << "):None]" << endl;
            }
        }
        

        // 클라이언트 소켓에 읽기 이벤트가 발생한 게 있는지
        for (int clientSock : clientSocks) {
            if (!FD_ISSET(clientSock, &rset))
                continue;
            
            char data[65565];
            int numRecv;
            short bytesToReceive;

            memset(data, 0, sizeof(data));

            // 데이터의 크기가 담긴 첫 두 바이트를 읽어옴
            if (!CustomReceive(clientSock, &bytesToReceive, sizeof(bytesToReceive), numRecv))
                continue;

            // 나머지 데이터를 모두 읽어옴
            if (!CustomReceive(clientSock, data, bytesToReceive, numRecv))
                continue;

            {
                lock_guard<mutex> socketsOnQueueLock(socketsOnQueueMutex);
                // 아직 해당 소켓의 메시지가 처리되지 못했는지 확인
                if (socketsOnQueue.find(clientSock) != socketsOnQueue.end())
                    continue;
            }
            {
                lock_guard<mutex> messagesQueueLock(messagesQueueMutex);

                socketsOnQueue.insert(clientSock);
                messagesQueue.push({ data, clientSock});
                messagesQueueEdited.notify_one();
                cout << "pushed" << endl;
            }
        }

        for (int clientSock : willClose) {
            cout << "Closed: " << clientSock << endl;
            close(clientSock);
            clientSocks.erase(clientSock);
        }
        willClose.clear();
    }

    cout << "Main thread 종료 중" << endl;

    // Worker threads 정리
    for (auto& worker : workers)
        if (worker->joinable()) {
            cout << "작업 쓰레드 join() 시작" << endl;
            worker->join();
            cout << "작업 쓰레드 join() 완료" << endl;
        }

    // 소켓 정리
    for (int clientSock : clientSocks) 
        close(clientSock);
    close(serverSock);

    return 0;
}