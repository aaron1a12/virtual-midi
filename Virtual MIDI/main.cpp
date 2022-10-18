#include <Windows.h>
#include "Commctrl.h"
#include "Uxtheme.h"
#include <stdio.h>
#include <conio.h>
#include "teVirtualMIDI.h"
#include <mmsystem.h>
#include <string>
#include <map>
#include <array>
#include "resource.h"
#include "mainWindow.h"

using namespace std;

#define MAX_SYSEX_BUFFER	65535
#define VIRTUAL_DEVICE_NAME  Virtual MIDI Device
#define REGKEY_APP "Software\\Wild Montage\\Virtual MIDI"
wchar_t* virtualDeviceName = L"Virtual MIDI Device";

//HINSTANCE hInst;
HWND hMainDlg;

HWND hBtnStart;
HWND hBtnStop;
HWND hBtnTest;
HWND hMidiListView;
HWND hStatusListCtrl;
HWND hTestInputTxtBox;
HWND hBtnTestInput;
HWND hBtnHoldSus;
HWND hBtnIgnoreCC64;

BOOL bVirtualDevicePowered = false;

void log(LPCWSTR str);
void logf(const char *str);
void loadPreferences();
void savePreferences();

// Source MIDI
HMIDIIN hMidiDevice = NULL;;
void CALLBACK srcMidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD dwParam2);
BOOL bMonitorSource = false;
char sLastUsedDevice[128];
//char sLastUsedDevice[] = "SAMSON Graphite 49";

// Virtual MIDI
LPVM_MIDI_PORT pVirtualMidiPort;

// MIDI Filter / Options
map<int, int> sustainQueue;
BOOL bPedalPressed = false;
BOOL bIgnoreCC64 = true;


// Source MIDI

//array<string, 0> midiDevices = { {} };

void changeMIDISrc(DWORD nMidiDeviceId) {
	// Close previous device
	midiInStop(hMidiDevice);
	midiInClose(hMidiDevice);
	//DWORD nMidiPort = 0;

	// Open new device

	MMRESULT rv = midiInOpen(&hMidiDevice, nMidiDeviceId, (DWORD_PTR)(void*)srcMidiInProc, 0, CALLBACK_FUNCTION);
	if (rv != MMSYSERR_NOERROR) {
		char szBuff[64];
		sprintf(szBuff, "Unable to open MIDI device. Please make sure another application is not using it.\n\nrv=%d", rv);
		MessageBoxA(hMainDlg, szBuff, "Error", MB_ICONINFORMATION | MB_OK);
		ShowWindow(hMainDlg, SW_SHOW);
	}
	else {
		string sLogMsg = "\"";
		sLogMsg.append(sLastUsedDevice);
		sLogMsg.append("\" selected as source.");
		logf(sLogMsg.c_str());
		midiInStart(hMidiDevice);		
	}
}

void refreshMIDISrc() {
	log(L"Refreshing MIDI devices...");
	//ListView_DeleteAllItems(hMidiListView);
	SendMessage(hMidiListView, LVM_DELETEALLITEMS, 0, 0);

	UINT nMidiDeviceNum;
	MIDIINCAPS caps;

	nMidiDeviceNum = midiInGetNumDevs();
	if (nMidiDeviceNum == 0 && bVirtualDevicePowered == false ||
		nMidiDeviceNum == 1 && bVirtualDevicePowered == true) {
		logf("No MIDI devices found. Trying pluging one in.");
		return;
	}
	else {
		//array<string, nMidiDeviceNum> midiDevices = { {} };
		log(L"Found MIDI devices.");
	}

	LVITEM item;
	item.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
	item.pszText = "MIDI Device?";
	item.iSubItem = 0;
	item.state = 0;
	item.lParam = 1234;

	for (unsigned int i = 0; i < nMidiDeviceNum; ++i) {
		midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));

		item.pszText = caps.szPname;
		item.state = 0;
		item.lParam = i;

		// If the device name matches the last used name, switch to it.
		if (strcmp(caps.szPname, sLastUsedDevice) == 0) {
			changeMIDISrc(i);
			item.state = LVIS_SELECTED;
		}

		if (strcmp(caps.szPname, "Virtual MIDI Device") != 0) {
			SendMessage(hMidiListView, LVM_INSERTITEM, 0, (LPARAM)&item);
		}
	}
}

void log(LPCWSTR str) {
	SendMessageW(hStatusListCtrl, LB_INSERTSTRING, -1, (LPARAM)str);
	SendMessageW(hStatusListCtrl, WM_VSCROLL, SB_BOTTOM, 0);
}

void logf(const char *str) {
	char szBuff[64];
	sprintf(szBuff, str);

	SendMessageA(hStatusListCtrl, LB_INSERTSTRING, -1, (LPARAM)szBuff);
	SendMessageA(hStatusListCtrl, WM_VSCROLL, SB_BOTTOM, 0);
}

void logf(LPCWSTR str) {
	WCHAR szBuff[64];
	swprintf_s(szBuff, str);
	
	//MessageBox(hMainDlg, (LPCWSTR)szBuff, L"d", MB_OK);
	SendMessageW(hStatusListCtrl, LB_INSERTSTRING, -1, (LPARAM)szBuff);
	SendMessageW(hStatusListCtrl, WM_VSCROLL, SB_BOTTOM, 0);
}

char *binToStr(const unsigned char *data, DWORD length) {
	static char dumpBuffer[MAX_SYSEX_BUFFER * 3];
	DWORD index = 0;

	while (length--) {
		sprintf(dumpBuffer + index, "%02x", *data);
		if (length) {
			strcat(dumpBuffer, ":");
		}
		index += 3;
		data++;
	}
	return dumpBuffer;
}

void CALLBACK teVMCallback(LPVM_MIDI_PORT midiPort, LPBYTE midiDataBytes, DWORD length, DWORD_PTR dwCallbackInstance) {
	if ((NULL == midiDataBytes) || (0 == length)) {
		logf("Empty command - driver was probably shut down!\n");
		return;
	}
	if (!virtualMIDISendData(midiPort, midiDataBytes, length)) {
		logf("Error sending data: %d\n" + GetLastError());
		return;
	}
	logf("Command: %s\n");
	logf(binToStr(midiDataBytes, length));
}


void startMIDIDevice() {
	EnableWindow(hBtnStart, false);

	logf("Attempting to open virtual port...");
	logf("using dll-version:");
	logf(virtualMIDIGetVersion(NULL, NULL, NULL, NULL));
	logf("using driver-version:");
	logf(virtualMIDIGetDriverVersion(NULL, NULL, NULL, NULL));

	//virtualMIDILogging(TE_VM_LOGGING_MISC | TE_VM_LOGGING_RX | TE_VM_LOGGING_TX);

	pVirtualMidiPort = virtualMIDICreatePortEx2(virtualDeviceName, teVMCallback, 0, MAX_SYSEX_BUFFER, TE_VM_FLAGS_PARSE_RX);
	if (!pVirtualMidiPort) {
		logf("Error: could not create port: %d\n");
		logf(GetLastError());
		EnableWindow(hBtnStart, true);
		ShowWindow(hMainDlg, SW_SHOW);
		return;
	}

	std::wstring msg = L"Port \"";
	msg.append( virtualDeviceName );
	msg.append(L"\" created.");
	LPCWSTR msgTxt = msg.data();
	SendMessageW(hStatusListCtrl, LB_INSERTSTRING, -1, (LPARAM)msgTxt);
	SendMessageW(hStatusListCtrl, WM_VSCROLL, SB_BOTTOM, 0);

	bVirtualDevicePowered = true;
	EnableWindow(hBtnStop, true);
	EnableWindow(hBtnTest, true);
}



void CALLBACK srcMidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD dwParam2)
{
	switch (wMsg) {
	case MIM_OPEN:
		printf("wMsg=MIM_OPEN\n");
		break;
	case MIM_CLOSE:
		printf("wMsg=MIM_CLOSE\n");
		break;
	case MIM_DATA:
		{
			//printf("wMsg=MIM_DATA, dwInstance=%08x, dwParam1=%08x, dwParam2=%08x\n", dwInstance, dwParam1, dwParam2);
			//char szBuff[32];
			//sprintf(szBuff, "wMsg=MIM_DATA, dwInstance=%08x, dwParam1=%08x, dwParam2=%08x\n", dwInstance, dwParam1, dwParam2);

			DWORD midVelocity = HIWORD(dwParam1); // 0-127
			BYTE midNoteNumber = HIBYTE(dwParam1); // 60 = middle C
			BYTE midCommand = LOBYTE(dwParam1);

			BYTE firstDataByte = LOWORD(HIBYTE(dwParam1));
			BYTE secondDataByte = HIWORD(LOBYTE(dwParam1)); // Usually 0 or unused.

			if (bMonitorSource) {
				wchar_t monCmdBuffer[256];
				wsprintfW(monCmdBuffer, L"%d", midCommand);

				wstring monText = L"Cmd: ";
				monText.append(monCmdBuffer);

				array<string, 12> noteString = { { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" } };

				int octave = (midNoteNumber / 12) - 1;
				int noteIndex = (midNoteNumber % 12);

				string noteName = noteString[noteIndex].c_str();
				
				noteName.append( std::to_string(octave) );
				
				SendMessage(hTestInputTxtBox, WM_SETTEXT, 0, (LPARAM)noteName.c_str());
			}
		
			/*
			if (midCommand == 128 || midCommand == 144) {
				wchar_t buffer[256];
				wsprintfW(buffer, L"%d", midCommand);
				SendMessage(hTestInputTxtBox, WM_SETTEXT, 0, (LPARAM)buffer);
			}*/

			if (bPedalPressed && midCommand == 144) // note-on
			{
				// When a sustained note is played again, remove it
				if (sustainQueue.count((int)midNoteNumber)) { // if in sustain queue
					sustainQueue.erase((int)midNoteNumber);

					union { unsigned long word; unsigned char data[4]; } messageOff;
					messageOff.data[0] = 128;  // MIDI note-on message (requires to data bytes)
					messageOff.data[1] = midNoteNumber;    // MIDI note-on message: note number (60 = middle C)
					messageOff.data[2] = midVelocity;   // MIDI note-on message: note velocity (100 = loud)
					messageOff.data[3] = 0;     // Unused parameter

					virtualMIDISendData(pVirtualMidiPort, (LPBYTE)&messageOff, sizeof(messageOff));
				}
			}
			else if (bPedalPressed && midCommand == 128 ) // note-off
			{
				// Add to sustain queue
				sustainQueue[(int)midNoteNumber] = (int)midVelocity;
				break; // Do nothing so we can keep holding the notes!
			}
			
			// Pedal event
			if (midCommand == 176 && firstDataByte == 64)
			{
				if (midVelocity > 64) {
					bPedalPressed = true;
				}
				else {
					bPedalPressed = false;

					// Loop through the sustain queue and kill every note.
					map<int, int>::iterator i;
					for (i = sustainQueue.begin(); i != sustainQueue.end(); i++) {
						int noteNumber = i->first;
						int noteVelocity = i->second;

						union { unsigned long word; unsigned char data[4]; } messageOff;
						messageOff.data[0] = 128;  // MIDI note-on message (requires to data bytes)
						messageOff.data[1] = noteNumber;    // MIDI note-on message: note number (60 = middle C)
						messageOff.data[2] = noteVelocity;   // MIDI note-on message: note velocity (100 = loud)
						messageOff.data[3] = 0;     // Unused parameter

						virtualMIDISendData(pVirtualMidiPort, (LPBYTE)&messageOff, sizeof(messageOff));
					}

					sustainQueue.clear();
					//break;
				}

				if (bIgnoreCC64)
					break;
			}
				
			//
			// Send MIDI to DAW.
			//

			union { unsigned long word; unsigned char data[4]; } message;
			// message.data[0] = command byte of the MIDI message, for example: 0x90
			// message.data[1] = first data byte of the MIDI message, for example: 60
			// message.data[2] = second data byte of the MIDI message, for example 100
			// message.data[3] = not used for any MIDI messages, so set to 0
			//message.data[0] = 0x90;  // MIDI note-on message (requires to data bytes)
			message.data[0] = midCommand;  // MIDI note-on message (requires to data bytes)
			message.data[1] = midNoteNumber;    // MIDI note-on message: note number (60 = middle C)
			message.data[2] = midVelocity;   // MIDI note-on message: note velocity (100 = loud)
			message.data[3] = 0;     // Unused parameter

			virtualMIDISendData(pVirtualMidiPort, (LPBYTE)&message, sizeof(message));
		}
		break;
	case MIM_LONGDATA:
		printf("wMsg=MIM_LONGDATA\n");
		break;
	case MIM_ERROR:
		printf("wMsg=MIM_ERROR\n");
		break;
	case MIM_LONGERROR:
		printf("wMsg=MIM_LONGERROR\n");
		break;
	case MIM_MOREDATA:
		printf("wMsg=MIM_MOREDATA\n");
		break;
	default:
		printf("wMsg = unknown\n");
		break;
	}
	return;
}

// Message handler for our Main Window
INT_PTR CALLBACK MainWindow(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	int iIndex;

	switch (message)
	{
	case WM_INITDIALOG:
		if (strcmp(sLastUsedDevice, "") == 0) {
			ShowWindow(hDlg, SW_SHOW);
		}

		InitCommonControls(); 
		hMainDlg = hDlg;

		HICON hIcon;

		hIcon = (HICON)LoadImage(hInst,
			MAKEINTRESOURCE(IDI_MAIN_ICON),
			IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON),
			GetSystemMetrics(SM_CYSMICON),
			0);
		if (hIcon)
		{
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}

		hMidiListView = GetDlgItem(hDlg, IDC_MIDILIST);
		hStatusListCtrl = GetDlgItem(hDlg, IDC_LIST1);
		hBtnTestInput = GetDlgItem(hDlg, IDTESTSOURCE);
		hTestInputTxtBox = GetDlgItem(hDlg, IDC_TESTINPUT);
		hBtnStart = GetDlgItem(hDlg, IDSTART);
		hBtnStop = GetDlgItem(hDlg, IDSTOP);
		hBtnTest = GetDlgItem(hDlg, IDTEST);
		
		hBtnHoldSus = GetDlgItem(hDlg, IDC_HOLDSUS);
		hBtnIgnoreCC64 = GetDlgItem(hDlg, IDC_IGNORECC64);

		EnableWindow(hBtnHoldSus, false);

		SendMessage(hBtnHoldSus, BM_SETCHECK, bIgnoreCC64, 0);
		SendMessage(hBtnIgnoreCC64, BM_SETCHECK, bIgnoreCC64, 0);

		// Add icons to MIDI list
		
		HIMAGELIST hMidiIcons;
		hMidiIcons = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR32, 1, 0);
		ImageList_AddIcon(hMidiIcons, LoadIcon(hInst, MAKEINTRESOURCE(IDI_CONTROLLER)));
		ListView_SetImageList(hMidiListView, hMidiIcons, LVSIL_SMALL);
		ListView_SetExtendedListViewStyle(hMidiListView, LVS_EX_ONECLICKACTIVATE | LVS_EX_UNDERLINEHOT | LVS_EX_LABELTIP);
		
		SetWindowTheme(hTestInputTxtBox, L" ", L" ");

		// SetWindowTheme(hMidiListView, L"Explorer", NULL);

		// Start the virtual device
		startMIDIDevice();

		// Populate the source devices list
		refreshMIDISrc();

		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCLOSE || LOWORD(wParam) == IDCANCEL)
		{
			ShowWindow(hMainDlg, SW_HIDE);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDEXIT)
		{
			virtualMIDIClosePort(pVirtualMidiPort);
			savePreferences();
			EndDialog(hDlg, LOWORD(wParam));
			SendMessage(hMainWnd, WM_DESTROY, 0, 0);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDSTART)
		{
			startMIDIDevice();
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDSTOP)
		{
			virtualMIDIClosePort(pVirtualMidiPort);
			logf("Virtual port closed.\n");
			EnableWindow(hBtnStop, false);
			EnableWindow(hBtnStart, true);
			EnableWindow(hBtnTest, false);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDREFRESH)
		{
			refreshMIDISrc();
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDTESTSOURCE)
		{
			bMonitorSource = !bMonitorSource;

			SendMessage(hBtnTestInput, BM_SETCHECK, bMonitorSource, 0);
			return (INT_PTR)TRUE;
		}
		
		if (LOWORD(wParam) == IDTEST)
		{
			union { unsigned long word; unsigned char data[4]; } message;
			// message.data[0] = command byte of the MIDI message, for example: 0x90
			// message.data[1] = first data byte of the MIDI message, for example: 60
			// message.data[2] = second data byte of the MIDI message, for example 100
			// message.data[3] = not used for any MIDI messages, so set to 0
			message.data[0] = 0x90;  // MIDI note-on message (requires to data bytes)
			message.data[1] = 60;    // MIDI note-on message: Key number (60 = middle C)
			message.data[2] = 100;   // MIDI note-on message: Key velocity (100 = loud)
			message.data[3] = 0;     // Unused parameter

			virtualMIDISendData(pVirtualMidiPort, (LPBYTE)&message, sizeof(message));
			logf("Pressing middle-C on keyboard with 100 velocity.\n");
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_IGNORECC64)
		{
			bIgnoreCC64 = !bIgnoreCC64;

			SendMessage(hBtnIgnoreCC64, BM_SETCHECK, bIgnoreCC64, 0);
			//MessageBox(hMainDlg, L"Check?", L"Message", MB_OK);
			return (INT_PTR)TRUE;
		}
		break;

	case WM_DEVICECHANGE:
		refreshMIDISrc();
		break;

	case WM_NOTIFY:
		LPNMHDR lpnmHdr = (LPNMHDR)lParam;

		if (hMidiListView == lpnmHdr->hwndFrom)
		{
			switch (lpnmHdr->code)
			{
			case LVN_ITEMCHANGED:
				// lParam  contains our user-defined info!
				NMITEMACTIVATE itemAct = *(NMITEMACTIVATE*)lParam;

				CHAR szBuffer[1024];
				DWORD cchBuf(1024);
				LVITEM lvi;
				lvi.iItem = itemAct.iItem;
				lvi.iSubItem = 0;
				lvi.mask = LVIF_TEXT | LVIF_PARAM;
				lvi.pszText = szBuffer;
				lvi.cchTextMax = cchBuf;

				ListView_GetItem(lpnmHdr->hwndFrom, &lvi);

				if (strcmp(sLastUsedDevice, szBuffer) != 0) {
					strcpy(sLastUsedDevice, szBuffer);
					changeMIDISrc(lvi.lParam);
				}
				break;
			}
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void loadPreferences()
{
	HKEY hAppKey;
	LONG openRes = RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_ALL_ACCESS, &hAppKey);

	if (openRes != ERROR_SUCCESS) {
		// Preferences key does not exist so create it. Run time run?
		RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Wild Montage\\Virtual MIDI", 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hAppKey, NULL);
	}

	//
	// Preferences key exists so try to read it
	//
	else {
		// Open App key
		RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_READ, &hAppKey);

		// Read
		DWORD dwSize = sizeof(sLastUsedDevice);

		if (RegQueryValueEx(hAppKey, "MIDI Source", 0, NULL,
			(LPBYTE)sLastUsedDevice, &dwSize) == ERROR_SUCCESS) {
			//Studio.Username.assign(szUserBuffer);
		}

		RegCloseKey(hAppKey);
	}
}

void savePreferences()
{
	// Convert to Wide strings. Windows stores strings internally as wide.
	wchar_t* wsLastUsedDevice = new wchar_t[sizeof(sLastUsedDevice) + 1];
	swprintf(wsLastUsedDevice, sizeof(sLastUsedDevice) + 1, L"%hs", sLastUsedDevice);

	HKEY hAppKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_ALL_ACCESS, &hAppKey);
	RegSetValueExW(hAppKey, L"MIDI Source", 0, REG_SZ, (LPBYTE)wsLastUsedDevice, wcslen(wsLastUsedDevice) * sizeof(wchar_t));

	RegCloseKey(hAppKey);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Allow any process to set us as the foreground
	AllowSetForegroundWindow(ASFW_ANY);

	//
	// Create a shared file mapping in the memory.
	// If the mapping already exists, then exit the launcher.
	//

	HANDLE hMapFile = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, "VirtualMIDI123");

	if (hMapFile == NULL)
		return 1;
	else
		if (ERROR_ALREADY_EXISTS == GetLastError())
			return 2; // Exit

	loadPreferences();

	// Initialize global strings
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	// Create out main dialog
	hMainDlg = CreateDialog(hInstance,
		MAKEINTRESOURCE(IDD_MAINDLG),
		hMainDlg,
		MainWindow);

	//
	// Main message loop
	//

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!hMainDlg || !IsWindow(hMainDlg) || !IsDialogMessage(hMainDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

/*
//int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	hInst = hInstance;
	DialogBox(hInst, MAKEINTRESOURCE(IDD_MAINDLG), NULL, MainWindow);
}
*/