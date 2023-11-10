#pragma once

// Worker Thread가 처리할 일의 단위
typedef struct work {
    char* dataToParse; // 처리할 데이터
    int dataOwner; // 데이터 주인 소켓
} Work;