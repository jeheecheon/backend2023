#include "../include/Room.h"

using namespace std;

Room::Room() {}

Room::~Room() {}

bool Room::IsThisRoomEmpty() const { return clientsInThisRoom.empty(); }
