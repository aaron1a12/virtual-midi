#include <Windows.h>
#include "mainWindow.h"
#include "resource.h"

HINSTANCE hInst;
HWND hMainWnd;

// Callback used by the tray icon for events.
#define WMS_ICONNOTIFY		(WM_USER + 0x5103)
// Menu command id for exit in tray icon.
#define CMD_EXITAPP 1


UINT WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");

NOTIFYICONDATA niData = { sizeof(niData) };
void createTrayIcon()
{
	niData.uFlags = NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE | NIF_GUID | NIF_ICON;
	niData.uID = 1555;
	niData.hWnd = hMainWnd;
	//niData.hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_ICON1));
	niData.hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	niData.uCallbackMessage = WMS_ICONNOTIFY;

	//TODO: Use this method lstrcpyn(niData.szTip, L"Wild Montage Studio", sizeof(niData.szTip)/sizeof(TCHAR));
	lstrcpyn(niData.szTip, "Virtual MIDI", sizeof(niData.szTip) / sizeof(TCHAR));
	//LoadString(hInst, IDS_TRAYTIP, niData.szTip, 100);

	//niData.hIcon = (HICON)LoadImage( AppInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

	Shell_NotifyIcon(NIM_ADD, &niData);
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "VirtualMIDIClassV2";
	wcex.hIconSm = NULL;

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

					   // This window must be hidden
	hMainWnd = CreateWindow("VirtualMIDIClassV2", "VirtualMIDI", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 500, 500, NULL, NULL, hInstance, NULL);

	if (!hMainWnd)
	{
		return FALSE;
	}


	//ShowWindow(hMainWnd, nCmdShow);
	//UpdateWindow(hMainWnd);

	createTrayIcon();

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//int wmId, wmEvent;

	switch (message)
	{
	case WMS_ICONNOTIFY:
	{
		switch (lParam)
		{
		case WM_LBUTTONUP:
		{
			// Restore
			ShowWindow(hMainDlg, SW_SHOW);
		}
		break;
		case WM_RBUTTONUP:
		{
			POINT pt;
			GetCursorPos(&pt);

			HMENU hPopupMenu = CreatePopupMenu();
			InsertMenu(hPopupMenu, -1, MF_BYPOSITION | MF_STRING, CMD_EXITAPP, "Exit Virtual MIDI");
			SetForegroundWindow(hWnd);

			int cmd = TrackPopupMenu(hPopupMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);

			DestroyMenu(hPopupMenu);

			if (cmd == CMD_EXITAPP) {
				SendMessage(hMainDlg, WM_COMMAND, MAKEWPARAM(IDEXIT, 1), 0);

			}
		}
		break;
		}

		return 0;
	}
	case WM_DESTROY:
	{
		/*
		// Check if studio is running
		DWORD exitCode = 0;
		GetExitCodeProcess(hStudioProcess, &exitCode);

		// Terminate Studio. TODO: Notify Studio first?
		if (exitCode != 0)
			TerminateProcess(hStudioProcess, 0);
			*/
		// Delete tray icon
		Shell_NotifyIcon(NIM_DELETE, &niData);
		
		PostQuitMessage(0);
		break;
	}

	default:
		if (message == WM_TASKBARCREATED)
			createTrayIcon();

		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
