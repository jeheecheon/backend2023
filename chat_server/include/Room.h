#pragma once

#ifndef ROOM_H
#define ROOM_H

#include <set>
#include <string>
#include <vector>

#include "User.h"

class Room {
public:
    int roomId;
    vector<shared_ptr<User>> usersInThisRoom;
    string title;
    int clientsCount;

public:
    static int roomIdIncrementer;

public:
    Room();
    Room(string title);
    ~Room();

    bool operator<(const Room& other) const;

    bool IsThisRoomEmpty() const;
};

#endif // ROOM_H
