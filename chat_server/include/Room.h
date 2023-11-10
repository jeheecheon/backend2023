#pragma once

#ifndef ROOM_H
#define ROOM_H

#include <set>
#include <string>

#include "Client.h"

class Client;  // 전방 선언

class Room {
   public:
    std::set<Client*> clientsInThisRoom;

   public:
    Room();
    ~Room();

    bool IsThisRoomEmpty() const;
};

#endif // ROOM_H
