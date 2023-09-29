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

    struct sockaddr_in sin;
    socklen_t sin_size = sizeof(sin);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(20000 + 135); // 자기서브넷숫자
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        cerr << strerror(errno) << endl;
        return 0;
    }

    // string buf = "Hello World";
    // memset(&sin, 0, sizeof(sin));
    // sin.sin_family = AF_INET;
    // sin.sin_port = htons(20000 + 135); // 자기서브넷숫자
    // sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    // int numBytes = sendto(s, buf.c_str(), buf.length(), 0, (struct sockaddr *)&sin, sizeof(sin));
    // cout << "Sent: " << numBytes << endl;

    char buf2[65536];
    memset(&buf2, 0, sizeof(buf2));

    while (true)
    {
        // 수신
        memset(&sin, 0, sizeof(sin));
        memset(&buf2, 0, sizeof(buf2));
        int numBytes = recvfrom(s, buf2, sizeof(buf2), 0, (struct sockaddr *)&sin, &sin_size);
        cout << "Recevied: " << numBytes << endl;
        cout << "From " << inet_ntoa(sin.sin_addr) << endl;
        cout << buf2 << endl << endl;

        sleep(1); // 1초 딜레이


        /// 송신
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");
        sin.sin_port = htons(20000 + 135); // 자기서브넷숫자
        numBytes = sendto(s, buf2, numBytes, 0, (struct sockaddr *)&sin, sizeof(sin));
        cout << "Sent: " << numBytes << endl;
        cout << buf2 << endl << endl;

        sleep(1); // 1초 딜레이
    }

    close(s);

    return 0;
}
