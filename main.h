#ifndef _MAIN_H_
#define _MAIN_H_

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <richedit.h>
#include <mmsystem.h>

// Initialization & data structures

#pragma warning(disable:4200)

/* defines */
#define BTN_HIGHLIGHT RGB(192, 192, 255)
#define BTN_SHADOW RGB(98, 98, 255)
#define BTN_COLOR RGB(128, 128, 255)

#define WM_RESIZEVIEW (WM_USER+112)
#define WM_RESIZEFULL (WM_USER+113)

#define CON_NORMAL		0
#define CON_BLUE		1
#define CON_GREEN		2
#define CON_PURPLE		3
#define CON_RED			4
#define CON_YELLOW		5
#define CON_BROWN		6

#define IDC_MAIN_RICHEDIT 101
#define IDC_MAIN_REBAR	  102
#define IDC_MAIN_STATUS   103
#define IDR_MENUBAR1	  104

#define ID_FILE_NEW		  40001
#define ID_FILE_OPEN	  40002
#define ID_FILE_SAVEAS	  40003

#define EDIT_HEIGHT		20

extern HINSTANCE g_hInstance;
extern HWND g_hWnd;
extern HWND g_hWndMain;
extern HWND g_hToolBar;
extern HWND g_hEditBox;
extern HWND g_hComboBox;
extern char *g_pHost;

/* function definitions */
BOOL EntryPoint();
void CheckMessages();
INT_PTR CALLBACK DialogMessages(HWND hwnd,UINT uMsg,WPARAM wparam,LPARAM lparam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
void ToolBarButtonDropDown(HWND hwnd, LPNMHDR lpnm, LPNMTOOLBAR lpnmTB, int nIndex);
LRESULT CALLBACK EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ParseCommand(int argc, char **argv);
HFONT SetFont(char *myFont, int mySize);
void SetStatusBar(char *Text);
void SetPingStatus(int Ping);
void SetRichText(int datatype, char *buf, ...);
char *WindowsString(OSVERSIONINFO *osvi);
int MiscTimerRead(float *lpTime);
int CaptureAnImage(HWND hWnd, RECT rcClient);

#endif