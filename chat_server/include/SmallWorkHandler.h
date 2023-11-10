#pragma once

#ifndef SMALL_WORK_HANDLER_H
#define SMALL_WORK_HANDLER_H

#include <iostream>

bool CustomReceive(int clientSock, void* buf, size_t size, int& numRecv);

void HandleSmallWork();

#endif  // WORK_HANDLER_H
