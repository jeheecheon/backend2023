#include "../include/Client.h"

using namespace std;

Client::Client() {}

Client::~Client() {}

// Getter for _userName
const string& Client::getClientName() const { return _clientName; }

// Setter for _userName
void Client::setClientName(const string& clientName) { _clientName = clientName; }

// Getter for _connectedSocNum
int Client::getConnectedSocNum() const { return _connectedSocNum; }

// Setter for _connectedSocNum
void Client::setConnectedSocNum(int connectedSocNum) {
    _connectedSocNum = connectedSocNum;
}