#include "../include/ChatServer.h"


// 전역 변수로서 시그널이 수신되었는지를 나타내는 플래그
volatile atomic<bool> signalReceived(false);


int main(int argc, char* argv[]) {
    // option 입력 방법 출력
    cout << endl
         << "--format: <json|protobuf>: 메시지 포맷\n\
    (default: 'json')\n\
--port: 서버 Port 번호\n\
    (an integer)\n\
--workers: 작업 쓰레드 숫자\n\
    (default: '2')\n\
    (an integer)\n";

    int numOfWorkerThreads = 2;
    int port = -1;
    bool isJson = true;

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
                } else if (strcmp(argv[i], "--format") == 0) {
                    if (strcmp(argv[i + 1], "json") == 0)
                        isJson = true;
                    else if (strcmp(argv[i + 1], "protobuf") == 0) {
                        isJson = false;
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
        cerr << "FATAL Flags parsing error: " <<
        endl << "Flag --port must have a value other than None." << 
        endl << "Flag --workers must hava a numeric value." << 
        endl << "Flag --format must be either json or protobuf." << endl;
        return -1;
    }

    // 서버 세팅
    //
    ChatServer& chatServer = ChatServer::CreateSingleton();

    chatServer.SetIsJson(isJson);

    if (!chatServer.OpenServerSocket()) 
        return -1;

    if (!chatServer.BindServerSocket(port, INADDR_ANY)) 
        return -1;

    //서버 시작
    if (!chatServer.Start(numOfWorkerThreads)) 
        return -1;

    return 0;
}
