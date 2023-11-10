#pragma once

#ifndef MESSAGE_HANDLERS_H
#define MESSAGE_HANDLERS_H

void OnCsName(const char* data);
void OnCsRooms(const char* data);
void OnCsCreateRoom(const char* data);
void OnCsJoinRoom(const char* data);
void OnCsLeaveRoom(const char* data);
void OnCsChat(const char* data);
void OnCsShutDown(const char* data);

#endif 
