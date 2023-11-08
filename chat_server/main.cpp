#include <iostream>

#include "DTOs/Protobuf/message.pb.h"

using namespace std;

int main(int argc, char* argv[]) {
    int port = 9105;
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
                    port = stoi(argv[i + 1]);
                    ++i;
                }
                // worker thread number옵션 handling
                else if (strcmp(argv[i], "--worker_thread_number") == 0) {
                    numOfWorkerThreads = stoi(argv[i + 1]);
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
    if (isInputError) {
        cerr << "Input Error" << endl;
        return -1;
    }

    return 0;
}