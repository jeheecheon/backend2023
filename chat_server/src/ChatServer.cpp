
#include "../include/ChatServer.h"

#include <netinet/tcp.h>
#include <sys/select.h>
#include <unistd.h>

#include "../include/SmallWork.h"
#include "../include/SmallWorkHandler.h"

using namespace std;

bool ChatServer::_CustomReceive(int clientSock, void* buf, size_t size,
                                int& numRecv) {
    numRecv = recv(clientSock, buf, size, 0);

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
    cout << "Received: " << numRecv << " bytes, clientSock: " << clientSock
         << endl;

    return true;
}

// Open 서버 소켓
bool ChatServer::OpenServerSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    int on = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) <
        0) {
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
bool ChatServer::BindServerSocket(int port,
                                  in_addr_t addr) {  // port, INADDR_ANY
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
    return true;
}

bool ChatServer::Start(int numOfWorkerThreads) {
    if (_serverSocket == -1 || _isBinded || _port == -1) return false;

    // 서버 소켓을 passive socket 으로 변경
    if (listen(_serverSocket, 10) < 0) {
        cerr << "listen() failed: " << strerror(errno) << endl;
        return false;
    }

    cout << "Port 번호 " << _port << "에서 서버 동작 중" << endl;

    struct sockaddr_in sin;

    for (int i = 0; i < numOfWorkerThreads; ++i)
        workers.insert(make_unique<thread>(HandleSmallWork));

    //
    while (true) {
        fd_set rset;

        FD_ZERO(&rset);
        FD_SET(_serverSocket, &rset);

        int maxFd = _serverSocket;

        for (int clientSock : _clientSocks) {
            FD_SET(clientSock, &rset);

            if (clientSock > maxFd) maxFd = clientSock;
        }

        int numReady = select(maxFd + 1, &rset, NULL, NULL, NULL);
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
                _clientSocks.insert(clientSock);

                cout << "새로운 클라이언트 접속 [(" << inet_ntoa(sin.sin_addr)
                     << ", " << ntohs(sin.sin_port) << "):None]" << endl;
            }
        }

        // 클라이언트 소켓에 읽기 이벤트가 발생한 게 있는지
        for (int clientSock : _clientSocks) {
            if (!FD_ISSET(clientSock, &rset)) continue;

            char data[65565];
            int numRecv;
            short bytesToReceive;

            memset(data, 0, sizeof(data));

            // 데이터의 크기가 담긴 첫 두 바이트를 읽어옴
            if (!_CustomReceive(clientSock, &bytesToReceive,
                                sizeof(bytesToReceive), numRecv))
                continue;

            // 나머지 데이터를 모두 읽어옴
            if (!_CustomReceive(clientSock, data, bytesToReceive, numRecv))
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
                messagesQueue.push({data, clientSock});
                messagesQueueEdited.notify_one();
                cout << "pushed" << endl;
            }
        }

        for (int clientSock : _willClose) {
            cout << "Closed: " << clientSock << endl;
            close(clientSock);
            _clientSocks.erase(clientSock);
        }
        _willClose.clear();
    }
}

ChatServer::ChatServer() {
    _serverSocket = -1;
    _isBinded = false;
    _port = -1;
}

ChatServer::~ChatServer() {
    cout << "작업 Thread 정리 중" << endl;

    // 소켓 정리
    for (int clientSock : _clientSocks) close(clientSock);
    close(_serverSocket);

    // Worker threads 정리
    for (auto& worker : workers)
        if (worker->joinable()) {
            cout << "작업 쓰레드 join() 시작" << endl;
            worker->join();
            cout << "작업 쓰레드 join() 완료" << endl;
        }
}
