#pragma once

#ifndef USER_H
#define USER_H

#include <string>

class Room;

class Client {
   private:
    Room* _roomThisUserIn;  // Room 포인터
    std::string _clientName;
    int _connectedSocNum;

   public:
    Client();
    ~Client();

    // Getter and Setter methods
    Room* getRoomThisUserIn() const;
    void setRoomThisUserIn(Room* room);
    const std::string& getClientName() const;
    void setClientName(const std::string& clientName);
    int getConnectedSocNum() const;
    void setConnectedSocNum(int connectedSocNum);
};

#endif 
