#pragma once

#ifndef ROOM_H
#define ROOM_H

#include <set>
#include <string>

#include "User.h"

class Room {
public:
    std::set<User*> clientsInThisRoom;
    string title;
    int clientsCount;

public:
    Room();
    ~Room();

    bool IsThisRoomEmpty() const;
};

#endif // ROOM_H
