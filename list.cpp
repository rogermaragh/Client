#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <stdlib.h>
#include <commdlg.h>
#include "main.h"
#include "list.h"
#include "tree.h"
#include "plugin.h"
#include "tcp.h"
#include "gui.h"
#include "resource.h"

// Global variables
char *QueryCaption, *QueryText, *QueryDst;
int QueryLen;
OPENFILENAME openFileName;
char programFileName[270];

HWND g_hWndList;
HWND hListView;
HWND g_hStatus;

int WINAPI WinList(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Initialize variables
	static char szClassName[] = "ConsoleList";
	static char szWindowName[] = "ConsoleList";

	// Define & register window class
	WNDCLASSEX WndClass;

	memset(&WndClass, 0, sizeof(WndClass));
	WndClass.cbSize = sizeof(WndClass);						// size of structure
	WndClass.lpszClassName = szClassName;					// name of window class
	WndClass.lpfnWndProc = WndListProc;						// points to a window procedure
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
	g_hWndList = CreateWindowEx(WS_EX_WINDOWEDGE, szClassName, szWindowName, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 400, 0, 0, g_hInstance, 0);

	if (!g_hWndList)
	{
		return 0;
	}

	ShowWindow(g_hWndList, nCmdShow);
	UpdateWindow(g_hWndList);

	return 0;
}

LRESULT CALLBACK WndListProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_CREATE:
		{
			INITCOMMONCONTROLSEX icex;

			icex.dwSize = sizeof(icex);
			icex.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_USEREX_CLASSES;

			InitCommonControlsEx(&icex);

			// Create ListView Control
			HFONT hfDefault;
			//HWND hListView;

			hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, 0,
				WS_CHILD | WS_VISIBLE | LVS_REPORT, 0, 0, 0, 0,
				hWnd, (HMENU)IDC_LIST_LISTVIEW, GetModuleHandle(NULL), NULL);

			if (hListView == NULL)
				MessageBox(hWnd, "failed to create listview control", "error", MB_OK | MB_ICONERROR);

			hfDefault = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessage(hListView, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE, 0));

			LVITEM lvItem;
			LVCOLUMN lvColumn;

			CHAR szText[81];
			HBITMAP hBitmap;
			HBITMAP hBitmapSmall;
			HDC hDC;
			int dIndex;
			int dX;
			int dY;

			HIMAGELIST g_hImageList;
			HIMAGELIST g_hSmallIL;

			// Create and initialize image list used with control
			g_hImageList = ImageList_Create(32, 32, ILC_COLORDDB | ILC_MASK, 11, 1);
			g_hSmallIL = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 11, 1);
			// Load images based on color depth
			hDC = GetDC(hWnd);
			if(GetDeviceCaps(hDC, BITSPIXEL) <= 8)
			{
				hBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_DEFAULT));
				hBitmapSmall = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_DEFAULT));
			}
			else
			{
				hBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_DEFAULT));
				hBitmapSmall = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_DEFAULT));
			}
			ReleaseDC(hWnd, hDC);
			// Add images to image list
			ImageList_AddMasked(g_hImageList, hBitmap, RGB(255, 0, 255));
			ImageList_AddMasked(g_hSmallIL, hBitmapSmall, RGB(255, 0, 255));
			DeleteObject(hBitmap);
			DeleteObject(hBitmapSmall);

			// Set image list of list view
			ListView_SetImageList(hListView, g_hImageList, LVSIL_NORMAL);
			ListView_SetImageList(hListView, g_hSmallIL, LVSIL_SMALL);

			// Add columns
			lvColumn.mask = LVCF_TEXT|LVCF_WIDTH;

			// Column 0
			lvColumn.pszText = "Name";
			lvColumn.cx = ListView_GetStringWidth(hListView, "Jupiter")+32;
			ListView_InsertColumn(hListView, 0, &lvColumn);

			// Column 1
			lvColumn.pszText = "Size";
			lvColumn.cx = ListView_GetStringWidth(hListView, "0,000,000,000")+16;
			ListView_InsertColumn(hListView, 1, &lvColumn);

			// Column 2
			lvColumn.pszText = "Attributes";
			lvColumn.cx = ListView_GetStringWidth(hListView, "00,000.0")+16;
			ListView_InsertColumn(hListView, 2, &lvColumn);

			// Column 3
			lvColumn.pszText = "Mass (kg)";
			lvColumn.cx = ListView_GetStringWidth(hListView, "0.0000e00")+16;
			ListView_InsertColumn(hListView, 3, &lvColumn);

			// Add items to list view
			lvItem.state = 0;
			lvItem.stateMask = 0;
			lvItem.cchTextMax = 0;
			lvItem.lParam = 0;
			lvItem.iIndent = 0;

			ListView_InsertItem(hListView,&lvItem);

			for(dX=9;dX>=1;dX--)
			{
				LoadString(g_hInstance, ITEMOFFSET+(dX-1)*4, szText, sizeof(szText));

				lvItem.mask = LVIF_TEXT|LVIF_IMAGE;
				lvItem.iItem = 0;
				lvItem.iSubItem = 0;
				lvItem.pszText = szText;
				lvItem.iImage = dX-1;

				dIndex = ListView_InsertItem(hListView, &lvItem);

				// Add sub-items
				for(dY=1;dY<4;dY++)
				{
					LoadString(g_hInstance, ITEMOFFSET+((dX-1)*4)+dY, szText, sizeof(szText));

					lvItem.mask = TVIF_TEXT;
					lvItem.iItem = dIndex;
					lvItem.iSubItem = dY;
					lvItem.pszText = szText;

					ListView_SetItem(hListView, &lvItem);
				}
			}

			HCURSOR g_hCursor;
			HWND g_hUserToolTip;

			// Set value returned by 'cursor' in parser
			g_hCursor = ListView_GetHotCursor(hListView);

			// Set value returned by 'tooltip' in parser
			g_hUserToolTip = ListView_GetToolTips(hListView);

			// Create Rebar Control
			REBARINFO rbi;
			REBARBANDINFO rbbInfo;

			TBBUTTON tbbStruct[10];
			UINT dImages[] = { 0, 1, 2, -1, 3, 4, -1, 5, -1, 6 };

			COMBOBOXEXITEM cbeItem;
			LPSTR pLabels[] = { "Desktop","Network","3½ Floppy (A:)","HDD (C:)","Control Spy","CDROM (D:)","Printers","Control Panel","Network","Recycle Bin" };
			UINT dIndent[] = { 0, 1, 2, 2, 3, 2, 2, 2, 1, 1 };

			RECT rcControl;
			//HBITMAP hBitmap;
			//int dX;

			HIMAGELIST g_hToolbarIL = NULL;
			HIMAGELIST g_hToolbarHotIL = NULL;
			HIMAGELIST g_hComboBoxExIL = NULL;


			HWND hRebar;

			hRebar = CreateWindowEx(0, REBARCLASSNAME, NULL,
				WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
				CCS_ADJUSTABLE | RBS_VARHEIGHT | RBS_BANDBORDERS, 0, 0, 0, 0,
				hWnd, (HMENU)IDC_LIST_REBAR, GetModuleHandle(NULL), NULL);

			memset(&rbi, 0, sizeof(rbi));
			rbi.cbSize = sizeof(rbi);
			rbi.fMask = 0;
			rbi.himl = (HIMAGELIST)0;
			
			SendMessage(hRebar, RB_SETBARINFO, 0, (LPARAM)&rbi);

			// Toolbar child
            HWND hToolBar;

			hToolBar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
				WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | CCS_NORESIZE | CCS_NODIVIDER,
				0, 0, 0, 0, hRebar, NULL, GetModuleHandle(NULL), NULL);

			// Since created toolbar with CreateWindowEx, specify the button structure size
			// This step is not required if CreateToolbarEx was used
			SendMessage(hToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

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
			SendMessage(hToolBar, TB_SETIMAGELIST, 0,(LPARAM)g_hToolbarIL);
			SendMessage(hToolBar, TB_SETHOTIMAGELIST, 0,(LPARAM)g_hToolbarHotIL);

			// Set toolbar padding
			SendMessage(hToolBar, TB_SETINDENT, 2, 0);

			// Add buttons to control
			for(dX=0;dX<10;dX++)
			{
				// Add button
				if(dImages[dX] != -1)
				{
					tbbStruct[dX].iBitmap = dImages[dX];
					tbbStruct[dX].fsStyle = TBSTYLE_BUTTON;
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

			SendMessage(hToolBar, TB_ADDBUTTONS, 10, (LPARAM)tbbStruct);

			// Add Rebar band

			rbbInfo.cbSize = sizeof(REBARBANDINFO);

			rbbInfo.fMask = RBBIM_STYLE|RBBIM_CHILD|RBBIM_CHILDSIZE|RBBIM_SIZE;
			rbbInfo.fStyle = RBBS_GRIPPERALWAYS|RBBS_CHILDEDGE;
			rbbInfo.hwndChild = hToolBar;
			rbbInfo.cxMinChild = 0;
			rbbInfo.cyMinChild = HIWORD(SendMessage(hToolBar,TB_GETBUTTONSIZE,0,0));
			rbbInfo.cx = 200;

			SendMessage(hRebar,RB_INSERTBAND,-1,(LPARAM)&rbbInfo);

			// COMBOBOXEX

			HWND hComboBoxEx;

			hComboBoxEx = CreateWindowEx(0, WC_COMBOBOXEX, NULL,
				WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, //CBS_DROPDOWNLIST,
				0, 0, 300, 150, hRebar, NULL, GetModuleHandle(NULL), NULL);

			// Create and initialize image list used for child ComboBoxEx
			g_hComboBoxExIL = ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 10, 1);
			hBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_COMPUTER));
			ImageList_AddMasked(g_hComboBoxExIL, hBitmap, RGB(255, 0, 255));
			DeleteObject(hBitmap);

			// Assign image list to control
			SendMessage(hComboBoxEx, CBEM_SETIMAGELIST, 0, (LPARAM)g_hComboBoxExIL);

			// Add items to control
			for(dX=0;dX<10;dX++)
			{
				cbeItem.mask = CBEIF_IMAGE|CBEIF_SELECTEDIMAGE|CBEIF_TEXT|CBEIF_INDENT;
				cbeItem.iItem = -1;
				cbeItem.pszText = pLabels[dX];
				cbeItem.cchTextMax = strlen(pLabels[dX]);
				cbeItem.iImage = dX;
				cbeItem.iSelectedImage = dX;
				cbeItem.iIndent = dIndent[dX];

				SendMessage(hComboBoxEx, CBEM_INSERTITEM, 0, (LPARAM)&cbeItem);
			}

			// Select first item
			SendMessage(hComboBoxEx, CB_SETCURSEL, 4, 0);

			// Add Rebar band

			rbbInfo.cbSize = sizeof(REBARBANDINFO);
			rbbInfo.fMask = RBBIM_STYLE|RBBIM_CHILD|RBBIM_TEXT|RBBIM_CHILDSIZE|RBBIM_SIZE;
			rbbInfo.fStyle = RBBS_GRIPPERALWAYS|RBBS_CHILDEDGE;
			rbbInfo.hwndChild = hComboBoxEx;
			GetWindowRect(hComboBoxEx, &rcControl);
			rbbInfo.cxMinChild = 0;
			rbbInfo.cyMinChild = rcControl.bottom-rcControl.top;
			rbbInfo.lpText = "Address";
			rbbInfo.cx = 200;

			SendMessage(hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbInfo);


			// Create StatusBar Control
			//HWND hStatus;
			int statwidths[] = {100, -1};

			g_hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
				hWnd, (HMENU)IDC_LIST_STATUS, GetModuleHandle(NULL), NULL);

			SendMessage(g_hStatus, SB_SETPARTS, sizeof(statwidths)/sizeof(int), (LPARAM)statwidths);
			SendMessage(g_hStatus, SB_SETTEXT, 0, (LPARAM)"0 ms");
			SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Console Ready...");
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

			HWND hListView;
			int iListViewHeight;
			RECT rcClient;

			// Size toolbar and get height

			hTool = GetDlgItem(hWnd, IDC_LIST_REBAR);

			GetWindowRect(hTool, &rcTool);
			iToolHeight = rcTool.bottom - rcTool.top;

			// Resize rebar
			MoveWindow(hTool, 0, 0, LOWORD(lParam), iToolHeight, FALSE);

			// Size statusbar and get height

			hStatus = GetDlgItem(hWnd, IDC_LIST_STATUS);
			SendMessage(hStatus, WM_SIZE, 0, 0);

			GetWindowRect(hStatus, &rcStatus);
			iStatusHeight = rcStatus.bottom - rcStatus.top;

			// Calculate the remaining height and size richedit

			GetClientRect(hWnd, &rcClient);

			iListViewHeight = rcClient.bottom - iToolHeight - iStatusHeight;

			hListView = GetDlgItem(hWnd, IDC_LIST_LISTVIEW);
			SetWindowPos(hListView, NULL, 0, iToolHeight, rcClient.right, iListViewHeight, SWP_NOZORDER);

			InvalidateRect(hWnd, 0, FALSE);
		}
		break;
		case WM_CLOSE:
		{
			// Do not destroy window yet
			ShowWindow(hWnd, SW_HIDE);

			return 0;
		}
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
				GetClientRect(g_hWndList, &rc);
				PostMessage(g_hWndList, WM_SIZE, 0, ((rc.bottom - rc.top) << 16) | (rc.right - rc.left));

				return 0;
			}

			if (((NMHDR *)lParam)->hwndFrom == hListView)
			{
				switch (((NMHDR *)lParam)->code) 
				{
					case NM_DBLCLK:
					{
						if (ListView_GetSelectedCount(hListView) == 1)
						{
							char buf[MAX_PATH], Caption[MAX_PATH];
							int idx;

							idx = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
							strcpy(buf, g_szCurrentPath);

							LVITEM Item;
							Item.mask = LVIF_TEXT | LVIF_PARAM;
							Item.iItem = idx;
							Item.iSubItem = 0;
							Item.pszText = Caption;
							Item.cchTextMax = sizeof(Caption);

							ListView_GetItem(hListView, &Item);

							if (strcmp(Item.pszText, "..") == 0)
							{
								UpOneLevel(buf);
							}
							else
							{
								strcat(buf, Item.pszText);
								strcat(buf, "\\");
							}

							//UpdateListing(buf, false);

							char WindowText[MAX_PATH + 1], buffer[MAX_PATH + 1];

							GetWindowText(g_hWndList, WindowText, sizeof(WindowText));

							strcpy(buffer, WindowText);

							char *temp;
							int i = 0;
							int j = 0;
							temp = (char *)calloc(lstrlen(buffer), sizeof(char));
							for (i=lstrlen(buffer); i>0; i--)
								if (buffer[i] == ' ')
									break;
							for (j=i+1; j<lstrlen(buffer); j++)
								strncat(temp, &buffer[j], 1);

							if (stricmp(temp, "Application") == 0) {
								SendStr((char *)"45", buf);
							} else if (stricmp(temp, "Process") == 0) {
								SendStr((char *)"45", buf);
							} else if (stricmp(temp, "Drive") == 0) {
								UpdateListing(buf, false);
							} else if (stricmp(temp, "Window") == 0) {
								SendStr((char *)"45", buf);
							} else if (stricmp(temp, "Network") == 0) {
								UpdateListing(buf, false);
							} else if (stricmp(temp, "Registry") == 0) {
								SendStr((char *)"45", buf);
								strcpy(g_szCurrentPath, buf);
							}
						}
						return 0;
					}
					case NM_RCLICK:
					{
						POINT curs;
						HMENU PopupMenu;
						char WindowText[MAX_PATH + 1], buffer[MAX_PATH + 1];
						int ret = IDR_MENU_POPUPS;

						GetWindowText(g_hWndList, WindowText, sizeof(WindowText));

						strcpy(buffer, WindowText);

						char *temp;
						int i = 0;
						int j = 0;
						temp = (char *)calloc(lstrlen(buffer), sizeof(char));
						for (i=lstrlen(buffer); i>0; i--)
							if (buffer[i] == ' ')
								break;
						for (j=i+1; j<lstrlen(buffer); j++)
							strncat(temp, &buffer[j], 1);

						if (stricmp(temp, "Application") == 0) {
							ret = 1;
						} else if (stricmp(temp, "Process") == 0) {
							ret = 0;
						} else if (stricmp(temp, "Drive") == 0) {
							ret = 0;
						} else if (stricmp(temp, "Window") == 0) {
							ret = 1;
						} else if (stricmp(temp, "Network") == 0) {
							ret = 0;
						} else if (stricmp(temp, "Registry") == 0) {
							ret = 2;
						}

						GetCursorPos(&curs);
						PopupMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU_POPUPS));

						TrackPopupMenu(GetSubMenu(PopupMenu, ret),  TPM_LEFTALIGN | TPM_LEFTBUTTON,
							curs.x, curs.y, 0, g_hWndList, 0);

						DestroyMenu(PopupMenu);
						return 0;
					}
				}
			}
		}
		break;
		case WM_COMMAND:
		{
			unsigned long argl[10];
			char *argv[10], buffer[MAX_PATH + 1], buffer2[MAX_PATH + 1];
			int idx;

			switch(LOWORD(wParam))
			{
				case ID_REGISTRYEXPLORER_REFRESH:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"45", g_szCurrentPath);
					return 0;
				}
				case ID_ROOT_HKEY:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CLASSES_ROOT");
					strcpy(g_szCurrentPath, "");
					return 0;
				}
				case ID_ROOT_HKEY40033:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_USER");
					strcpy(g_szCurrentPath, "");
					return 0;
				}
				case ID_ROOT_HKEY40034:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_LOCAL_MACHINE");
					strcpy(g_szCurrentPath, "");
					return 0;
				}
				case ID_ROOT_HKEY40035:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_USERS");
					strcpy(g_szCurrentPath, "");
					return 0;
				}
				case ID_ROOT_HKEY40036:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_CONFIG");
					strcpy(g_szCurrentPath, "");
					return 0;
				}
				case ID_REGISTRYEXPLORER_SETVALUE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						char caption[MAX_PATH];

						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
						while (idx != -1)
						{
							strcpy(caption, buffer2);
							if (InputBox("Set Value", "Value data:", caption, sizeof(caption)))
							{
								strcpy(buffer, buffer2);
								strcpy(buffer2, g_szCurrentPath);

								argv[0] = buffer;
								argl[0] = strlen(buffer) + 1;
								argv[1] = buffer2;
								argl[1] = strlen(buffer2) + 1;
								argv[2] = caption;
								argl[2] = strlen(caption) + 1;

								SendData(46, 3, argv, argl);
							}

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
						}
					}
					return 0;
				}
				case ID_CREATE_SUBKEY:
				{
					char caption[MAX_PATH];

					if (InputBox("Create Subkey", "Subkey data:", caption, sizeof(caption)))
					{
						strcpy(buffer, "1");
						strcpy(buffer2, g_szCurrentPath);
						strcat(buffer2, caption);

						argv[0] = buffer;
						argl[0] = strlen(buffer) + 1;
						argv[1] = buffer2;
						argl[1] = strlen(buffer2) + 1;
						argv[2] = caption;
						argl[2] = strlen(caption) + 1;

						SendData(47, 3, argv, argl);
					}
					return 0;
				}
				case ID_CREATE_STRINGVALUE:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_USER");
					return 0;
				}
				case ID_CREATE_DWORDVALUE:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_USER");
					return 0;
				}
				case ID_CREATE_BINARYVALUE:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_USER");
					return 0;
				}
				case ID_REGISTRYEXPLORER_DELETE:
				{
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_USER");
					return 0;
				}

				case ID_WINDOWEXPLORER_REFRESH:
				{
					SetWindowText(g_hWndList, "ConsoleList Application");
					SendStr((char *)"30", (char *)"\\");
					return 0;
				}
				case ID_WINDOWEXPLORER_SHOW:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "3");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_HIDE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "4");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_MINIMIZE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "7");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_MAXIMIZE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "8");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_RESTORE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "9");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_ENABLE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "5");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_DISABLE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "6");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_SETCAPTION:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						char caption[MAX_PATH];

						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(caption, buffer2);
							if (InputBox("Set Caption", "Title:", caption, sizeof(caption)))
							{
								strcpy(buffer, "1");

								argv[0] = buffer;
								argl[0] = strlen(buffer) + 1;
								argv[1] = buffer2;
								argl[1] = strlen(buffer2) + 1;
								argv[2] = caption;
								argl[2] = strlen(caption) + 1;

								SendData(29, 3, argv, argl);
							}

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_BRINGTOFRONT:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "2");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}
				case ID_WINDOWEXPLORER_CLOSE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 1);
						while (idx != -1)
						{
							strcpy(buffer, "0");

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							argv[1] = buffer2;
							argl[1] = strlen(buffer2) + 1;

							SendData(29, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 1);
						}
					}
					return 0;
				}

				case ID_FILEEXPLORER_REFRESH:
				{
					UpdateListing(g_szCurrentPath, true);
					return 0;
				}
				case ID_FILEEXPLORER_DELETE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						if (MessageBoxEx(g_hWndList, "Are you sure you wish to delete the selected items?",
							"Confirmation", MB_YESNO | MB_ICONINFORMATION, 0) == IDYES)
						{
							idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
							while (idx != -1)
							{
								strcpy(buffer, g_szCurrentPath);
								strcat(buffer, buffer2);
								
								argv[0] = buffer;
								argl[0] = strlen(buffer) + 1;

								SendData(12, 1, argv, argl);
								//SendData(17, 1, argv, argl);

								idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
							}
						}
					}
					return 0;
				}
				case ID_FILEEXPLORER_EXECUTE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
						while (idx != -1)
						{
							strcpy(buffer, g_szCurrentPath);
							strcat(buffer, buffer2);

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;

							SendData(15, 1, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
						}
					}
					return 0;
				}
				case ID_FILEEXPLORER_MOVE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						char buffer3[MAX_PATH];

						strcpy(buffer3, g_szCurrentPath);
						InputBox("Move File", "Destination:", buffer3, sizeof(buffer3));

						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
						while (idx != -1)
						{
							strcpy(buffer, g_szCurrentPath);
							strcat(buffer, buffer2);

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							strcat(buffer3, buffer2);
							argv[1] = buffer3;
							argl[1] = strlen(buffer3) + 1;

							SendData(14, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
						}
					}

					return 0;
				}
				case ID_FILEEXPLORER_COPY:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						char buffer3[MAX_PATH];

						strcpy(buffer3, g_szCurrentPath);
						InputBox("Copy File", "Destination:", buffer3, sizeof(buffer3));

						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
						while (idx != -1)
						{
							strcpy(buffer, g_szCurrentPath);
							strcat(buffer, buffer2);

							argv[0] = buffer;
							argl[0] = strlen(buffer) + 1;
							strcat(buffer3, buffer2);
							argv[1] = buffer3;
							argl[1] = strlen(buffer3) + 1;

							SendData(13, 2, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
						}
					}

					return 0;
				}
				case ID_FILEEXPLORER_NEWFOLDER:
				{
					strcpy(buffer, g_szCurrentPath);
					InputBox("New Folder", "Destination:", buffer, sizeof(buffer));
					argv[0] = buffer;
					argl[0] = strlen(buffer) + 1;
					SendData(16, 1, argv, argl);
					return 0;
				}
				case ID_FILEEXPLORER_COMPARE:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
						while (idx != -1)
						{
							strcpy(buffer, g_szCurrentPath);
							strcat(buffer, buffer2);

							argv[0] = buffer;
							argl[0] = (int)strlen(buffer) + 1;

							char *temp;
							int i = 0;
							int j = 0;
							//sendstr (128)(C:\Documents and Settings\Owner\My Documents\Downloads\Henry.Poole.Is.Here.DVDRip.XviD-ARROW\Sample\arw-hpih.dvdrip.xvid.sample.avi)
							temp = (char *)calloc(lstrlen(buffer), sizeof(char));
							for (i=lstrlen(buffer); i>0; i--)
								if (buffer[i] == '\\')
									break;
							for (j=i+1; j<lstrlen(buffer); j++)
								strncat(temp, &buffer[j], 1);

							LocalFileName = temp;

							SendData(7, 1, argv, argl);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
						}
					}
					return 0;
				}
				case ID_FILEEXPLORER_DOWNLOAD:
				{
					if (ListView_GetSelectedCount(hListView) > 0)
					{
						idx = GetSelectedItemText(-1, buffer2, sizeof(buffer2), 0);
						while (idx != -1)
						{
							strcpy(buffer, g_szCurrentPath);
							strcat(buffer, buffer2);

							argv[0] = buffer;
							argl[0] = (int)strlen(buffer) + 1;

							char *temp;
							int i = 0;
							int j = 0;
							//sendstr (128)(C:\Documents and Settings\Owner\My Documents\Downloads\Henry.Poole.Is.Here.DVDRip.XviD-ARROW\Sample\arw-hpih.dvdrip.xvid.sample.avi)
							temp = (char *)calloc(lstrlen(buffer), sizeof(char));
							for (i=lstrlen(buffer); i>0; i--)
								if (buffer[i] == '\\')
									break;
							for (j=i+1; j<lstrlen(buffer); j++)
								strncat(temp, &buffer[j], 1);

							LocalFileName = temp;

							//SendData(128, 1, argv, argl);
							EntryPoint();
							char *rdbuffer[2];
							rdbuffer[0] = "download";
							rdbuffer[1] = buffer;
							DownloadParseCommand(2, rdbuffer);

							idx = GetSelectedItemText(idx, buffer2, sizeof(buffer2), 0);
						}
					}
					return 0;
				}
				case ID_FILEEXPLORER_UPLOAD:
				{
					openFileName.lStructSize = sizeof(openFileName);
					openFileName.hInstance = g_hInstance;
					openFileName.lpstrFilter = "All Files (*.*)\0*.*\0";
					openFileName.lpstrTitle = "Select a file to upload";
					openFileName.Flags = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
					openFileName.lpstrFile = programFileName;
					openFileName.nMaxFile = 270;
	
					if (GetOpenFileName(&openFileName)) {
						strcpy(buffer, programFileName);
						strcpy(buffer2, g_szCurrentPath);

						char *temp;
						int i = 0;
						int j = 0;
						//sendstr (128)(C:\Documents and Settings\Owner\My Documents\Downloads\Henry.Poole.Is.Here.DVDRip.XviD-ARROW\Sample\arw-hpih.dvdrip.xvid.sample.avi)
						temp = (char *)calloc(lstrlen(buffer), sizeof(char));
						for (i=lstrlen(buffer); i>0; i--)
							if (buffer[i] == '\\')
								break;
						for (j=i+1; j<lstrlen(buffer); j++)
							strncat(temp, &buffer[j], 1);

						LocalFileName = temp;

						strncat(buffer2, temp, lstrlen(temp));

						g_hFile = CreateFile(programFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL, NULL);
						if (g_hFile != INVALID_HANDLE_VALUE)
						{
							unsigned long size = GetFileSize(g_hFile, NULL);
							char buf[256];
							wsprintf(buf, "%d", GetFileSize(g_hFile, NULL));
//							Text = (char *)&buf;
//							dw = lstrlen(Text);

							argv[0] = buffer2;
							argl[0] = (int)strlen(buffer2) + 1;
							argv[1] = buf;
							argl[1] = (int)strlen(buf) + 1;

							SendData(129, 2, argv, argl);
//							SendData(128, 1, &Text, &dw, ci);
							g_bTransferring = true;
							WSAAsyncSelect(g_hTcpSocket, g_hWnd, WM_MAINSOCKET, 0);
							ioctlsocket(g_hTcpSocket, FIONBIO, 0);
							Sleep(100);
							StartSendThread(g_hTcpSocket);
						}
					}
					return 0;
				}
				case ID_FILE_NEW:
					SetWindowText(g_hWndList, "ConsoleList Application");
					SendStr((char *)"30", (char *)"\\");
				break;
				case ID_FILE_OPEN:
					SetWindowText(g_hWndList, "ConsoleList Process");
					SendStr((char *)"22", (char *)"\\");
				break;
				case ID_FILE_SAVEAS:
					SetWindowText(g_hWndList, "ConsoleList Drive");
					SendStr((char *)"11", (char *)"\\");
				break;
				case 40005:
					SetWindowText(g_hWndList, "ConsoleList Window");
					SendStr((char *)"26", (char *)"\\");
				break;
				case 40006:
					SetWindowText(g_hWndList, "ConsoleList Network");
					SendStr((char *)"35", (char *)"\\");
				break;
				case 40008:
					SetWindowText(g_hWndList, "ConsoleList Registry");
					SendStr((char *)"43", (char *)"HKEY_CURRENT_USER");
					strcpy(g_szCurrentPath, "");
				break;
				case 30010:
					SendStr((char *)"200", (char *)"\\");
				break;
			}
		}
		break;
		case WM_GETMINMAXINFO:
		{
			// Set window size constraints
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = 500;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = 400;
		}
		break;
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

int GetSelectedItemText(int idx, char *buf, int len, int sub)
{
	int newidx;
	LVITEM Item;

	newidx = ListView_GetNextItem(hListView, idx, LVNI_SELECTED);
						
	Item.mask = LVIF_TEXT | LVIF_PARAM;
	Item.iItem = newidx;
	Item.iSubItem = sub;
	Item.pszText = buf;
	Item.cchTextMax = len;

	ListView_GetItem(hListView, &Item);
	return newidx;
}

BOOL CALLBACK QueryDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			SetWindowText(hwndDlg, QueryCaption);
			SetDlgItemText(hwndDlg, IDC_QUERY_TEXT, QueryText);
			SetDlgItemText(hwndDlg, IDC_QUERY_EDIT, QueryDst);
			return TRUE;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					GetWindowText(GetDlgItem(hwndDlg, IDC_QUERY_EDIT), QueryDst, QueryLen);
					EndDialog(hwndDlg, IDOK);
					return TRUE;
				}
				case IDCANCEL:
				{
					EndDialog(hwndDlg, IDCANCEL);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}
