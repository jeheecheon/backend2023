#include <string>
#include "Room.h"
#include "User.h"

using namespace std;

class User {
private:
    Room _roomThisUserIn;
    string _userName;
    int _connectedSocNum;

public:
    User() {

    }

    ~User() {

    }

    // Getter for _roomNumber
    int getRoomNumber() const {
        return _roomNumber;
    }

    // Setter for _roomNumber
    void setRoomNumber(int roomNumber) {
        _roomNumber = roomNumber;
    }

    // Getter for _userName
    const string& getUserName() const {
        return _userName;
    }

    // Setter for _userName
    void setUserName(const string& userName) {
        _userName = userName;
    }

    // Getter for _connectedSocNum
    int getConnectedSocNum() const {
        return _connectedSocNum;
    }

    // Setter for _connectedSocNum
    void setConnectedSocNum(int connectedSocNum) {
        _connectedSocNum = connectedSocNum;
    }
};