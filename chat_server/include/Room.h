#pragma once

#include <set>

class User; // 전방 선언

class Room {
private:
    std::set<User*> usersInThisRoom; // User 포인터

public:
    Room();
    ~Room();

    // Getter methods
    const std::set<User*>& getUsersInThisRoom() const;
    bool IsThisRoomEmpty() const;
};
