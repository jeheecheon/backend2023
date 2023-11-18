#pragma once

#ifndef USER_H
#define USER_H

#include <string>
#include <arpa/inet.h>
#include <memory>

using namespace std;

class Room;

/**
 * @class User
 * @brief 채팅버버의 유저
 *
 * 채팅 서버의 유저를 정의한 클래스
*/
class User {
public:
    shared_ptr<Room> roomThisUserIn; // 유저가 속한 방
    int socketNumber; // 소켓 번호
    string ipAddress; // 아이피
    in_port_t port; // 포트 번호
    string userName; // 유저 이름
public:
    /**
     * @brief User 생성자
    */
    User();

    /**
     * @brief User 소멸자
    */
    ~User();

    /**
     * @brief 포트, 아이피로 인코딩된 스트링을 만들어 반환한다
     *
     * @return 인코딩된 유저 이름 스트링
    */
    string PortAndIpIntoString() const;

    /**
     * @brief 포트, 아이피, 이름으로 인코딩된 스트링을 만들어 반환한다
     *
     * @return 인코딩된 유저 이름 스트링
    */
    string PortAndIpAndNameIntoString() const;

    /**
     * @brief 유저이름이 있으면 유저이름을 반환한다. 없으면 포트, 아이피로 인코딩된 스트링을 만들어 반환한다.
     *
     * @return 인코딩된 유저 이름 스트링
    */
    string GetUserName() const;

    bool operator<(const User& other) const;
};

#endif 
