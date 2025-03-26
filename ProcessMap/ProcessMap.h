#pragma once

#include "resource.h"

void PopulateProcessesList(HWND hList);
void PopulateThreadsList(HWND hList, DWORD processId);
void InitProcessListView(HWND hWndListView);
void InitThreadListView(HWND hWndListView);
void UpdateProcessCount();
void UpdateTotalThreadCount();
int ListView_FindItemByLParam(HWND hListView, LPARAM lParam);
void EnableDebugPrivilege();
bool IsSystemProcess(DWORD processID);