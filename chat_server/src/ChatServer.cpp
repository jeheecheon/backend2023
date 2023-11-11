
#include "../include/ChatServer.h"

#include <netinet/tcp.h>
#include <sys/select.h>
#include <unistd.h>
#include <algorithm>

#include "../include/SmallWork.h"
#include "../rapidjson/document.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"

using namespace rapidjson;
using namespace std;


ChatServer* ChatServer::_instance = nullptr;

bool ChatServer::isJson = true;

volatile atomic<bool> ChatServer::terminateSignal;

set<shared_ptr<Room>> ChatServer::rooms;
mutex ChatServer::roomsMutex;


set<shared_ptr<User>> ChatServer::users;
mutex ChatServer::usersMutex;


queue<SmallWork> ChatServer::messagesQueue;
mutex ChatServer::messagesQueueMutex;
condition_variable ChatServer::messagesQueueEdited;


unordered_set<int> ChatServer::socketsOnQueue;
mutex ChatServer::socketsOnQueueMutex;


unordered_map<string, ChatServer::MessageHandler> ChatServer::jsonHandlers;
unordered_map<mju::Type::MessageType, ChatServer::MessageHandler> ChatServer::protobufHandlers;



void ChatServer::SetIsJson(bool b) {
    isJson = b;
}

// Open 서버 소켓
bool ChatServer::OpenServerSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    int on = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        cerr << "setsockopt() failed: " << strerror(errno) << endl;
        return false;
    }
    if (setsockopt(_serverSocket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) <
        0) {
        cerr << "Failed to turn off the Nagle’s algorithm...";
        return false;
    }
    return true;
}

// 특정 TCP Port 에 bind
bool ChatServer::BindServerSocket(int port, in_addr_t addr) {  // port, INADDR_ANY
    if (_serverSocket == -1) return false;

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
    if (_serverSocket == -1 || !_isBinded || _port == -1) return false;

    // 서버 소켓을 passive socket 으로 변경
    if (listen(_serverSocket, 10) < 0) {
        cerr << "listen() failed: " << strerror(errno) << endl;
        return false;
    }

    cout << "Port 번호 " << _port << "에서 서버 동작 중" << endl;

    ConfigureMsgHandlers();

    struct sockaddr_in sin;

    for (int i = 0; i < numOfWorkerThreads; ++i)
        workers.push_back(thread(HandleSmallWork));

    //
    while (terminateSignal.load() == false) {
        fd_set rset;

        FD_ZERO(&rset);
        FD_SET(_serverSocket, &rset);

        int maxFd = _serverSocket;

        for (auto& client : _clients) {
            FD_SET(client, &rset);

            if (client > maxFd) 
                maxFd = client;
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100 milliseconds

        int numReady = select(maxFd + 1, &rset, NULL, NULL, &timeout);
        if (numReady < 0) {
            cerr << "select() failed: " << strerror(errno) << endl;
            continue;
        } else if (numReady == 0)
            continue;

        if (FD_ISSET(_serverSocket, &rset)) {
            memset(&sin, 0, sizeof(sin));
            unsigned int sin_len = sizeof(sin);

            int clientSock = accept(_serverSocket, (struct sockaddr*)&sin, &sin_len);

            if (clientSock < 0)
                cerr << "accept() failed: " << strerror(errno) << endl;
            else {
                shared_ptr<User> user = make_shared<User>();
                user->ipAddress = string(inet_ntoa(sin.sin_addr));
                user->port = sin.sin_port;
                user->socketNumber = clientSock;

                {
                    lock_guard<mutex> usersLock(usersMutex);
                    users.insert(user);
                    cout << "새로운 클라이언트 접속 " << user->PortAndIpAndNameToString() << endl;
                }

                _clients.insert(clientSock);
            }
        }

        // 클라이언트 소켓에 읽기 이벤트가 발생한 게 있는지
        for (auto& client : _clients) {
            if (!FD_ISSET(client, &rset)) continue;

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

            {
                lock_guard<mutex> socketsOnQueueLock(socketsOnQueueMutex);
                // 아직 해당 소켓의 과거 메시지가 처리되지 못 했는지 확인
                if (socketsOnQueue.find(client) != socketsOnQueue.end())
                    continue;
                socketsOnQueue.insert(client); // 해당소켓의 과거 메시지가 처리 중인지 확인하기 위해 다른 자료구조에 소켓을 보관
            }
            {
                lock_guard<mutex> messagesQueueLock(messagesQueueMutex);
                messagesQueue.push({data, client}); // 메시지를 작업 큐에 삽입
                messagesQueueEdited.notify_one();
            }
        }

        // 닫힌 소켓 정리
        for (int clientSock : _willClose) {
            close(clientSock);
            _clients.erase(clientSock);

            shared_ptr<User> userFound;
            if (!FindUserBySocketNum(clientSock, userFound)) 
                cerr << "User 정보 없음" << endl;
            else {
                lock_guard<mutex> usersLock(usersMutex);
                users.erase(userFound);
            }

            cout << "Closed: " << clientSock << endl;
        }
        _willClose.clear();
    }

    cout << "Main thread 종료 중" << endl;
    TerminateServer();
    return true;
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

// 핸들러들을 설정
// ConfigureHandlers(true); // JSON 핸들러 설정
// ConfigureHandlers(false); // Protobuf 핸들러 설정
void ChatServer::ConfigureMsgHandlers() {
    if (isJson) {
        // JSON handler 등록
        jsonHandlers["CSName"] = OnCsName;
        jsonHandlers["CSRooms"] = OnCsRooms;
        jsonHandlers["CSCreateRoom"] = OnCsCreateRoom;
        jsonHandlers["CSJoinRoom"] = OnCsJoinRoom;
        jsonHandlers["CSLeaveRoom"] = OnCsLeaveRoom;
        jsonHandlers["CSChat"] = OnCsChat;
        jsonHandlers["CSShutdown"] = OnCsShutDown;
    } else {
        // Protobuf handler 등록
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_NAME] = OnCsName;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_ROOMS] = OnCsRooms;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CREATE_ROOM] = OnCsCreateRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_JOIN_ROOM] = OnCsJoinRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_LEAVE_ROOM] = OnCsLeaveRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CHAT] = OnCsChat;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_SHUTDOWN] = OnCsShutDown;
    }
}

void ChatServer::Stop() {
    terminateSignal.store(true);
}

void ChatServer::TerminateServer() {
    terminateSignal.store(true);
    delete _instance;
}

void ChatServer::HandleSmallWork() {
    cout << "메시지 작업 쓰레드 #" << this_thread::get_id() << " 생성" << endl;

    while (!terminateSignal.load()) {
        SmallWork work;
        {
            unique_lock<mutex> messagesQueueLock(messagesQueueMutex);
            while (terminateSignal.load() == false && messagesQueue.empty())
                messagesQueueEdited.wait_for(messagesQueueLock, chrono::milliseconds(100));

            work = messagesQueue.front();
            messagesQueue.pop();
        }
        if (terminateSignal.load())
            break;

        if (isJson) {
            Document jsonDoc;
            jsonDoc.Parse(work.dataToParse);

            if (jsonDoc.HasParseError()) {
                cerr << "Failed to parse JSON data." << endl;
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            jsonDoc.Accept(writer);
            cout << "JSON Data: " << buffer.GetString() << endl;

            // Json 데이터 Type 멤버의 값 접근
            if (jsonDoc.HasMember("type") && jsonDoc["type"].IsString()) {
                const string typeValue = jsonDoc["type"].GetString();

                try {
                    if (typeValue == "CSName") {
                        if (jsonDoc.HasMember("name") && jsonDoc["name"].IsString()) {
                            string name = jsonDoc["name"].GetString();
                            if (!name.empty()) {
                                jsonHandlers[typeValue](work.dataOwner, (void*)name.c_str());
                            }
                            else throw runtime_error("Empty name...");
                        }
                        else throw runtime_error("Empty name...");
                    } else if (typeValue == "CSRooms") {
                        jsonHandlers[typeValue](work.dataOwner, nullptr);
                    } else if (typeValue == "CSCreateRoom") {
                        if (jsonDoc.HasMember("title") && jsonDoc["title"].IsString()) {
                            string title = jsonDoc["title"].GetString();
                            if (!title.empty())
                                jsonHandlers[typeValue](work.dataOwner, (void*)title.c_str());
                            else throw runtime_error("Empty title...");
                        }
                        else throw runtime_error("Empty title...");
                    } else if (typeValue == "CSJoinRoom") {
                        if (jsonDoc.HasMember("roomId") && jsonDoc["roomId"].IsInt()) {
                            int roomId = jsonDoc["roomId"].GetInt();
                            jsonHandlers[typeValue](work.dataOwner, (void*)&roomId);
                        }
                        else throw runtime_error("No roomId provided or failed to parse JSON");
                    } else if (typeValue == "CSLeaveRoom") {
                        jsonHandlers[typeValue](work.dataOwner, nullptr);
                    } else if (typeValue == "CSChat") {
                        if (jsonDoc.HasMember("text") && jsonDoc["text"].IsString()) {
                            string text = jsonDoc["text"].GetString();
                            if (!text.empty())
                                jsonHandlers[typeValue](work.dataOwner, (void*)text.c_str());
                            else throw runtime_error("Empty text...");
                        }
                        else throw runtime_error("Empty text...");
                    } else if (typeValue == "CSShutdown") {
                        jsonHandlers[typeValue](work.dataOwner, nullptr);
                    }
                } catch (runtime_error & ex) {
                    cerr << "Exception caught while parsing JSON.." << ex.what() <<  endl;
                }
            } else
                cerr << "Missing or invalid 'type' key in JSON data." << endl;

        } else {  // protobuf로 처리중인 경우
            // protobuf 는 두 개의 메시지로 분리하여 전송 받음
            // 첫 번째 메시지가 MessageType
            // 두 번째 메시지는 세부 타입이다

            mju::Type type;
            type.ParseFromString(work.dataToParse);
            mju::Type::MessageType messageType(type.type());

            switch (messageType) {
                case 0:
                    mju::CSName* csName = new mju::CSName;
                    csName->ParseFromString(work.dataToParse);
                    cout << csName->name().length() << endl;
                    break;
            }

            protobufHandlers[messageType](work.dataOwner, work.dataToParse);
        }
        {
            // 해당 소켓의 메시지 처리를 마치고 다른 메시지를 처리 할 준비가 됨
            lock_guard<mutex> socketsOnQueueLock(socketsOnQueueMutex);
            socketsOnQueue.erase(work.dataOwner);
        }
    }

    cout << "메시지 작업 쓰레드 #" << this_thread::get_id() << " 종료" << endl;

    return;
}

// ! 주의: usersMutex 를 사용 중
bool ChatServer::FindUserBySocketNum(int sock, shared_ptr<User>& user) {
    shared_ptr<User> userFound = nullptr;
    {
        lock_guard<mutex> usersLock(usersMutex);
        for (auto u : users)
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

ChatServer& ChatServer::CreateSingleton() {
    if (_instance == NULL)
        _instance = new ChatServer();
    return *_instance;
}

ChatServer::ChatServer() {
    _serverSocket = -1;
    _isBinded = false;
    _port = -1;
}

ChatServer::~ChatServer() {
    cout << "작업 Thread 정리 중" << endl;

    // 소켓 정리
    for (auto& client : _clients) 
        close(client);
    close(_serverSocket);

    // Worker threads 정리
    for (auto& worker : workers)
        if (worker.joinable()) {
            cout << "작업 쓰레드 join() 시작" << endl;
            worker.join();
            cout << "작업 쓰레드 join() 완료" << endl;
        }
}
