#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <string>

using namespace std;

int main()
{
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0)
        return 1;

    string buf1;
    char buf2[65536];

    struct sockaddr_in sin;
    socklen_t sin_size = sizeof(sin);

    memset(&sin, 0, sin_size);

    // EOF는 입력 실패로 간주되어 false 반환
    while (cin >> buf1)
    {
        // 소켓 옵션 설정
        sin.sin_family = AF_INET;
        sin.sin_port = htons(10001);
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");

        // 입력된 문자열 전송 및 전송된 바이트 출력
        int numBytes = sendto(s, buf1.c_str(), buf1.length(), 0, (struct sockaddr *)&sin, sin_size);
        cout << "Sent: " << numBytes << endl;

        // 소켓 옵션과 buf2 버퍼 초기화
        memset(&sin, 0, sin_size);
        memset(&buf2, 0, sizeof(buf2));

        // 송신 및 관련 정보 출력
        numBytes = recvfrom(s, buf2, sizeof(buf2), 0, (struct sockaddr *)&sin, &sin_size);
        cout << "Recevied: " << numBytes << endl;
        cout << "From " << inet_ntoa(sin.sin_addr) << endl;
        cout << buf2 << endl;
    }

    close(s);

    return 0;
}
