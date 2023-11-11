#pragma once

#ifndef MESSAGE_HANDLERS_H
#define MESSAGE_HANDLERS_H

void OnCsName(int clientSock, const void* data);
void OnCsRooms(int clientSock, const void* data);
void OnCsCreateRoom(int clientSock, const void* data);
void OnCsJoinRoom(int clientSock, const void* data);
void OnCsLeaveRoom(int clientSock, const void* data);
void OnCsChat(int clientSock, const void* data);
void OnCsShutDown(int clientSock, const void* data);

#endif 
