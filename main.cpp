#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#pragma comment(linker,"/FILEALIGN:0x200 /MERGE:.data=.text /MERGE:.rdata=.text /SECTION:.text,EWR")

#pragma comment (lib, "wsock32.lib")
#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "winmm.lib")

#include "main.h"
#include "list.h"
#include "tree.h"
#include "plugin.h"
#include "tcp.h"
#include "gui.h"
#include "shared.h"
#include "resource.h"

static bool bPerformanceTimerEnabled;
static _int64 PerformanceTimerFrequency;
static _int64 PerformanceTimerProgramStart;
static DWORD RegularTimerProgramStart;
static float TimerSecsPerTick;

// Global variables
HINSTANCE g_hInstance;
HWND g_hWnd;
HWND g_hWndMain;
HWND g_hToolBar;
HWND g_hEditBox;
HWND g_hComboBox;
char *g_pHost;

//Subclassed items
WNDPROC OrigEditWndProc;


BOOL EntryPoint()
{
	HWND hWnd;
	HDC	hDC;
	HFONT hFont;
	char filename[MAX_PATH + 1]; 

	GetModuleFileName((HMODULE)g_hWnd, filename, sizeof(filename));

	g_hWndMain = CreateDialog(GetModuleHandle(filename), MAKEINTRESOURCE(IDD_PROG), (HWND)0, DialogMessages);

	hWnd = g_hWndMain;

	if (!hWnd)
	{
		return FALSE;
	}

	hDC = GetDC(GetDesktopWindow());
	hFont = CreateFont(-MulDiv(24, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");
	SendDlgItemMessage(g_hWndMain, IDD_MAIN, WM_SETFONT, (WPARAM)hFont, TRUE);
	DeleteObject(hFont);

	ShowWindow(g_hWndMain, SW_SHOW);
	UpdateWindow(g_hWndMain);

	CheckMessages( );
	return TRUE;
}


//function to let windows check for messages
void CheckMessages()
{
   int i;

   for ( i = 0; i < 10; i++ )
   {
      MSG msg;

      //check the message queue, if there 
      //is a message in there, remove it
      //and dispatch it to wherever it needs to go
      if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
      {
         TranslateMessage( &msg );
         DispatchMessage ( &msg );
      }

      else
      {
         //if there are no other messages in the queue
         //then we can break out of this function
         break;
      }
   }
}

INT_PTR CALLBACK DialogMessages(HWND hwnd,UINT uMsg,WPARAM wparam,LPARAM lparam)
{
	switch(uMsg)
	{
	case WM_MAINSOCKET:
		{
			switch (lparam & 0xFFFF)
			{
			case FD_CLOSE:
				{
					DownloadCleanup(wparam);
					SetWindowText(g_hWndMain, "ghTcpSocket: Connection broken between socket and server.");
					gbConnected = false;
					break;
				}
			case FD_READ:
				{
					for (int i=0; i<ClientCount; i++)
						if (Clients[i].hSocket == wparam)
						{
							memset(Clients[i].SocketBuffer, 0, sizeof(Clients[i].SocketBuffer));
							int len = recv(wparam, Clients[i].SocketBuffer, sizeof(Clients[i].SocketBuffer), 0);
							if (len > 0)
							{
								RecvBufSize += len;
								RecvBuffer = (char *)realloc(RecvBuffer, RecvBufSize);
								memcpy(RecvBuffer + RecvBufSize - len, Clients[i].SocketBuffer, len);
								DownloadProcessQueue(i);
								//								SetRichText(CON_NORMAL, "Main (%i): %s\r\n", i, Clients[i].SocketBuffer);
							}
							break;
						}

				}
			case FD_WRITE:
				{
					int Len;

					for (int i=0; i<ClientCount; i++)
						if (Clients[i].hSocket == wparam)
						{
							if (SendBufSize > 0) {
								Len = send(Clients[i].hSocket, SendBuffer, SendBufSize, 0);
								if (Len > 0) {
									SendBufSize -= Len;
									memcpy(SendBuffer, SendBuffer + Len, SendBufSize);
									//memmove(SendBuffer, SendBuffer + Len, SendBufSize);
									//SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
								}
							}
							break;
						}
						break;

				}
			}
			return 0;
		}
		/* close dialog */
	case WM_CLOSE:
		//ShowWindow(g_hWndMain, SW_HIDE);
		EndDialog( hwnd, 0 );
		break;
	case WM_SYSCOMMAND:
		if (wparam == SC_CLOSE)
		{
			char *rdbuffer[1];
			rdbuffer[0] = "disconnect";
			DownloadParseCommand(1, rdbuffer);

			EndDialog(hwnd, 0);
			return TRUE;
		}
		break;
	}

	if ( uMsg == WM_COMMAND )
	{
		if ( HIWORD(wparam) == BN_CLICKED )
		{
			if (LOWORD(wparam) == IDOK)
			{
				SendStr("111", "0");
				SetRichText(CON_NORMAL, "http://127.0.0.1:8080/getVideo?width=640&fps=10&cap=D3D11&dev=%5C%5C.%5CDISPLAY1\r\n");

				SendDlgItemMessage( hwnd,IDC_LINE1,WM_SETTEXT,0,
										(LPARAM) "25%" );
				//advance the progress bar to 25%
				SendDlgItemMessage( hwnd, IDC_PROGRESS1, 
									PBM_SETPOS, 25, 0 );
				Sleep(500);
				CheckMessages( );

				SendDlgItemMessage( hwnd,IDC_LINE2,WM_SETTEXT,0,
										(LPARAM) "50%" );
				//advance the progress bar to 50%
				SendDlgItemMessage( hwnd, IDC_PROGRESS1, 
									PBM_SETPOS, 50, 0 );
				Sleep(500);
				CheckMessages( );
	      		
				SendDlgItemMessage( hwnd,IDC_LINE3,WM_SETTEXT,0,
										(LPARAM) "75%" );
				//advance the progress bar to 75%
				SendDlgItemMessage( hwnd, IDC_PROGRESS1, 
									PBM_SETPOS, 75, 0 );
				Sleep(500);
				CheckMessages( );
	      		
				SendDlgItemMessage( hwnd,IDC_LINE4,WM_SETTEXT,0,
										(LPARAM) "100%" );
				//advance the progress bar to 100%
				SendDlgItemMessage( hwnd, IDC_PROGRESS1, 
									PBM_SETPOS, 100, 0 );
				Sleep(500);
				CheckMessages( );

				EndDialog( hwnd, 0 );
				PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
				return TRUE;
			}

			else if (LOWORD(wparam) == IDCANCEL)
			{
				EndDialog( hwnd, 0 );
				PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
				return TRUE;
			}
		}
	}
	return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Processing = false;
	RecvBufSize = 0;
	SendBufSize = 0;
	RecvBuffer = NULL;
	SendBuffer = NULL;

	// Initialize global variables
	g_hInstance = GetModuleHandle(0);
	g_bConnected = false;

	QueryPerformanceFrequency(&g_Frequency);

	static char szClassName[] = "Console";
	static char szWindowName[] = "Console";

	// Define & register window class
	WNDCLASSEX WndClass;
	MSG msg;

	memset(&WndClass, 0, sizeof(WndClass));
	WndClass.cbSize = sizeof(WndClass);						// size of structure
	WndClass.lpszClassName = szClassName;					// name of window class
	WndClass.lpfnWndProc = WndProc;							// points to a window procedure
	WndClass.hInstance = g_hInstance;						// handle to instance
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);		// predefined app. icon
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);		// small class icon
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);			// predefined arrow
	WndClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);	// background brush
	WndClass.lpszMenuName = NULL;							// name of menu resource

	if (!RegisterClassEx(&WndClass))
	{
		return 0;
	}

	// Create actual window
	g_hWnd = CreateWindowEx(WS_EX_WINDOWEDGE, szClassName, szWindowName, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 300, 0, 0, g_hInstance, 0);

	if (!g_hWnd)
	{
		return 0;
	}

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	WinTree(NULL, NULL, NULL, SW_HIDE);
	WinList(NULL, NULL, NULL, SW_HIDE);
	WinRemoteDesktop(NULL, NULL, NULL, SW_HIDE);

	HWND hRichEdit;
	hRichEdit = GetDlgItem(g_hWnd, IDC_MAIN_RICHEDIT);

	SendMessage(hRichEdit, WM_SETFONT, (WPARAM)SetFont("Sans", 9), MAKELPARAM(FALSE, 0));

	SetRichText(CON_BLUE, "W");
	SetRichText(CON_GREEN, "I");
	SetRichText(CON_PURPLE, "Z");
	SetRichText(CON_RED, "A");
	SetRichText(CON_YELLOW, "R");
	SetRichText(CON_BROWN, "D");
	SetRichText(CON_NORMAL, " Console v1.00 alpha\r\n\r\n");

	WSADATA WSAData;
	int i;

	if (WSAStartup(MAKEWORD(2, 2), &WSAData) == 0)
	{
		SetRichText(CON_NORMAL, "Winsock v%d.%d initialized. Provider description: %s\r\n", 
			HIBYTE(WSAData.wVersion), LOBYTE(WSAData.wVersion), WSAData.szDescription);
	}
	else
	{
		SetRichText(CON_RED, "Winsock initialization failed (%d).\r\n", WSAGetLastError());
	}

	g_hUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (g_hUdpSocket != SOCKET_ERROR)
	{
		WSAAsyncSelect(g_hUdpSocket, g_hWnd, WM_QUERYSOCKET, FD_READ);

		struct sockaddr_in usin;

		usin.sin_family = AF_INET;
		usin.sin_port = 0;
		usin.sin_addr.S_un.S_addr = 0;

		if (bind(g_hUdpSocket, (struct sockaddr*)&usin, sizeof(usin)) == SOCKET_ERROR)
		{
			closesocket(g_hUdpSocket);
			g_hUdpSocket = INVALID_SOCKET;
			SetRichText(CON_RED, "Unable to create UDP query socket (%d).\r\n", WSAGetLastError());
		}
		else
		{
            i = sizeof(usin);
			getsockname(g_hUdpSocket, (struct sockaddr *)&usin, &i);
			SetRichText(CON_NORMAL, "UDP query socket created on port %d.\r\n", htons(usin.sin_port));
		}
	}
	else
	{
		SetRichText(CON_RED, "Unable to create UDP query socket (%d).\r\n", WSAGetLastError());
	}

    LoadPlugins();

	OSVERSIONINFO g_VersionInfo;

	g_VersionInfo.dwOSVersionInfoSize = sizeof(g_VersionInfo);
	GetVersionEx(&g_VersionInfo);

	SetRichText(CON_BLUE, "Windows version... ");
	SetRichText(CON_NORMAL, "%s\r\n", WindowsString(&g_VersionInfo));

	char g_szConsoleFile[MAX_PATH];
	char g_szConsolePath[MAX_PATH];
	char g_szSettingsFile[MAX_PATH];
	char g_szWindowsPath[MAX_PATH];

	// Get all the paths we need

	GetModuleFileName(g_hInstance, g_szConsoleFile, sizeof(g_szConsoleFile));
	GetWindowsDirectory(g_szWindowsPath, sizeof(g_szWindowsPath));
	strcat(g_szWindowsPath, "\\");

	strcpy(g_szConsolePath, g_szConsoleFile);
	for (int i = (int)strlen(g_szConsolePath) - 1; i > 0; i--)
	{
		if (g_szConsolePath[i] == '\\')
		{
			g_szConsolePath[i + 1] = 0;
			break;
		}
	}

	strcpy(g_szSettingsFile, g_szConsolePath);
	strcat(g_szSettingsFile, "Settings.ini"); //add into own dir

	SetRichText(CON_BLUE, "Windows directory... ");
	SetRichText(CON_NORMAL, "%s\r\n", g_szWindowsPath);

	//TCPConnect("127.0.0.1:80");

	while( GetMessage(&msg, NULL, 0, 0) )
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	//Cleanup
	WSACleanup();

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_MAINSOCKET:
		{
			switch (lParam & 0xFFFF)
			{
			case FD_READ:
				{
					char buf[4096];
					memset(&buf, 0, sizeof(buf));

                    int len = recv(g_hTcpSocket, buf, sizeof(buf), 0);
					if (len > 0)
					{
						RecvBufSize += len;
						RecvBuffer = (char *)realloc(RecvBuffer, RecvBufSize);
						memcpy(RecvBuffer + RecvBufSize - len, buf, len);
						ProcessQueue();

						//ProcessBuffer(buf);
						//SetRichText(CON_NORMAL, "Main: %s\r\n", buf);
					}
					break;
				}
			case FD_CLOSE:
				{
					closesocket(g_hTcpSocket);
					g_bConnected = false;
					SetRichText(CON_NORMAL, "g_hTcpSocket: Connection broken between socket and server.\r\n");
					break;
				}
			case FD_WRITE:
				{
					if (SendBufSize > 0) {
						int len = send(g_hTcpSocket, SendBuffer, SendBufSize, 0);
						if (len > 0) {
							SendBufSize -= len;
							//memcpy(Clients[i]->SendBuffer, Clients[i]->SendBuffer + Len, Clients[i]->SendBufSize);
							memmove(SendBuffer, SendBuffer + len, SendBufSize);
							SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
						}
					}
					break;
				}
				break;
			}
			return 0;
		}


	case WM_QUERYSOCKET:
		{
			switch (lParam & 0xFFFF)
			{
			case FD_READ:
				{
					char buf[1024];
					memset(&buf, 0, sizeof(buf));
					SOCKADDR_IN sin;
					int bytes_read, i;

					i = sizeof(sin);
					bytes_read = recvfrom(g_hUdpSocket, buf, sizeof(buf), 0, (SOCKADDR *)&sin, &i);
					if (bytes_read != SOCKET_ERROR)
					{
						SetRichText(CON_NORMAL, "Query: %s\r\n", buf);
					}
					break;
				}
			}
			return 0;
		}


		case WM_CREATE:
		{
			HMODULE hLibRichEdit;

			hLibRichEdit = LoadLibrary("RICHED20.DLL");

			if (!hLibRichEdit)
			{
				MessageBox(hWnd, "failed to load RICHED20.DLL", "error", MB_OK | MB_ICONERROR);
				return -1;
			}

			INITCOMMONCONTROLSEX icex;

			icex.dwSize = sizeof(icex);
			icex.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_USEREX_CLASSES;

			InitCommonControlsEx(&icex);

			// Create RichEdit Control
			HFONT hfDefault;
			HWND hRichEdit;

			hRichEdit = CreateWindowEx(WS_EX_CLIENTEDGE, RICHEDIT_CLASS, 0,
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
				/*ES_READONLY |*/ ES_NOHIDESEL, 0, 0, 100, 100, hWnd, (HMENU)IDC_MAIN_RICHEDIT, GetModuleHandle(NULL), NULL);

			if (hRichEdit == NULL)
				MessageBox(hWnd, "failed to create richedit control", "error", MB_OK | MB_ICONERROR);

			hfDefault = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessage(hRichEdit, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE, 0));

			// Create Editbox Control
			g_hEditBox = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", 0,
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
				0, 207, 492, EDIT_HEIGHT, hWnd, 0, g_hInstance, 0);

			SendMessage(g_hEditBox, WM_SETFONT, (WPARAM)SetFont("Tahoma", 8), TRUE);

			// Subclass Editbox
			OrigEditWndProc = (WNDPROC)SetWindowLong(g_hEditBox, GWL_WNDPROC, (DWORD)EditWndProc);

			// Create Rebar Control
			REBARINFO rbi;
			REBARBANDINFO rbbInfo;

			TBBUTTON tbbStruct[10];
			UINT dImages[] = { 0, 1, 2, -1, 3, 4, -1, 5, -1, 6 };

			COMBOBOXEXITEM cbeItem;
			LPSTR pLabels[] = { "Desktop","Network","3½ Floppy (A:)","HDD (C:)","Control Spy","CDROM (D:)","Printers","Control Panel","Network","Recycle Bin" };
			UINT dIndent[] = { 0, 1, 2, 2, 3, 2, 2, 2, 1, 1 };

			RECT rcControl;
			HBITMAP hBitmap;
			int dX;

			HIMAGELIST g_hToolbarIL = NULL;
			HIMAGELIST g_hToolbarHotIL = NULL;
			HIMAGELIST g_hComboBoxExIL = NULL;


			HWND hRebar;

			hRebar = CreateWindowEx(0, REBARCLASSNAME, NULL,
				WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
				CCS_ADJUSTABLE | RBS_VARHEIGHT | RBS_BANDBORDERS, 0, 0, 0, 0,
				hWnd, (HMENU)IDC_MAIN_REBAR, GetModuleHandle(NULL), NULL);

			memset(&rbi, 0, sizeof(rbi));
			rbi.cbSize = sizeof(rbi);
			rbi.fMask = 0;
			rbi.himl = (HIMAGELIST)0;
			
			SendMessage(hRebar, RB_SETBARINFO, 0, (LPARAM)&rbi);

			// Toolbar child
            //HWND hToolBar;

			g_hToolBar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
				WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | CCS_NORESIZE | CCS_NODIVIDER,
				0, 0, 0, 0, hRebar, (HMENU)IDR_MENUBAR1, GetModuleHandle(NULL), NULL);

			// Since created toolbar with CreateWindowEx, specify the button structure size
			// This step is not required if CreateToolbarEx was used
			SendMessage(g_hToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

			// Create and initialize image list used for child toolbar
			g_hToolbarIL = ImageList_Create(20, 20, ILC_COLORDDB | ILC_MASK, 7, 1);
			hBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_INTERFACE));
			ImageList_AddMasked(g_hToolbarIL, hBitmap, RGB(255, 0, 255));
			DeleteObject(hBitmap);

			// Create and initialize hot image list used for child toolbar
			g_hToolbarHotIL = ImageList_Create(20, 20, ILC_COLORDDB | ILC_MASK, 7, 1);
			hBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_INTERFACEHOT));
			ImageList_AddMasked(g_hToolbarHotIL, hBitmap, RGB(255, 0, 255));
			DeleteObject(hBitmap);

			// Add image lists to toolbar
			SendMessage(g_hToolBar, TB_SETIMAGELIST, 0,(LPARAM)g_hToolbarIL);
			SendMessage(g_hToolBar, TB_SETHOTIMAGELIST, 0,(LPARAM)g_hToolbarHotIL);

			// Set toolbar padding
			SendMessage(g_hToolBar, TB_SETINDENT, 2, 0);

			// Add buttons to control
			for(dX=0;dX<10;dX++)
			{
				// Add button
				if(dImages[dX] != -1)
				{
					tbbStruct[dX].iBitmap = dImages[dX];
					tbbStruct[dX].fsStyle = TBSTYLE_BUTTON;
					if(dX == 0)
					{
						tbbStruct[dX].fsStyle = TBSTYLE_DROPDOWN;
					}
				}
				else
				{
					tbbStruct[dX].iBitmap = 0;
					tbbStruct[dX].fsStyle = TBSTYLE_SEP;
				}
				tbbStruct[dX].fsState = TBSTATE_ENABLED;
				tbbStruct[dX].idCommand = 40001 + dX;
				tbbStruct[dX].dwData = 0;
				tbbStruct[dX].iString = -1;
			}

			SendMessage(g_hToolBar, TB_SETEXTENDEDSTYLE, 0, (LPARAM)TBSTYLE_EX_DRAWDDARROWS);
			SendMessage(g_hToolBar, TB_ADDBUTTONS, 10, (LPARAM)tbbStruct);

			// Add Rebar band

			rbbInfo.cbSize = sizeof(REBARBANDINFO);

			rbbInfo.fMask = RBBIM_STYLE|RBBIM_CHILD|RBBIM_CHILDSIZE|RBBIM_SIZE;
			rbbInfo.fStyle = RBBS_GRIPPERALWAYS|RBBS_CHILDEDGE;
			rbbInfo.hwndChild = g_hToolBar;
			rbbInfo.cxMinChild = 0;
			rbbInfo.cyMinChild = HIWORD(SendMessage(g_hToolBar,TB_GETBUTTONSIZE,0,0));
			rbbInfo.cx = 200;

			SendMessage(hRebar,RB_INSERTBAND,-1,(LPARAM)&rbbInfo);

			// COMBOBOXEX

			//HWND hComboBoxEx;

			g_hComboBox = CreateWindowEx(0, WC_COMBOBOXEX, NULL,
				WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, //CBS_DROPDOWNLIST,
				0, 0, 300, 150, hRebar, NULL, GetModuleHandle(NULL), NULL);

			// Create and initialize image list used for child ComboBoxEx
			g_hComboBoxExIL = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 10, 1);
			hBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_COMPUTER));
			ImageList_AddMasked(g_hComboBoxExIL, hBitmap, RGB(255, 0, 255));
			DeleteObject(hBitmap);

			// Assign image list to control
			SendMessage(g_hComboBox, CBEM_SETIMAGELIST, 0, (LPARAM)g_hComboBoxExIL);

			// Add items to control
			for(dX=0;dX<10;dX++)
			{
				cbeItem.mask = CBEIF_IMAGE|CBEIF_SELECTEDIMAGE|CBEIF_TEXT|CBEIF_INDENT;
				cbeItem.iItem = -1;
				cbeItem.pszText = pLabels[dX];
				cbeItem.cchTextMax = (int)strlen(pLabels[dX]);
				cbeItem.iImage = dX;
				cbeItem.iSelectedImage = dX;
				cbeItem.iIndent = dIndent[dX];

				SendMessage(g_hComboBox, CBEM_INSERTITEM, 0, (LPARAM)&cbeItem);
			}

			// Select first item
			SendMessage(g_hComboBox, CB_SETCURSEL, 4, 0);

			static char szText[] = "127.0.0.1:80";

			SendMessage(g_hComboBox, CB_ADDSTRING, 0, (LPARAM)(char *)szText);
			SendMessage(g_hComboBox, WM_SETTEXT, 0, (LPARAM)(char *)szText);

			// Add Rebar band

			rbbInfo.cbSize = sizeof(REBARBANDINFO);
			rbbInfo.fMask = RBBIM_STYLE|RBBIM_CHILD|RBBIM_TEXT|RBBIM_CHILDSIZE|RBBIM_SIZE;
			rbbInfo.fStyle = RBBS_GRIPPERALWAYS|RBBS_CHILDEDGE;
			rbbInfo.hwndChild = g_hComboBox;
			GetWindowRect(g_hComboBox, &rcControl);
			rbbInfo.cxMinChild = 0;
			rbbInfo.cyMinChild = rcControl.bottom-rcControl.top;
			rbbInfo.lpText = "Address";
			rbbInfo.cx = 200;

			SendMessage(hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbInfo);


			// Create StatusBar Control
			HWND hStatus;
			int statwidths[] = {100, -1};

			hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
				hWnd, (HMENU)IDC_MAIN_STATUS, GetModuleHandle(NULL), NULL);

			SendMessage(hStatus, SB_SETPARTS, sizeof(statwidths)/sizeof(int), (LPARAM)statwidths);
			SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)"0 ms");
			SendMessage(hStatus, SB_SETTEXT, 1, (LPARAM)"Console Ready...");
		}
		break;
		case WM_SIZE:
		{
			HWND hTool;
			RECT rcTool;
			int iToolHeight;

			HWND hStatus;
			RECT rcStatus;
			int iStatusHeight;

			HWND hRichEdit;
			int iRichEditHeight;
			RECT rcClient;

			// Size toolbar and get height

			hTool = GetDlgItem(hWnd, IDC_MAIN_REBAR);

			GetWindowRect(hTool, &rcTool);
			iToolHeight = rcTool.bottom - rcTool.top;

			// Resize rebar
			MoveWindow(hTool, 0, 0, LOWORD(lParam), iToolHeight, FALSE);

			// Size statusbar and get height

			hStatus = GetDlgItem(hWnd, IDC_MAIN_STATUS);
			SendMessage(hStatus, WM_SIZE, 0, 0);

			GetWindowRect(hStatus, &rcStatus);
			iStatusHeight = rcStatus.bottom - rcStatus.top;

			// Calculate the remaining height and size richedit

			GetClientRect(hWnd, &rcClient);

			iRichEditHeight = rcClient.bottom - iToolHeight - iStatusHeight - EDIT_HEIGHT;

			hRichEdit = GetDlgItem(hWnd, IDC_MAIN_RICHEDIT);
			SetWindowPos(hRichEdit, NULL, 0, iToolHeight, rcClient.right, iRichEditHeight, SWP_NOZORDER);

			// Resize editbox

			MoveWindow(g_hEditBox, 0, iToolHeight + iRichEditHeight, LOWORD(lParam), EDIT_HEIGHT, FALSE);

			InvalidateRect(hWnd, 0, FALSE);
		}
		break;
		case WM_CLOSE:
			DestroyWindow(hWnd);
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		case WM_NOTIFY:
		{
			if (((NMHDR *)lParam)->code == RBN_HEIGHTCHANGE)
			{
				// Rebar height has changed, realign other controls

				RECT rc;
				GetClientRect(g_hWnd, &rc);
				PostMessage(g_hWnd, WM_SIZE, 0, ((rc.bottom - rc.top) << 16) | (rc.right - rc.left));
				
				return 0;
			}
			switch (((LPNMHDR)lParam)->code)
			{
				case TBN_DROPDOWN:
				{
					LPNMHDR lpnm = ((LPNMHDR)lParam);
					LPNMTOOLBAR lpnmTB = ((LPNMTOOLBAR)lParam);

					switch(lpnmTB->iItem)
					{
						case ID_FILE_NEW:
						{
							ToolBarButtonDropDown(hWnd, lpnm, lpnmTB, 0);
						}break;
					}
				}break;
			}
		}
		break;
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case ID_FILE_EXIT:
				{
					PostQuitMessage(0);
					return 0;
				}
				case ID_FILE_NEW:
					//MessageBox(hWnd, "ID_FILE_NEW", "info", MB_OK | MB_ICONINFORMATION);
					//WinTree(NULL, NULL, NULL, SW_SHOWNORMAL);
					ShowWindow(g_hWndTree, SW_SHOW);
				break;
				case ID_FILE_OPEN:
					//MessageBox(hWnd, "ID_FILE_OPEN", "info", MB_OK | MB_ICONINFORMATION);
					ShowWindow(g_hWndList, SW_SHOW);
				break;
				case ID_FILE_SAVEAS:
					char *buffer[1];
					buffer[0] = "connect";
					ParseCommand(1, buffer);
				break;
				case ID_FILE_SAVEAS + 2:
					//MessageBox(hWnd, "ID_FILE_SAVEAS + 2", "info", MB_OK | MB_ICONINFORMATION);
					//structure
					INITCOMMONCONTROLSEX controls;

					//Set of bit flags that indicate which 
					//common control classes will be loaded
					controls.dwICC = ICC_PROGRESS_CLASS;

					//Size of the structure, in bytes.
					controls.dwSize = sizeof(INITCOMMONCONTROLSEX);

					//function
					//tells windows to load any controls specified in dwICC
					InitCommonControlsEx(&controls);

					DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PROG), 
                                          NULL, DialogMessages);
				break;
				case ID_FILE_SAVEAS + 3:
					//EntryPoint();
					ShowWindow(ghWnd, SW_SHOW);
					char *rdbuffer[1];
					rdbuffer[0] = "connect";
					RemoteDesktopParseCommand(1, rdbuffer);
					ShowWindow(ghWnd, SW_SHOW);
				break;
				case ID_FILE_SAVEAS + 5:
					SendStr("302", "0");
				break;
				case ID_FILE_SAVEAS + 7:
					SendStr("304", "0");
					break;
			}
		}
		break;
		case WM_GETMINMAXINFO:
		{
			// Set window size constraints
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = 500;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = 300;
		}
		break;
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

void ToolBarButtonDropDown(HWND hwnd, LPNMHDR lpnm, LPNMTOOLBAR lpnmTB, int nIndex)
{
	RECT rc;
	TPMPARAMS tpm;
	HMENU hPopupMenu = NULL;

	SendMessage(lpnmTB->hdr.hwndFrom, TB_GETRECT, (WPARAM)lpnmTB->iItem, (LPARAM)&rc);

	MapWindowPoints(lpnmTB->hdr.hwndFrom, HWND_DESKTOP, (LPPOINT)&rc, 2);

	tpm.cbSize = sizeof(TPMPARAMS);
	tpm.rcExclude = rc;
	hPopupMenu = GetSubMenu(LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU1)), nIndex);

	TrackPopupMenuEx(hPopupMenu, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_VERTICAL, rc.left, rc.bottom, hwnd, &tpm);
}

LRESULT CALLBACK EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_KEYDOWN:
		{
			switch(wParam)
			{
			case VK_RETURN:
				{
					//Get text
					int length = (int)SendMessage(g_hEditBox, WM_GETTEXTLENGTH, 0, 0) + 1;
					if (length > 1)
					{
						char *buffer = (char*)malloc(sizeof(char) * length);
						SendMessage(g_hEditBox, WM_GETTEXT, (WPARAM)length, (LPARAM)buffer);
						SetRichText(CON_NORMAL, "%s\r\n", buffer);
						SendMessage(g_hEditBox, WM_SETTEXT, 0, 0);

						char *token;
						int x = 0;
						char **argv = 0;
						int argc = 0;

						token = strtok(buffer, "()");
						while(token) {
							for(x = lstrlen(token) - 1; token[x] == ' ' || token[x] == '\n'; --x);
								token[x+1] = '\0';
							if (lstrlen(token) > 0) {
								argc++;
								argv = (char **)realloc(argv, sizeof(argv) * argc);
								argv[argc - 1] = token;
								//MessageBox(0, strquotes(token), token, 0);
							}
							token = strtok(0, "()");
						}

						ParseCommand(argc, argv);

						//for(int i=0; i<argc; i++)
						//MessageBox(0, argv[i], argv[i], 0);

						free(argv);
						free(buffer);
					}

					return 0;
				}
				
			}
			break;
		}
	}
	return CallWindowProc(OrigEditWndProc, hWnd, uMsg, wParam, lParam);
}

void ParseCommand(int argc, char **argv)
{
	if (stricmp(argv[0], "connect") == 0)
	{
		char *pos;
		int port;

		if (g_bConnected == false)
		{
			if (argc == 2)
			{
				pos = strchr(argv[1], ':');
				if (pos != NULL)
				{
					port = atoi(pos + 1);
					g_pHost = strtok(argv[1], ":");
				}
				else
					SetRichText(CON_RED, "Syntax error.\r\n");
			}
			else if (argc == 1)
			{
				int length = (int)SendMessage(g_hComboBox, WM_GETTEXTLENGTH, 0, 0) + 1;
				if (length > 1)
				{
					char *buffer = (char*)malloc(sizeof(char) * length);
					SendMessage(g_hComboBox, WM_GETTEXT, (WPARAM)length, (LPARAM)buffer);

					pos = strchr(buffer, ':');
					if (pos != NULL)
					{
						port = atoi(pos + 1);
						g_pHost = strtok(buffer, ":");
					}
					else
						SetRichText(CON_RED, "Syntax error.\r\n");
				}
				else
					SetRichText(CON_RED, "Syntax error.\r\n");
			}
			else
				SetRichText(CON_RED, "Syntax error.\r\n");

			struct sockaddr_in sin;
			struct hostent *host;

			sin.sin_family = AF_INET;
			sin.sin_port = htons((unsigned short)port);
			sin.sin_addr.S_un.S_addr = inet_addr(g_pHost);

			//resolve hostname if not ip
			if (sin.sin_addr.S_un.S_addr == INADDR_NONE)
			{
				host = gethostbyname(g_pHost);
				if (host != NULL)
					memcpy(&sin.sin_addr.S_un.S_addr, host->h_addr_list[0], host->h_length);

			}

			g_hTcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (g_hTcpSocket != INVALID_SOCKET)
			{
	            if (connect(g_hTcpSocket, (struct sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
				{
					closesocket(g_hTcpSocket);
					SetRichText(CON_RED, "TCP connect failed (%d).\r\n", WSAGetLastError());
					g_bConnected = false;
				}
				else
				{
					SetRichText(CON_NORMAL, "Successfully connected to %s on port %d.\r\n", inet_ntoa(sin.sin_addr), port);
					g_bConnected = true;

					WSAAsyncSelect(g_hTcpSocket, g_hWnd, WM_MAINSOCKET, FD_READ | FD_CLOSE);
				}
			}
		}
		else
			SetRichText(CON_RED, "Error: Socket already connected.\r\n");
	}

	else if (stricmp(argv[0], "disconnect") == 0)
	{
		if (!g_bConnected)
		{
			SetRichText(CON_RED, "Cannot disconnect without connecting.\r\n");
		}
		else
		{
			closesocket(g_hTcpSocket);
			g_bConnected = false;
			SetRichText(CON_NORMAL, "Successfully disconnected from server.\r\n");
		}
	}

	else if ((stricmp(argv[0], "send") == 0) && (argc == 2))
	{
		if (!g_bConnected)
		{
			SetRichText(CON_RED, "Cannot send without connecting.\r\n");
		}
		else
		{
			if (send(g_hTcpSocket, argv[1], lstrlen(argv[1]), 0) == SOCKET_ERROR)
				SetRichText(CON_RED, "Unknown error. Failed to send data.\r\n");
		}
	}

	else if ((stricmp(argv[0], "sendstr") == 0) && (argc >= 3))
	{
		if (!g_bConnected)
		{
			SetRichText(CON_RED, "Cannot send without connecting.\r\n");
		}
		else
		{
			if (stricmp(argv[1], "128") == 0)
			{
				char *temp;
				int i = 0;
				int j = 0;
				//sendstr (128)(C:\Documents and Settings\Owner\My Documents\Downloads\Henry.Poole.Is.Here.DVDRip.XviD-ARROW\Sample\arw-hpih.dvdrip.xvid.sample.avi)
				temp = (char *)calloc(lstrlen(argv[2]), sizeof(char));
				for (i=lstrlen(argv[2]); i>0; i--)
					if (argv[2][i] == '\\')
						break;
				for (j=i+1; j<lstrlen(argv[2]); j++)
					strncat(temp, &argv[2][j], 1);

				LocalFileName = temp;
			}

			SendStr(argv[1], argv[2]);
		}
	}

	else if ((stricmp(argv[0], "sendmsg") == 0))
	{
		for (int i=0; i<PluginCount; i++)
		{
			if (stricmp(argv[1], Plugins[i].Name) == 0)
			{
				unsigned long argl;
				SendPluginMessage(Plugins[i].Name, atoi(argv[2]), argc, argv, &argl, i);
				break;
			}
		}
	}

	else if ((stricmp(argv[0], "download") == 0) && (argc == 2))
	{
		if (g_bTransferring)
		{
			SetRichText(CON_RED, "Cannot download without connecting.\r\n");
		}
		else
		{
			char buf[256];
			char *temp;
			int i = 0;
			int j = 0;
			//download ("C:\Documents and Settings\Administrator\Desktop\Client.zip")
			temp = (char *)calloc(lstrlen(argv[1]), sizeof(char));
			for (i=lstrlen(argv[1]); i>0; i--)
				if (argv[1][i] == '\\')
					break;
			for (j=i+1; j<lstrlen(argv[1])-1; j++)
				strncat(temp, &argv[1][j], 1);

			LocalFileName = temp;

			Sleep(1000);

			wsprintf(buf, "download %s", argv[1]);
			send(g_hTcpSocket, buf, lstrlen(buf), 0);
		}
	}

	else if ((stricmp(argv[0], "udp") == 0) && (argc == 4))
	{
		WSADATA WSAData;
		SOCKET s;
		SOCKADDR_IN addr;
		struct hostent *host;

		if (WSAStartup(MAKEWORD(2, 2), &WSAData) == 0)
		{
			s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (s != SOCKET_ERROR)
			{
				addr.sin_family = AF_INET;
				addr.sin_port = htons(atoi(argv[2]));
				addr.sin_addr.S_un.S_addr = inet_addr(argv[1]);

				//resolve hostname if not ip
				if (addr.sin_addr.S_un.S_addr == INADDR_NONE)
				{
                    host = gethostbyname(argv[1]);
					if (host != NULL)
						memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
				}

				if (sendto(s, argv[3], (int)strlen(argv[3]), 0, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
					SetRichText(CON_RED, "UDP send failed (%d).\r\n", WSAGetLastError());
			}
			closesocket(s);
			WSACleanup();
		}
	}
	else
	{
		SetRichText(CON_RED, "Invalid command and or syntax error.\r\n");
	}
}

HFONT SetFont(char *myFont, int mySize)
{
	HFONT hFont;
	HDC hDc;

	hDc = GetDC(GetDesktopWindow());
	hFont = CreateFont(-MulDiv(mySize, GetDeviceCaps(hDc, LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, (LPCTSTR)myFont);
	ReleaseDC(GetDesktopWindow(), hDc);
	return hFont;
}

void SetStatusBar(char *Text)
{
	SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)Text);
}

void SetPingStatus(int Ping)
{
	char buf[15];
	sprintf(buf, "%dms", Ping);
	SendMessage(g_hStatus, SB_SETTEXT, 0, (LPARAM)buf);
}

void SetRichText(int datatype, char *buf, ...)
{
	char text[1024];
	va_list args;
	CHARFORMAT cf;
	int selpos;

	// Form string

	va_start(args, buf);
	vsprintf(text, buf, args);
	va_end(args);
	// strcat(text, "\r\n");

	// Set color format

	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;

	switch(datatype)
	{
	case CON_BLUE:
		{
			cf.crTextColor = 0xFF0000;
			break;
		}
	case CON_GREEN:
		{
			cf.crTextColor = 0x008000;
			break;
		}
	case CON_PURPLE:
		{
			cf.crTextColor = 0xFF0080;
			break;
		}
	case CON_RED:
		{
            cf.crTextColor = 0x0000FF;
			break;
		}
	case CON_YELLOW:
		{
			cf.crTextColor = 0x008080;
			break;
		}
	case CON_BROWN:
		{
			cf.crTextColor = 0x000080;
			break;
		}
	default:
		{
			cf.crTextColor = 0x000000;
			break;
		}
	}

	HWND hRichEdit;
	hRichEdit = GetDlgItem(g_hWnd, IDC_MAIN_RICHEDIT);

	// Add text to buffer

	selpos = GetWindowTextLength(hRichEdit);
	SendMessage(hRichEdit, EM_SETSEL, selpos, selpos);
	SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	SendMessage(hRichEdit, EM_REPLACESEL, FALSE, (LPARAM)text);

	// Auto-scroll

	SendMessage(hRichEdit, EM_SCROLLCARET, 0, 0);
}

char *WindowsString(OSVERSIONINFO *osvi)
{
	if (osvi->dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) 
	{
		if (osvi->dwMajorVersion >= 5 || osvi->dwMinorVersion >= 50)
		{
			return "Windows ME";
		}
		else if (osvi->dwMajorVersion == 4 && osvi->dwMinorVersion >= 10)
		{
			return "Windows 98";
		} 
		else if (osvi->dwMajorVersion == 4 && osvi->dwMinorVersion == 0)
		{
			return "Windows 95";
		}
		else
		{
			return "Unknown";
		}

	} 
	else if (osvi->dwPlatformId == VER_PLATFORM_WIN32_NT) 
	{
		if (osvi->dwMajorVersion == 6 && osvi->dwMinorVersion >= 1)
		{
			return "Windows 7";
		}
		else if (osvi->dwMajorVersion == 6 && osvi->dwMinorVersion == 0)
		{
			return "Windows Vista";
		}
		else if (osvi->dwMajorVersion == 5 && osvi->dwMinorVersion >= 1)
		{
			return "Windows XP";
		}
		else if (osvi->dwMajorVersion == 5 && osvi->dwMinorVersion == 0)
		{
			return "Windows 2000";
		}
		else if (osvi->dwMajorVersion == 4)
		{
			return "Windows NT4";
		}
		else
		{
			return "Unknown";
		}

	}
	else
	{
		return "Unknown";
	}
}

int MiscTimerRead(float *lpTime)
{
	_int64 PerformanceTimerValue;
	DWORD RegularTimerValue;

	if (bPerformanceTimerEnabled)
	{
		if (!QueryPerformanceCounter((LARGE_INTEGER *)&PerformanceTimerValue))
			return -1;
		*lpTime = (float)(PerformanceTimerValue - PerformanceTimerProgramStart) * TimerSecsPerTick;
	}
	else
	{
		RegularTimerValue = timeGetTime();
		*lpTime = (RegularTimerValue - RegularTimerProgramStart) * TimerSecsPerTick;
	}

	return 0;
}

int CaptureAnImage(HWND hWnd, RECT rcClient)
{
	HDC hdcScreen;
	HDC hdcWindow;
	HDC hdcMemDC = NULL;
	HBITMAP hbmScreen = NULL;
	BITMAP bmpScreen;

	// Retrieve the handle to a display device context for the client 
	// area of the window. 
	hdcScreen = GetDC(NULL);
	hdcWindow = GetDC(hWnd);

	// Create a compatible DC which is used in a BitBlt from the window DC
	hdcMemDC = CreateCompatibleDC(hdcWindow); 

	if(!hdcMemDC)
	{
		MessageBox(hWnd, "CreateCompatibleDC has failed","Failed", MB_OK);
		goto done;
	}

	// Get the client area for size calculation
//	RECT rcClient;
//	GetClientRect(hWnd, &rcClient);

	//This is the best stretch mode
	SetStretchBltMode(hdcWindow,HALFTONE);

	//The source DC is the entire screen and the destination DC is the current window (HWND)
	if(!StretchBlt(hdcWindow, 
		0,0, 
		rcClient.right, rcClient.bottom, 
		hdcScreen, 
		0,0,
		GetSystemMetrics (SM_CXSCREEN),
		GetSystemMetrics (SM_CYSCREEN),
		SRCCOPY))
	{
		MessageBox(hWnd, "StretchBlt has failed","Failed", MB_OK);
		goto done;
	}

	// Create a compatible bitmap from the Window DC
	hbmScreen = CreateCompatibleBitmap(hdcWindow, rcClient.right-rcClient.left, rcClient.bottom-rcClient.top);

	if(!hbmScreen)
	{
		MessageBox(hWnd, "CreateCompatibleBitmap Failed","Failed", MB_OK);
		goto done;
	}

	// Select the compatible bitmap into the compatible memory DC.
	SelectObject(hdcMemDC,hbmScreen);

	// Bit block transfer into our compatible memory DC.
	if(!BitBlt(hdcMemDC, 
		0,0, 
		rcClient.right-rcClient.left, rcClient.bottom-rcClient.top, 
		hdcWindow, 
		0,0,
		SRCCOPY))
	{
		MessageBox(hWnd, "BitBlt has failed", "Failed", MB_OK);
		goto done;
	}

	// Get the BITMAP from the HBITMAP
	GetObject(hbmScreen,sizeof(BITMAP),&bmpScreen);

	BITMAPFILEHEADER   bmfHeader;    
	BITMAPINFOHEADER   bi;

	bi.biSize = sizeof(BITMAPINFOHEADER);    
	bi.biWidth = bmpScreen.bmWidth;    
	bi.biHeight = bmpScreen.bmHeight;  
	bi.biPlanes = 1;    
	bi.biBitCount = 32;    
	bi.biCompression = BI_RGB;    
	bi.biSizeImage = 0;  
	bi.biXPelsPerMeter = 0;    
	bi.biYPelsPerMeter = 0;    
	bi.biClrUsed = 0;    
	bi.biClrImportant = 0;

	DWORD dwBmpSize = ((bmpScreen.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

	// Starting with 32-bit Windows, GlobalAlloc and LocalAlloc are implemented as wrapper functions that 
	// call HeapAlloc using a handle to the process's default heap. Therefore, GlobalAlloc and LocalAlloc 
	// have greater overhead than HeapAlloc.
	HANDLE hDIB = GlobalAlloc(GHND,dwBmpSize); 
	char *lpbitmap = (char *)GlobalLock(hDIB);    

	// Gets the "bits" from the bitmap and copies them into a buffer 
	// which is pointed to by lpbitmap.
	GetDIBits(hdcWindow, hbmScreen, 0,
		(UINT)bmpScreen.bmHeight,
		lpbitmap,
		(BITMAPINFO *)&bi, DIB_RGB_COLORS);

	// A file is created, this is where we will save the screen capture.
	HANDLE hFile = CreateFile("captureqwsx.bmp",
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);   

	// Add the size of the headers to the size of the bitmap to get the total file size
	DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	//Offset to where the actual bitmap bits start.
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER); 

	//Size of the file
	bmfHeader.bfSize = dwSizeofDIB; 

	//bfType must always be BM for Bitmaps
	bmfHeader.bfType = 0x4D42; //BM   

	DWORD dwBytesWritten = 0;
	WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
	WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
	WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

	//Unlock and Free the DIB from the heap
	GlobalUnlock(hDIB);    
	GlobalFree(hDIB);

	//Close the handle for the file that was created
	CloseHandle(hFile);

	//Clean up
done:
	DeleteObject(hbmScreen);
	DeleteObject(hdcMemDC);
	ReleaseDC(NULL,hdcScreen);
	ReleaseDC(hWnd,hdcWindow);

	return 0;
}