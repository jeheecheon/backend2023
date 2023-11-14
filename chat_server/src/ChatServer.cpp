
#include "../include/ChatServer.h"

#include <netinet/tcp.h>
#include <sys/select.h>
#include <unistd.h>
#include <algorithm>

#include "../include/SmallWork.h"
#include "../include/rapidjson/document.h"
#include "../include/rapidjson/stringbuffer.h"
#include "../include/rapidjson/writer.h"

using namespace rapidjson;
using namespace std;

// 정적 맴버 초기화
ChatServer* ChatServer::_Instance = nullptr;
bool ChatServer::IsJson = true;
volatile atomic<bool> ChatServer::TerminateSignal;
volatile atomic<bool> ChatServer::IsRunning;
unordered_set<shared_ptr<Room>> ChatServer::Rooms;
mutex ChatServer::RoomsMutex;
unordered_set<shared_ptr<User>> ChatServer::Users;
mutex ChatServer::UsersMutex;
queue<SmallWork> ChatServer::SmallWorkQueue;
mutex ChatServer::SmallWorkQueueMutex;
condition_variable ChatServer::SmallWorkAdded;
unordered_set<int> ChatServer::SocketsOnQueue;
condition_variable ChatServer::SocketsOnQueueAddable;
mutex ChatServer::SocketsOnQueueMutex;
unordered_map<string, ChatServer::MessageHandler> ChatServer::JsonHandlers;
unordered_map<mju::Type::MessageType, ChatServer::MessageHandler> ChatServer::ProtobufHandlers;

void ChatServer::SetIsJson(bool isJson) {
    IsJson = isJson;
}

bool ChatServer::OpenServerSocket() {
    if (_isOpened) {
        cerr << "서버 소켓 이미 오픈됨" << endl;
        return false;
    }

    _serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // TCP 소켓 개방

    // SO_REUSEADDR 옵션을 켠다
    int on = 1; 
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        cerr << "setsockopt() failed: " << strerror(errno) << endl;
        return false;
    }
    // TCP_NODELAY 옵션을 켠다
    if (setsockopt(_serverSocket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) <
        0) {
        cerr << "Failed to turn off the Nagle’s algorithm...";
        return false;
    }
    return _isOpened = true;
}

bool ChatServer::BindServerSocket(int port, in_addr_t addr) {
    if (_isBinded) {
        cerr << "서버 소켓 이미 바인드됨" << endl;
        return false;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = addr;
    sin.sin_port = htons(port);
    if (bind(_serverSocket, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        cerr << "bind() failed: " << strerror(errno) << endl;
        return false;
    }

    _port = port;
    return _isBinded = true;
}

bool ChatServer::Start(int numOfWorkerThreads) {
    if (IsRunning.load() || !_isOpened || !_isBinded) {
        cerr << "채팅 서버 실행 실패..." << endl;
        return false;
    }

    struct sockaddr_in sin;

    // 서버 소켓을 passive socket 으로 변경
    if (listen(_serverSocket, 10) < 0) {
        cerr << "listen() failed: " << strerror(errno) << endl;
        IsRunning.store(false);
        return false;
    }

    cout << "Port 번호 " << _port << "에서 서버 동작 중" << endl;

    ConfigureMsgHandlers(); // 메시지 핸들러 등록

    // Worker Threads 생성
    for (int i = 0; i < numOfWorkerThreads; ++i)
        _workers.push_back(thread(HandleSmallWork));

    IsRunning.store(true); // 서버 실행 중임을 뜻하는 Flag를 true로 저장

    // 종료 시그널 오기 전까지 서버 실행
    while (TerminateSignal.load() == false) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(_serverSocket, &rset);

        // 소켓들을 fd_set에 등록
        int maxFd = _serverSocket;
        for (auto& client : _clients) {
            FD_SET(client, &rset);
            // 소켓 번호가 가장 큰 것을 찾는다
            if (client > maxFd) 
                maxFd = client;
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100 milliseconds
        
        // 읽기 이벤트를 기다림
        int numReady = select(maxFd + 1, &rset, NULL, NULL, &timeout);
        if (numReady < 0) {
            cerr << "select() failed: " << strerror(errno) << endl;
            continue;
        } else if (numReady == 0)
            continue;

        // 서버 소켓에 읽기 이벤트가 발생한 경우
        if (FD_ISSET(_serverSocket, &rset)) {
            memset(&sin, 0, sizeof(sin));
            unsigned int sin_len = sizeof(sin);

            // 클라이언트 소켓을 입력받음
            int clientSock = accept(_serverSocket, (struct sockaddr*)&sin, &sin_len);

            if (clientSock < 0)
                cerr << "accept() failed: " << strerror(errno) << endl;
            else {
                // 유저 정보를 User 객체에 저장
                shared_ptr<User> user = make_shared<User>();
                user->ipAddress = string(inet_ntoa(sin.sin_addr));
                user->port = sin.sin_port;
                user->socketNumber = clientSock;
                {
                    lock_guard<mutex> usersLock(UsersMutex);
                    Users.insert(user); // 유저 객체를 Users에 저장. 유저의 채팅방과 이름 관리에 사용함
                    cout << "새로운 클라이언트 접속 " << user->PortAndIpAndNameToString() << endl;
                }

                // 소켓번호를 _clients에 저장. 읽기 이벤트를 받을 때 사용함
                _clients.insert(clientSock);
            }
        }

        // 클라이언트 소켓에 읽기 이벤트가 발생한 게 있는지
        for (auto& client : _clients) {
            if (!FD_ISSET(client, &rset)) 
                continue;

            char data[65565];
            int numRecv;
            short bytesToReceive;

            memset(data, 0, sizeof(data));

            // 데이터의 크기가 담긴 첫 두 바이트를 읽어옴
            if (!CustomReceive(client, &bytesToReceive, sizeof(bytesToReceive), numRecv))
                continue;

            if (numRecv != 2) {
                cerr << "첫 두 바이트에는 항상 뒤따라오는 메시지의 바이트수가 들어있어야 함" << endl;
                continue;
            }

            bytesToReceive = ntohs(bytesToReceive);

            // 나머지 메시지 데이터를 모두 읽어옴
            if (!CustomReceive(client, data, bytesToReceive, numRecv))
                continue;
            if (numRecv != bytesToReceive) {
                cerr << "메시지 일부 손실되었음..." << endl;
                continue;
            }

            SmallWork work;
            work.dataOwner = client;

            // Protobuf 포맷
            if (!IsJson) {
                shared_ptr<mju::Type> type = make_shared<mju::Type>();
                type->ParseFromArray((const void*)data, numRecv);
                work.msgTypeForProtobuf = type->type();

                // 데이터의 크기가 담긴 첫 두 바이트를 읽어옴
                if (!CustomReceive(client, &bytesToReceive, sizeof(bytesToReceive), numRecv))
                    continue;

                if (numRecv != 2) {
                    cerr << "첫 두 바이트에는 항상 뒤따라오는 메시지의 바이트수가 들어있어야 함" << endl;
                    continue;
                }

                bytesToReceive = ntohs(bytesToReceive);

                // 나머지 메시지 데이터를 모두 읽어옴
                if (!CustomReceive(client, data, bytesToReceive, numRecv))
                    continue;
                if (numRecv != bytesToReceive) {
                    cerr << "메시지 일부 손실되었음..." << endl;
                    continue;
                }
                work.bytesOfDataForProtobuf = numRecv;
            }

            work.dataToParse = data;

            {
                unique_lock<mutex> socketOnQueueLock(SocketsOnQueueMutex);

                // 클라이언트 소켓의 과거 메시지가 아직 처리 중인지 확인
                while (SocketsOnQueue.find(client) != SocketsOnQueue.end()) 
                    SocketsOnQueueAddable.wait(socketOnQueueLock); // 과거 메시지의 처리 완료를 기다림

                // 클라이언트의 소켓번호를 다시 추가하여 해당 소켓의 새 메시지가 다시 메시지 처리 중임을 알림
                SocketsOnQueue.insert(client); //
            }
            
            {
                lock_guard<mutex> smallWorkQueueLock(SmallWorkQueueMutex);

                // 메시지를 작업 큐에 삽입 및 worker를 깨움
                SmallWorkQueue.push(work);
                SmallWorkAdded.notify_one();
            }
        }

        // 닫힌 소켓 정리
        for (int clientSock : _willClose) {
            // 유저 소켓을 닫음
            close(clientSock);

            // 유저 소켓을 읽기이벤트 목록 자료구조에서 제거
            _clients.erase(clientSock);

            shared_ptr<User> userFound;

            // 해당 유저의 유저데이터를 찾는다
            if (!FindUserBySocketNum(clientSock, userFound)) 
                cerr << "User 정보 없음" << endl;
                
            // 찾은 유저를 방에서 내쫓고 유저 정보를 제거
            else {
                lock_guard<mutex> usersLock(UsersMutex);
                {
                    lock_guard<mutex> roomsLock(RoomsMutex);
                    
                    shared_ptr<Room> room = userFound->roomThisUserIn; // 유저가 속한 방
                    room->usersInThisRoom.erase(userFound); // 유저를 내쫓음
                    
                    // 방에 아무도 없으면 방을 폭파시킴
                    if (room->usersInThisRoom.size() == 0)
                        Rooms.erase(room);
                        
                    // 유저 정보를 삭제
                    Users.erase(userFound);
                }
            }

            cout << "Closed: " << clientSock << endl;
        }
        _willClose.clear();
    }

    cout << "Main thread 종료 중" << endl;
    TerminateServer();
    return true;
}

void ChatServer::ConfigureMsgHandlers() {
    if (IsJson) {
        // JSON handler 등록
        JsonHandlers["CSName"] = OnCsName;
        JsonHandlers["CSRooms"] = OnCsRooms;
        JsonHandlers["CSCreateRoom"] = OnCsCreateRoom;
        JsonHandlers["CSJoinRoom"] = OnCsJoinRoom;
        JsonHandlers["CSLeaveRoom"] = OnCsLeaveRoom;
        JsonHandlers["CSChat"] = OnCsChat;
        JsonHandlers["CSShutdown"] = OnCsShutDown;
    } else {
        // Protobuf handler 등록
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_NAME] = OnCsName;
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_ROOMS] = OnCsRooms;
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CREATE_ROOM] = OnCsCreateRoom;
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_JOIN_ROOM] = OnCsJoinRoom;
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_LEAVE_ROOM] = OnCsLeaveRoom;
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CHAT] = OnCsChat;
        ProtobufHandlers[mju::Type::MessageType::Type_MessageType_CS_SHUTDOWN] = OnCsShutDown;
    }
}

void ChatServer::Stop() {
    if (IsRunning)
        TerminateServer();
    else
        TerminateSignal.store(true);
}

void ChatServer::TerminateServer() {
    TerminateSignal.store(true);
    delete _Instance;
}

bool ChatServer::HasInstance() {
    return _Instance != nullptr;
}


void ChatServer::HandleSmallWork() {
    cout << "메시지 작업 쓰레드 #" << this_thread::get_id() << " 생성" << endl;

    // 종료 시그널을 받을 때까지 반복
    while (!TerminateSignal.load()) {
        SmallWork work;
        {
            unique_lock<mutex> smallWorkQueueLock(SmallWorkQueueMutex);

            // 할일이 올때까지 기다림
            while (TerminateSignal.load() == false && SmallWorkQueue.empty())
                SmallWorkAdded.wait_for(smallWorkQueueLock, chrono::milliseconds(100));

            // 할일을 꺼냄
            work = SmallWorkQueue.front();
            SmallWorkQueue.pop();
        }
        // 종료 시그널이 받은 경우 반복문 탈출
        if (TerminateSignal.load())
            break;

        // Json 포맷으로 설정 되어있는 경우
        if (IsJson) {
            // Json 데이터 Parse 시도
            Document jsonDoc;
            jsonDoc.Parse(work.dataToParse);
            if (jsonDoc.HasParseError()) {
                cerr << "Failed to parse JSON data." << endl;
            }

            // Parsing한 데이터 출력
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            jsonDoc.Accept(writer);
            cout << "JSON Data: " << buffer.GetString() << endl;

            // Json 데이터 Type 멤버 값 확인
            if (jsonDoc.HasMember("type") && jsonDoc["type"].IsString()) {
                // Type 멤버 값 추출
                const string typeValue = jsonDoc["type"].GetString();

                try {
                    // CSName 메시지
                    if (typeValue == "CSName") {
                        if (jsonDoc.HasMember("name") && jsonDoc["name"].IsString()) {
                            string name = jsonDoc["name"].GetString();
                            if (!name.empty()) {
                                JsonHandlers[typeValue](work.dataOwner, (const void*)name.c_str());
                            }
                            else throw runtime_error("Empty name...");
                        }
                        else throw runtime_error("Empty name...");
                    } 
                    // CSRooms 메시지
                    else if (typeValue == "CSRooms") {
                        JsonHandlers[typeValue](work.dataOwner, nullptr);
                    } 
                    // CSCreateRoom 메시지
                    else if (typeValue == "CSCreateRoom") {
                        if (jsonDoc.HasMember("title") && jsonDoc["title"].IsString()) {
                            string title = jsonDoc["title"].GetString();
                            if (!title.empty())
                                JsonHandlers[typeValue](work.dataOwner, (const void*)title.c_str());
                            else throw runtime_error("Empty title...");
                        }
                        else throw runtime_error("Empty title...");
                    } 
                    // CSJoinRoom 메시지
                    else if (typeValue == "CSJoinRoom") {
                        if (jsonDoc.HasMember("roomId") && jsonDoc["roomId"].IsInt()) {
                            int roomId = jsonDoc["roomId"].GetInt();
                            JsonHandlers[typeValue](work.dataOwner, (const void*)&roomId);
                        }
                        else throw runtime_error("No roomId provided or failed to parse JSON");
                    } 
                    // CSLeaveRoom 메시지
                    else if (typeValue == "CSLeaveRoom") {
                        JsonHandlers[typeValue](work.dataOwner, nullptr);
                    } 
                    // CSChat 메시지
                    else if (typeValue == "CSChat") {
                        if (jsonDoc.HasMember("text") && jsonDoc["text"].IsString()) {
                            string text = jsonDoc["text"].GetString();
                            if (!text.empty())
                                JsonHandlers[typeValue](work.dataOwner, (const void*)text.c_str());
                            else throw runtime_error("Empty text...");
                        }
                        else throw runtime_error("Empty text...");
                    }
                    // CSShutdown 메시지
                    else if (typeValue == "CSShutdown") {
                        JsonHandlers[typeValue](work.dataOwner, nullptr);
                    }
                } catch (runtime_error & ex) {
                    cerr << "Exception caught while parsing JSON.." << ex.what() <<  endl;
                }
            } else // Type 멤버가 없는 경우 잘못된 메시지 형식
                cerr << "Missing or invalid 'type' key in JSON data." << endl;

        // Protobuf 포맷으로 설정 되어있는 경우
        } else {  
            // protobuf로 처리중인 경우
            unique_ptr<mju::CSName> nameProtobuf;
            unique_ptr<mju::CSCreateRoom> createRoomProtobuf;
            unique_ptr<mju::CSJoinRoom> joinRoomProtobuf;
            unique_ptr<mju::CSChat> chatProtobuf;

            switch (work.msgTypeForProtobuf) {
            case mju::Type_MessageType::Type_MessageType_CS_NAME:
                nameProtobuf = make_unique<mju::CSName>();
                nameProtobuf->ParseFromArray((const void*)work.dataToParse, work.bytesOfDataForProtobuf);
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, (const void*)nameProtobuf->name().c_str());
                break;
            case mju::Type_MessageType::Type_MessageType_CS_ROOMS:
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, nullptr);
                break;
            case mju::Type_MessageType::Type_MessageType_CS_CREATE_ROOM:
                createRoomProtobuf = make_unique<mju::CSCreateRoom>();
                createRoomProtobuf->ParseFromArray((const void*)work.dataToParse, work.bytesOfDataForProtobuf);
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, (const void*)createRoomProtobuf->title().c_str());
                break;
            case mju::Type_MessageType::Type_MessageType_CS_JOIN_ROOM:
                joinRoomProtobuf = make_unique<mju::CSJoinRoom>();
                joinRoomProtobuf->ParseFromArray((const void*)work.dataToParse, work.bytesOfDataForProtobuf);
                int temp;
                temp = static_cast<int>(joinRoomProtobuf->roomid()); // 초기화
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, (const void*)&temp);
                break;
            case mju::Type_MessageType::Type_MessageType_CS_LEAVE_ROOM:
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, nullptr);
                break;
            case mju::Type_MessageType::Type_MessageType_CS_CHAT:
                chatProtobuf = make_unique<mju::CSChat>();
                chatProtobuf->ParseFromArray((const void*)work.dataToParse, work.bytesOfDataForProtobuf);
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, chatProtobuf->text().c_str());
                break;
            case mju::Type_MessageType::Type_MessageType_CS_SHUTDOWN:
                ProtobufHandlers[work.msgTypeForProtobuf](work.dataOwner, nullptr);
                break;
            }
        }
        {
            // 여기까지 도달했을 경우 메시지 처리가 완료된 경우이다.
            unique_lock<mutex> socketsOnQueueLock(SocketsOnQueueMutex);

            // SocketsOnQueue에 소켓번호를 지워줌으로써 해당 소켓의 이후의 메시지를 이제 처리가능함을 알림
            SocketsOnQueue.erase(work.dataOwner);

            // 메인 쓰레드가 해당 메시지의 처리 완료를 기다리고 있을 수 있음
            SocketsOnQueueAddable.notify_one();
        }
    }

    cout << "메시지 작업 쓰레드 #" << this_thread::get_id() << " 종료" << endl;

    return;
}

bool ChatServer::FindUserBySocketNum(int sock, shared_ptr<User>& user) {
    // ! 주의: UsersMutex 를 사용 중

    shared_ptr<User> userFound = nullptr;
    {
        lock_guard<mutex> usersLock(UsersMutex);
        for (auto u : Users) // 유저 목록 순회
            // 유저의 소켓번호로 식별하여 찾음
            if (u->socketNumber == sock) {
                userFound = u;
                break;
            }
        if (userFound != nullptr)
            user = userFound;
    }
    return userFound != nullptr;
}

void ChatServer::CustomSend(int sock, void* dataToSend, int bytesToSend) {
    int offset = 0;

    // bytesToSend 바이트 수만큼 전송
    while (offset < bytesToSend) {
        int numSend = send(sock, (char*)dataToSend + offset, bytesToSend - offset, 0);
        if (numSend < 0) {
            cerr << "send() failed: " << strerror(errno) << endl;
        } else {
            cout << "Sent: " << numSend << endl;
            offset += numSend;
        }
    }
}

bool ChatServer::CustomReceive(int clientSock, void* buf, size_t size, int& numRecv) {
    numRecv = recv(clientSock, buf, size, MSG_DONTWAIT);

    if (numRecv == 0) {
        cout << "Socket closed: " << clientSock << endl;
        {
            _willClose.insert(clientSock);
            return false;
        }
    } else if (numRecv < 0) {
        cerr << "recv() failed: " << strerror(errno)
             << ", clientSock: " << clientSock << endl;
        {
            _willClose.insert(clientSock);
            return false;
        }
    }
    cout << "Received: " << numRecv << " bytes, clientSock: " << clientSock << endl;

    return true;
}

ChatServer& ChatServer::CreateSingleton() {
    if (_Instance == NULL)
        _Instance = new ChatServer();
    return *_Instance;
}

ChatServer::ChatServer() {
    _serverSocket = -1;
    _isBinded = false;
    _isOpened = false;
    IsRunning.store(false);
    TerminateSignal.store(false);
}

ChatServer::~ChatServer() {
    IsRunning.store(false);

    // 소켓 정리
    if (_serverSocket != -1)
        close(_serverSocket);
    for (auto& client : _clients) 
        close(client);

    // Worker threads 정리
    if (_workers.size() > 0) {
        cout << "작업 Thread 정리 중" << endl;
        for (auto& worker : _workers)
            if (worker.joinable()) {
                cout << "작업 쓰레드 join() 시작" << endl;
                worker.join();
                cout << "작업 쓰레드 join() 완료" << endl;
            }
    }
        
    _Instance = nullptr;
}
