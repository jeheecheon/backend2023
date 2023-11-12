#pragma once

#ifndef ROOM_H
#define ROOM_H

#include <set>
#include <string>
#include <vector>

#include "User.h"

/**
 * @class Room
 * @brief 채팅버버의 채팅방
 *
 * 채팅 서버의 방을 정의한 클래스
*/
class Room {
public:
    int roomId; // 방번호
    set<shared_ptr<User>> usersInThisRoom; // 해당 서버에 접속된 유저 목록
    string title; // 채팅방 이름

private:
    static int roomIdIncrementer; // 채팅방 생성될 때 현재 값을 부여하고 1 증가.

public:
    /**
     * @brief Room 생상자
    */
    Room();

    /**
     * @brief Room 생상자
     * @param title 방번호
    */
    Room(string title);
    
    /**
     * @brief Room 소멸자
    */
    ~Room();

    bool IsThisRoomEmpty() const;

    bool operator<(const Room& other) const;
};

#endif // ROOM_H
