#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <queue>
#include <thread>

#include "DTOs/Protobuf/message.pb.h"

using namespace std;

void handleMessages() { 

    return;
}

int main(int argc, char* argv[]) {
    // option 입력 방법 출력
    cout << endl <<
"--format: <json|protobuf>: 메시지 포맷\n\
    (default: 'json')\n\
--port: 서버 port 번호\n\
    (an integer)\n\
--workers: 작업 쓰레드 숫자\n\
    (default: '2')\n\
    (an integer)\n";
    
    const char* format = "json";
    int port = -1;
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
                        port = stoi(argv[i + 1]);
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
                    if (strcmp(argv[i + 1], "json") == 0 || strcmp(argv[i + 1], "protobuf") == 0) {
                        format = argv[i + 1];
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
    if (isInputError || port == -1) {
        cerr << "FATAL Flags parsing error: flag --port=None: Flag --port must have a value other than None." << endl;
        return -1;
    }

    set<unique_ptr<thread>> workers;
    for (int i = 0; i < numOfWorkerThreads; ++i) {
        cout << "메시지 작업 쓰레드 #" << i << " 생성" << endl;
        workers.insert(make_unique<thread>(handleMessages));
    }

    
    // Open 서버 소켓
    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Port를 재사용할 수 있도록 옵션 설정
    int on = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        cerr << "setsockopt() failed: " << strerror(errno) << endl;
        return 1;
    }

    // 특정 TCP port 에 bind
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    if (bind(serverSock, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        cerr << "bind() failed: " << strerror(errno) << endl;
        return -1;
    }

    // 서버 소켓을 passive socket 으로 변경
    if (listen(serverSock, 10) < 0) {
        cerr << "listen() failed: " << strerror(errno) << endl;
        return -1;
    }

    cout << "Port 번호 " << port << "에서 서버 동작 중" << endl;


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

        // 각 클라이언트 소켓에 읽기 이벤트가 발생했다면
        //   1) 실제 읽을 데이터가 있거나
        //   2) 소켓이 닫힌 경우이다.
        set<int> willClose;
        for (int clientSock : clientSocks) {
            if (!FD_ISSET(clientSock, &rset))
                continue;

            // 이 연결로부터 데이터를 읽음
            char buf[65536];

            int numRecv = recv(clientSock, buf, sizeof(buf), 0);
            if (numRecv == 0) {
                cout << "Socket closed: " << clientSock << endl;
                willClose.insert(clientSock);
            } else if (numRecv < 0) {
                cerr << "recv() failed: " << strerror(errno)
                     << ", clientSock: " << clientSock << endl;
                willClose.insert(clientSock);
            } else {
                cout << "Received: " << numRecv
                     << " bytes, clientSock: " << clientSock << endl;

                // 읽은 데이터를 그대로 돌려준다.
                // send() 는 요청한 byte 수만큼 전송을 보장하지 않으므로
                // 반복해서 send 를 호출해야 될 수 있다.
                int offset = 0;
                while (offset < numRecv) {
                    int numSend =
                        send(clientSock, buf + offset, numRecv - offset, 0);

                    if (numSend < 0) {
                        cerr << "send() failed: " << strerror(errno)
                             << ", clientSock: " << clientSock << endl;
                        willClose.insert(clientSock);
                        break;
                    } else {
                        cout << "Sent: " << numSend
                             << " bytes, clientSock: " << clientSock << endl;
                        offset += numSend;
                    }
                }
            }
        }

        // 닫아야 되는 소켓들 정리
        for (int clientSock : willClose) {
            close(clientSock);
            clientSocks.erase(clientSock);
        }
    }

    // Worker threads 정리
    for (auto& worker : workers)
        if (worker->joinable()) worker->join();

    close(serverSock);

    return 0;
}