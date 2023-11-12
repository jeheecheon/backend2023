#pragma once

#ifndef WORK_H
#define WORK_H

/**
 * @struct smallWork
 * @brief 채팅 서버 클래스
 *
 * Worker Thread가 처리할 일의 단위
*/
typedef struct smallWork {
    char* dataToParse; // 처리할 데이터
    int dataOwner; // 데이터 주인 소켓
} SmallWork;

#endif