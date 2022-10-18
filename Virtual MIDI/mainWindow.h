#pragma once

#ifndef   MAINWINDOW_H
#define   MAINWINDOW_H

#include <windows.h>

#define MAX_LOADSTRING 100

// Global Variables:
extern HINSTANCE hInst;								// current instance
extern HWND hMainWnd;
extern TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
extern TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

extern HWND hMainDlg;
													// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

#endif