#include "../include/ChatServer.h"

bool IsJson = true;  // Json 또는 Protobuf

// 읽기 이벤트가 발생한 클라이언트 queue
queue<SmallWork> messagesQueue;
mutex messagesQueueMutex;
condition_variable messagesQueueEdited;

// messagesQueue에서 아직 연산을 기다리는 Sockets들을 담는 set
unordered_set<int> socketsOnQueue;
mutex socketsOnQueueMutex;

// 전역 변수로서 시그널이 수신되었는지를 나타내는 플래그
volatile atomic<bool> signalReceived(false);

// Worker Thread의 MessageHandlers
unordered_map<const char*, MessageHandler> jsonHandlers;
unordered_map<mju::Type::MessageType, MessageHandler> protobufHandlers;


int main(int argc, char* argv[]) {
    // option 입력 방법 출력
    cout << endl
         << "--format: <json|protobuf>: 메시지 포맷\n\
    (default: 'json')\n\
--Port: 서버 Port 번호\n\
    (an integer)\n\
--workers: 작업 쓰레드 숫자\n\
    (default: '2')\n\
    (an integer)\n";

    int numOfWorkerThreads = 2;
    int port = -1;

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
    if (isInputError || port == -1) {
        cerr << "FATAL Flags parsing error: flag --Port=None: Flag --Port must "
                "have a value other than None."
             << endl;
        return -1;
    }

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
        // Protobuf handler 등록
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_NAME] =
        OnCsName;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_ROOMS] =
        OnCsRooms;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CREATE_ROOM]
        = OnCsCreateRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_JOIN_ROOM] =
        OnCsJoinRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_LEAVE_ROOM]
        = OnCsLeaveRoom;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_CHAT] =
        OnCsChat;
        protobufHandlers[mju::Type::MessageType::Type_MessageType_CS_SHUTDOWN] =
        OnCsShutDown;
    }

    // 서버 세팅
    ChatServer chatServer;
    if (!chatServer.OpenServerSocket()) 
        return -1;
    if (!chatServer.BindServerSocket(port, INADDR_ANY)) 
        return -1;
    //서버 시작
    if (!chatServer.Start(numOfWorkerThreads)) 
        return -1;

    return 0;
}