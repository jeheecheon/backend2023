#pragma once

#ifndef MESSAGE_HANDLERS_H
#define MESSAGE_HANDLERS_H

void OnCsName(const void* data);
void OnCsRooms(const void* data);
void OnCsCreateRoom(const void* data);
void OnCsJoinRoom(const void* data);
void OnCsLeaveRoom(const void* data);
void OnCsChat(const void* data);
void OnCsShutDown(const void* data);

#endif 
