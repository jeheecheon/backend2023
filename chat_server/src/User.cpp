#include "../include/User.h"

using namespace std;

User::User() {}

User::~User() {}

string User::PortAndIpIntoString() const {
    return "('" + ipAddress + "', " + to_string(ntohs(port)) + ")";
}

string User::PortAndIpAndNameToString() const {
    return "[" + PortAndIpIntoString() + ":" + userName + "]"; 
}

bool User::operator<(const User& other) const {
    return socketNumber < other.socketNumber;
}

string User::GetUserName() const {
    if (!userName.empty())
        return userName;

    return PortAndIpIntoString();
}