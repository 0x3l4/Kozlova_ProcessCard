#include <Windows.h>
#include <TlHelp32.h>
#include "framework.h"
#include "ProcessMap.h"
#include <commctrl.h>
#include <psapi.h>
#include <winternl.h>
#include <iostream>

// Определение макроса для максимальной длины строки загрузки (заголовка окна)
#define MAX_LOADSTRING 100
#define IDC_STATUS_BAR 1001


// Глобальная переменная экземпляра приложения
HINSTANCE hInst;
// Массив символов для хранения заголовка окна
WCHAR szTitle[MAX_LOADSTRING];
// Массив символов для хранения имени класса окна
WCHAR szWindowClass[MAX_LOADSTRING];

// Функция регистрации класса окна
ATOM                MyRegisterClass(HINSTANCE hInstance);
// Функция инициализации экземпляра приложения
BOOL                InitInstance(HINSTANCE, int);
// Функция обработки сообщений окна
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
// Функция обработки сообщений диалогового окна "О программе"
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Дескриптор окна списка процессов
HWND hListProcesses;
// Дескриптор окна списка потоков
HWND hListThreads;
// Дескриптор списка процессов
HWND hStatusBar;

int parts[] = { 150, 300, 450, -1 }; // -1 означает, что последняя секция занимает оставшееся место

int sortColumnProcesses = 0;  // По умолчанию сортировка по PID
bool ascendingProcesses = true;

int sortColumnThreads = 0;  // По умолчанию сортировка по ID потока
bool ascendingThreads = true;

// Главная точка входа для приложений Windows
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{

	UNREFERENCED_PARAMETER(hPrevInstance); // Игнорируем неиспользуемый параметр
	UNREFERENCED_PARAMETER(lpCmdLine); // Игнорируем командную строку

	EnableDebugPrivilege();

	// Загружаем строку заголовка окна из ресурсов
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	// Загружаем имя класса окна из ресурсов
	LoadStringW(hInstance, IDC_PROCESSMAP, szWindowClass, MAX_LOADSTRING);
	// Регистрируем класс окна
	MyRegisterClass(hInstance);

	// Инициализируем приложение
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE; // Если инициализация не удалась, выходим с ошибкой
	}

	// Загружаем таблицу ускорителей (горячих клавиш)
	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PROCESSMAP));

	// Структура для хранения сообщений
	MSG msg;

	// Основной цикл обработки сообщений
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		// Проверяем, есть ли горячие клавиши для данного сообщения
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg); // Переводим виртуальные коды клавиш в символьные сообщения
			DispatchMessage(&msg); // Отправляем сообщение соответствующей процедуре окна
		}
	}

	// Возвращаем код завершения приложения
	return (int)msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
	// Определяем структуру для описания класса окна
	WNDCLASSEXW wcex;

	// Устанавливаем размер структуры
	wcex.cbSize = sizeof(WNDCLASSEX);

	// Задаём стиль класса окна
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	// Указываем функцию обратного вызова для обработки сообщений окна
	wcex.lpfnWndProc = WndProc;
	// Дополнительное пространство для класса окна (здесь не нужно)
	wcex.cbClsExtra = 0;
	// Дополнительное пространство для каждого окна (здесь не нужно)
	wcex.cbWndExtra = 0;
	// Присваиваем текущий экземпляр приложения
	wcex.hInstance = hInstance;
	// Загружаем большой значок для окна
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_PROCESSMAP));
	// Загружаем стандартный курсор для окна
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	// Устанавливаем цвет фона окна
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	// Присваиваем меню для окна
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PROCESSMAP);
	// Присваиваем имя класса окна
	wcex.lpszClassName = szWindowClass;
	// Загружаем маленький значок для окна
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	// Регистрируем класс окна
	return RegisterClassExW(&wcex);
}

// Функция инициализирует первый экземпляр окна приложения
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	// Сохраняем экземпляр приложения в глобальной переменной
	hInst = hInstance;

	// Создаем главное окно приложения
	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	// Проверяем успешность создания окна
	if (!hWnd)
	{
		return FALSE; // Создание окна не удалось
	}

	// Отображаем окно с заданным параметром отображения
	ShowWindow(hWnd, nCmdShow);
	// Обновляем клиентскую область окна
	UpdateWindow(hWnd);

	return TRUE; // Успешная инициализация
}

// Основная функция обработки сообщений окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR lpnmh = nullptr;
	HDC hdc = nullptr; // Инициализация по умолчанию
	UINT wmId = 0; // Инициализация по умолчанию

	switch (message)
	{
	case WM_CREATE: // Сообщение отправляется при создании окна
		// Создаем дочернее окно-список процессов
		hListProcesses = CreateWindowW(WC_LISTVIEW, NULL,
			WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
			2, 2, 500, 670,
			hWnd, (HMENU)IDC_LIST_PROCESSES, NULL, NULL);

		// Создаем дочернее окно-список потоков
		hListThreads = CreateWindowW(WC_LISTVIEW, NULL,
			WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
			532, 2, 300, 670,
			hWnd, (HMENU)IDC_LIST_THREADS, NULL, NULL);

		// Инициализируем списки
		InitProcessListView(hListProcesses);
		InitThreadListView(hListThreads);

		// Заполняем список процессов


		// Создаем статус-бар
		hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
			WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
			0, 0, 0, 0, hWnd, (HMENU)IDC_STATUS_BAR, hInst, NULL);

		// Разделение статус-бара на секции
		SendMessage(hStatusBar, SB_SETPARTS, _countof(parts), (LPARAM)parts);

		// Устанавливаем текст в статус-баре
		SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Процессов: 0");
		SendMessage(hStatusBar, SB_SETTEXT, 1, (LPARAM)L"Потоков: 0");
		SendMessage(hStatusBar, SB_SETTEXT, 2, (LPARAM)L"Общ. потоков: None");
		PopulateProcessesList(hListProcesses);

		// Заполняем список потоков (изначально без выбора процесса)
		//PopulateThreadsList(hListThreads, 0);
		break;

	case WM_PAINT: // Сообщение приходит при необходимости перерисовки окна
		PAINTSTRUCT ps;
		hdc = BeginPaint(hWnd, &ps); // Начинаем отрисовку
		EndPaint(hWnd, &ps); // Заканчиваем отрисовку
		break;

	case WM_DESTROY: // Сообщение приходит при уничтожении окна
		PostQuitMessage(0); // Завершение работы программы
		break;

	case WM_COMMAND: // Сообщение приходит при выборе пункта меню или нажатии кнопки

		// Проверяем изменение выбора в списке процессов
		if (LOWORD(wParam) == IDC_LIST_PROCESSES && HIWORD(wParam) == LBN_SELCHANGE)
		{
			int index = (int)SendMessage(hListProcesses, LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{ // Если элемент выбран
				wchar_t buffer[256]; // Буфер для хранения текста
				// Получаем текст выбранного элемента
				SendMessage(hListProcesses, LB_GETTEXT, index, (LPARAM)buffer);

				DWORD processID = _wtoi(buffer);  // Получаем PID
				// Заполняем список потоков для выбранного процесса
				PopulateThreadsList(hListThreads, processID);
			}
		}

		wmId = LOWORD(wParam); // Идентифицируем команду

		// Реагируем на команды из меню
		switch (wmId)
		{
		case IDM_ABOUT:  // Команда "О программе"
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT: // Команда выхода
			DestroyWindow(hWnd);
			break;

		case IDM_REFRESH:  // Обновление списка
			// Обновляем список процессов
			PopulateProcessesList(hListProcesses);
			// Удаляем все элементы из списка потоков
			ListView_DeleteAllItems(hListThreads);
			break;
		}

		break;
	case WM_NOTIFY: // Сообщение приходит от элементов управления
		if (((LPNMHDR)lParam)->hwndFrom == hListProcesses && ((LPNMHDR)lParam)->code == LVN_ITEMCHANGED)
		{
			NMLISTVIEW* pNMLV = (NMLISTVIEW*)lParam; // Получаем указатель на структуру уведомлений списка
			if (pNMLV->uNewState & LVIS_SELECTED) // Проверяем, был ли выделен новый элемент
			{
				wchar_t buffer[10]; // Буфер для хранения текста
				// Получаем текст выбранного элемента
				ListView_GetItemText(hListProcesses, pNMLV->iItem, 0, buffer, sizeof(buffer));
				// Преобразуем текст в числовой идентификатор процесса
				DWORD processID = _wtoi(buffer);
				// Заполняем список потоков для выбранного процесса
				PopulateThreadsList(hListThreads, processID);
			}
		}

		lpnmh = (LPNMHDR)lParam;

		// Обработка списка процессов
		if (lpnmh->hwndFrom == hListProcesses && lpnmh->code == LVN_COLUMNCLICK) {
			NMLISTVIEW* pNMLV = (NMLISTVIEW*)lParam;
			if (sortColumnProcesses == pNMLV->iSubItem) {
				ascendingProcesses = !ascendingProcesses;
			}
			else {
				sortColumnProcesses = pNMLV->iSubItem;
				ascendingProcesses = true;
			}
		}

		// Обработка списка потоков
		if (lpnmh->hwndFrom == hListThreads && lpnmh->code == LVN_COLUMNCLICK) {
			NMLISTVIEW* pNMLV = (NMLISTVIEW*)lParam;
			if (sortColumnThreads == pNMLV->iSubItem) {
				ascendingThreads = !ascendingThreads;
			}
			else {
				sortColumnThreads = pNMLV->iSubItem;
				ascendingThreads = true;
			}
		}

		break;
	default: // Для всех остальных сообщений используем стандартную обработку
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0; // Для всех остальных сообщений используем стандартную обработку
}

// Функция обработки сообщений для диалогового окна "О программе"
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Игнорируем неиспользуемый параметр
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG: // Сообщение приходит при инициализации диалогового окна
		return (INT_PTR)TRUE; // Диалог успешно инициализирован

	case WM_COMMAND: // Сообщение приходит при взаимодействии с элементами диалога
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam)); // Закрываем диалоговое окно
			return (INT_PTR)TRUE; // Обработка выполнена успешно
		}
		break;
	}
	return (INT_PTR)FALSE;  // Сообщение не обработано
}


// Функция заполняет список процессов в предоставленном окне списка (list view).
void PopulateProcessesList(HWND hList)
{
	// Сначала удаляем все существующие элементы из списка
	ListView_DeleteAllItems(hList);

	// Массив для хранения идентификаторов процессов
	DWORD processes[1024];
	DWORD needed;  // Переменная для хранения количества байтов, необходимых для получения всех идентификаторов процессов

	// Получаем массив идентификаторов активных процессов
	if (EnumProcesses(processes, sizeof(processes), &needed))
	{
		// Вычисляем количество найденных процессов
		int count = needed / sizeof(DWORD);

		// Структура для добавления новых элементов в список
		LVITEM lvI;

		// Проходим по каждому процессу
		for (int i = 0; i < count; i++)
		{
			DWORD pid = processes[i];  // Идентификатор процесса
			if (pid == 0) continue;   // Пропускаем нулевые значения

			// Открываем процесс для получения дополнительной информации
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

			if (hProcess || !hProcess)
			{
				// По умолчанию имя процесса неизвестно
				wchar_t processName[MAX_PATH] = L"<Unknown>";

				// Получаем имя исполняемого файла процесса
				GetModuleBaseName(hProcess, NULL, processName, MAX_PATH);

				// Проверяем, является ли процесс системным
				if (IsSystemProcess(pid) || wcscmp(processName, L"<Unknown>") == 0)
				{
					wcscat_s(processName, MAX_PATH, L" [SYSTEM]");
				}

				// Подсчитываем количество потоков в процессе
				DWORD threadCount = 0;
				HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
				int totalThreads = 0;
				if (hSnap != INVALID_HANDLE_VALUE)
				{
					PROCESSENTRY32 pe;
					pe.dwSize = sizeof(pe);
					if (Process32First(hSnap, &pe))
					{
						totalThreads++;
						do {
							if (pe.th32ProcessID == pid) {
								threadCount = pe.cntThreads;
								break;
							}
						} while (Process32Next(hSnap, &pe));
					}
					CloseHandle(hSnap);
				}

				wchar_t threadCountStr[50];
				swprintf(threadCountStr, 50, L"Общ. потоков: %d", threadCount);
				SendMessage(hStatusBar, SB_SETTEXT, 2, (LPARAM)threadCountStr);

				// Преобразовываем идентификаторы в строки
				wchar_t pidStr[10], threadStr[10];
				swprintf(pidStr, 10, L"%d", pid);
				swprintf(threadStr, 10, L"%d", threadCount);

				LVITEM lvI;
				lvI.mask = LVIF_TEXT | LVIF_PARAM;
				lvI.iItem = ListView_GetItemCount(hListProcesses);
				lvI.iSubItem = 0;
				lvI.pszText = pidStr;
				lvI.lParam = pid;  // ❗ Храним PID, а не индекс строки!
				ListView_InsertItem(hListProcesses, &lvI);

				// Добавляем дополнительные столбцы с именем процесса и количеством потоков
				ListView_SetItemText(hList, lvI.iItem, 1, processName);
				ListView_SetItemText(hList, lvI.iItem, 2, threadStr);

				// Закрываем дескриптор процесса
				CloseHandle(hProcess);
			}
			// Обновляем статус-бар с актуальным количеством процессов
			//wchar_t processCountStr[50];
			//swprintf(processCountStr, 50, L"Процессовввв: %d", count);
			//SendMessage(hStatusBar, SB_SETTEXT, 1, (LPARAM)processCountStr);
		}

	}

	UpdateProcessCount();
	UpdateTotalThreadCount();
}

// Функция заполняет список потоков для указанного процесса в предоставленное окно списка.
void PopulateThreadsList(HWND hList, DWORD processId)
{
	// Очищаем существующий список
	ListView_DeleteAllItems(hList);

	// Создаём снимок всех потоков системы
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnap == INVALID_HANDLE_VALUE) return;  // Если снимок не получен, прерываем выполнение

	// Структура для получения информации о потоке
	THREADENTRY32 te;
	te.dwSize = sizeof(te);

	// Структура для добавления нового элемента в список
	LVITEM lvI;

	// Проходим по каждому потоку в снимке
	if (Thread32First(hSnap, &te))
	{
		do {
			if (te.th32OwnerProcessID == processId)  // Фильтруем потоки по указанному процессу
			{
				// Преобразовываем идентификаторы потока и процесса в строки
				wchar_t tidStr[10], pidStr[10];

				swprintf(tidStr, 10, L"%d", te.th32ThreadID);
				swprintf(pidStr, 10, L"%d", te.th32OwnerProcessID);

				// Готовим структуру для вставки нового элемента в список
				lvI.mask = LVIF_TEXT | LVIF_PARAM;
				lvI.iItem = ListView_GetItemCount(hListThreads);
				lvI.iSubItem = 0;
				lvI.pszText = tidStr;
				lvI.lParam = te.th32ThreadID;  // ❗ Храним TID, а не индекс строки!
				ListView_InsertItem(hListThreads, &lvI);

				// Добавляем дополнительный столбец с идентификатором процесса
				ListView_SetItemText(hList, lvI.iItem, 1, pidStr);
			}
		} while (Thread32Next(hSnap, &te));  // Переходим к следующему потоку
	}
	// Обновляем статус-бар с актуальным количеством потоков


	// Закрываем дескриптор снимка
	CloseHandle(hSnap);
	int threadCount = ListView_GetItemCount(hList);
	wchar_t threadCountStr[50];
	swprintf(threadCountStr, 50, L"Потоков: %d", threadCount);
	SendMessage(hStatusBar, SB_SETTEXT, 1, (LPARAM)threadCountStr);
}

// Функция инициализирует список процессов, добавляя необходимые колонки.
void InitProcessListView(HWND hWndListView)
{
	// Включаем режим выделения всей строки
	ListView_SetExtendedListViewStyle(hWndListView, LVS_EX_FULLROWSELECT);

	// Определяем параметры колонок
	LVCOLUMN lvc;
	lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;  // Маска для настройки текста, ширины и индекса колонки

	// Колонка с идентификаторами процессов
	lvc.cx = 110;           // Ширина колонки
	lvc.pszText = (LPWSTR)L"id";  // Название колонки
	ListView_InsertColumn(hWndListView, 0, &lvc);  // Вставка первой колонки

	// Колонка с именами процессов
	lvc.cx = 262;          // Ширина колонки
	lvc.pszText = (LPWSTR)L"Название";  // Название колонки
	ListView_InsertColumn(hWndListView, 1, &lvc);  // Вставка второй колонки

	// Колонка с количеством потоков
	lvc.cx = 110;          // Ширина колонки
	lvc.pszText = (LPWSTR)L"Потоков";  // Название колонки
	ListView_InsertColumn(hWndListView, 2, &lvc);  // Вставка третьей колонки
}

// Функция инициализирует список потоков, добавляя необходимые колонки.
void InitThreadListView(HWND hWndListView)
{
	// Включаем режим выделения всей строки
	ListView_SetExtendedListViewStyle(hWndListView, LVS_EX_FULLROWSELECT);

	// Определяем параметры колонок
	LVCOLUMN lvc;
	lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;  // Маска для настройки текста, ширины и индекса колонки

	// Колонка с идентификаторами потоков
	lvc.cx = 110;          // Ширина колонки
	lvc.pszText = (LPWSTR)L"id";  // Название колонки
	ListView_InsertColumn(hWndListView, 0, &lvc);  // Вставка первой колонки

	// Колонка с идентификаторами процессов
	lvc.cx = 172;           // Ширина колонки
	lvc.pszText = (LPWSTR)L"Процесс-родитель";  // Название колонки
	ListView_InsertColumn(hWndListView, 1, &lvc);  // Вставка второй колонки
}

void UpdateProcessCount() {
	DWORD processes[1024], needed;
	int count = 0;

	if (EnumProcesses(processes, sizeof(processes), &needed)) {
		count = needed / sizeof(DWORD);
	}

	wchar_t buffer[50];
	swprintf(buffer, 50, L"Процессов: %d", count);
	SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)buffer);

	// Получаем количество потоков перед обновлением статус-бара
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	int totalThreads = 0;

	if (hSnap != INVALID_HANDLE_VALUE) {
		THREADENTRY32 te;
		te.dwSize = sizeof(te);

		if (Thread32First(hSnap, &te)) {
			do {
				totalThreads++;
			} while (Thread32Next(hSnap, &te));
		}
		CloseHandle(hSnap);
	}
}

void UpdateTotalThreadCount() {
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	if (hSnap == INVALID_HANDLE_VALUE) return;

	THREADENTRY32 te;
	te.dwSize = sizeof(te);
	int totalThreads = 0;

	if (Thread32First(hSnap, &te)) {
		do {
			totalThreads++;
		} while (Thread32Next(hSnap, &te));
	}

	CloseHandle(hSnap);

	int threadCount = totalThreads;
	wchar_t threadCountStr[50];
	swprintf(threadCountStr, 50, L"Общ. потоков: %d", threadCount);
	SendMessage(hStatusBar, SB_SETTEXT, 2, (LPARAM)threadCountStr);
}


int ListView_FindItemByLParam(HWND hListView, LPARAM lParam) {
	LVFINDINFO findInfo = { 0 };
	findInfo.flags = LVFI_PARAM;
	findInfo.lParam = lParam;
	return ListView_FindItem(hListView, -1, &findInfo);
}


void EnableDebugPrivilege()
{
	HANDLE hToken;
	LUID luid;
	TOKEN_PRIVILEGES tkp;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
		{
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Luid = luid;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

			AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);
		}
		CloseHandle(hToken);
	}
}

bool IsSystemProcess(DWORD processID)
{
	wchar_t systemDir[MAX_PATH];
	GetSystemDirectory(systemDir, MAX_PATH); // Получаем системную директорию (обычно C:\Windows\System32)

	wchar_t wow64Dir[MAX_PATH];
	wcscpy_s(wow64Dir, systemDir);
	wcscat_s(wow64Dir, L"\\..\\SysWOW64"); // Получаем путь к C:\Windows\SysWOW64

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
	if (!hProcess) return false; // Нет доступа к процессу

	wchar_t processPath[MAX_PATH] = L"";
	DWORD size = MAX_PATH;
	bool isSystem = false;

	if (QueryFullProcessImageName(hProcess, 0, processPath, &size))
	{
		// Приводим к нижнему регистру для корректного сравнения
		_wcslwr_s(processPath, MAX_PATH);
		_wcslwr_s(systemDir, MAX_PATH);
		_wcslwr_s(wow64Dir, MAX_PATH);

		// Проверяем, начинается ли путь с System32 или SysWOW64
		if (wcsncmp(processPath, systemDir, wcslen(systemDir)) == 0 ||
			wcsncmp(processPath, wow64Dir, wcslen(wow64Dir)) == 0)
		{
			isSystem = true;
		}
	}

	CloseHandle(hProcess);
	return isSystem;
}

int CALLBACK CompareFuncProcesses(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	bool ascending = *(bool*)lParamSort;
	wchar_t text1[256], text2[256];

	// Получаем индекс строк
	int index1 = ListView_FindItemByLParam(hListProcesses, lParam1);
	int index2 = ListView_FindItemByLParam(hListProcesses, lParam2);

	ListView_GetItemText(hListProcesses, index1, sortColumnProcesses, text1, sizeof(text1));
	ListView_GetItemText(hListProcesses, index2, sortColumnProcesses, text2, sizeof(text2));

	int result = 0;

	// Числовая или строковая сортировка
	if (sortColumnProcesses == 0 || sortColumnProcesses == 2) {  // ID процесса / Количество потоков
		int num1 = _wtoi(text1);
		int num2 = _wtoi(text2);
		result = num1 - num2;
	}
	else {  // Имя процесса (строка)
		result = wcscmp(text1, text2);
	}

	return ascending ? result : -result;
}

int CALLBACK CompareFuncThreads(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	bool ascending = *(bool*)lParamSort;
	wchar_t text1[256], text2[256];

	int index1 = ListView_FindItemByLParam(hListThreads, lParam1);
	int index2 = ListView_FindItemByLParam(hListThreads, lParam2);

	ListView_GetItemText(hListThreads, index1, sortColumnThreads, text1, sizeof(text1));
	ListView_GetItemText(hListThreads, index2, sortColumnThreads, text2, sizeof(text2));

	int num1 = _wtoi(text1);
	int num2 = _wtoi(text2);
	int result = num1 - num2;

	return ascending ? result : -result;
}