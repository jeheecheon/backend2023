#include "../include/Room.h"

using namespace std;

int Room::roomIdIncrementer = 0;

Room::Room() {
    roomId = ++roomIdIncrementer;
}

Room::Room(string title) {
    roomId = ++roomIdIncrementer;
    this->title = title;
}

Room::~Room() {}

bool Room::operator<(const Room& other) const {
    return roomId < other.roomId;
}

bool Room::IsThisRoomEmpty() const { return usersInThisRoom.size() == 0; }
