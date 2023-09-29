#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>

using namespace std;

int main() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    cout << "Socket ID:" << s << endl;
    close(s);

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    cout << "Socket ID:" << s << endl;
    close(s);

    return 0;
}
