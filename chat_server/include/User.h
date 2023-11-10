#pragma once

#include <string>

// 전방 선언
class Room;

class User {
private:
    Room* _roomThisUserIn; // Room 포인터
    std::string _userName;
    int _connectedSocNum;

public:
    User();
    ~User();

    // Getter and Setter methods
    Room* getRoomThisUserIn() const;
    void setRoomThisUserIn(Room* room);
    const std::string& getUserName() const;
    void setUserName(const std::string& userName);
    int getConnectedSocNum() const;
    void setConnectedSocNum(int connectedSocNum);
};
