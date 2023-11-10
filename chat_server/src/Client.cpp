#include "../include/Client.h"

using namespace std;

Client::Client() {
    clientName = "None";
}

Client::~Client() {}

string Client::PortAndIpIntoString() {
    return "(" + ipAddress + ", " + to_string(ntohs(port)) + ")";
}

string Client::PortAndIpAndNameToString() {
    return "[" + PortAndIpIntoString() + ":" + clientName + "]"; 
}

bool Client::operator<(const Client& other) const {
    return socketNumber < other.socketNumber;
}