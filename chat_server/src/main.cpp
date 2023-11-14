#include "../include/ChatServer.h"


/**
 * @brief SIGINT 시그널 핸들러 함수: 채팅서버를 종료시키고 자원 정리
*/
void HandleTermination(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";

    // Sigleton 인스턴스를 불러옴
    if (ChatServer::HasInstance()) {
        ChatServer& chatServer = ChatServer::CreateSingleton();
        chatServer.Stop();
    }

    signal(signum, SIG_DFL);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, HandleTermination); // SIGINT 핸들러 등록

    // option 입력 방법 출력
    cout << endl <<
"--format: <json|protobuf>: 메시지 포맷\n\
    (default: 'json')\n\
--port: 서버 Port 번호\n\
    (an integer)\n\
--workers: 작업 쓰레드 숫자\n\
    (default: '2')\n\
    (an integer)\n";

    int numOfWorkerThreads = 2; // Worker threads 개수
    int port = -1; // 포트 번호
    bool isJson = true; // Json은 true, protobuf는 false

    // Arguments handling
    try {
        // argv 문자열 배열 순회
        for (int i = 1; i < argc; ++i) { 
            // 문자열 내용이 "--" 로 시작하는 경우
            if ((strlen(argv[i]) >= 3) && (argv[i][0] == '-') && (argv[i][1] == '-')) {
                // 입력된 옵션에 대응되는 value가 입력된 경우
                if (i + 1 < argc) {
                    // --port 옵션인 경우
                    if (strcmp(argv[i], "--port") == 0) {
                        port = stoi(argv[i + 1]);
                        if (port < 0)
                            throw runtime_error("포트 번호는 1 이상이어야 합니다.");
                        ++i;
                    }
                    // --workers 옵션인 경우
                    else if (strcmp(argv[i], "--workers") == 0) {
                        numOfWorkerThreads = stoi(argv[i + 1]);
                        if (numOfWorkerThreads <= 0)
                            throw runtime_error("Worker threads 개수는 1 이상이어야 합니다.");
                        ++i;
                    } 
                    // --format 옵션인 경우
                    else if (strcmp(argv[i], "--format") == 0) {
                        if (strcmp(argv[i + 1], "json") == 0)
                            isJson = true;
                        else if (strcmp(argv[i + 1], "protobuf") == 0) {
                            isJson = false;
                        } else
                            throw runtime_error("Format은 json 또는 protobuf만 지원됩니다.");
                        ++i;
                    }
                }
            }
        }
    } catch (runtime_error & ex) {
        cerr << "옵션 flags 입력 에러: " << ex.what() <<  endl;
        return -1;       
    }

    ChatServer& chatServer = ChatServer::CreateSingleton(); // 채팅서버 인스턴스 생성

    chatServer.SetIsJson(isJson); // 서버 입출력 형식 설정

    if (!chatServer.OpenServerSocket()) {
        cerr << "서버 소켓 오픈 실패" << endl;
        chatServer.TerminateServer();
        return -1;
    }

    if (!chatServer.BindServerSocket(port, INADDR_ANY)) {
        cerr << "서버 소켓 바인드 실패" << endl;
        chatServer.TerminateServer();
        return -1;
    }

    if (!chatServer.Start(numOfWorkerThreads)) {
        cerr << "서버 비정상 종료됨" << endl;
        chatServer.TerminateServer();
        return -1;
    }

    return 0;
}
