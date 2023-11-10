#include <string>
#include <set>

#include "Room.h"
#include "User.h"

using namespace std;

class Room {
private:
    const set<User> usersInThisRoom;

public:
    Room() {
    }

    ~Room() {
    }

    // Getter for usersInThisRoom
    const std::set<User>& getUsersInThisRoom() const {
        return usersInThisRoom;
    }

    bool IsThisRoomEmpty() const {
        return usersInThisRoom.empty();
    }
};