#pragma once

#include "resource.h"

void PopulateProcessesList(HWND hList);
void PopulateThreadsList(HWND hList, DWORD processId);
void InitProcessListView(HWND hWndListView);
void InitThreadListView(HWND hWndListView);
void UpdateProcessCount(HWND hWnd);
void UpdateTotalThreadCount(HWND hWnd);
int CALLBACK CompareFuncProcesses(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
int CALLBACK CompareFuncThreads(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
int ListView_FindItemByLParam(HWND hListView, LPARAM lParam);
void EnableDebugPrivilege();
bool IsSystemProcess(DWORD processID);