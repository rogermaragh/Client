#ifndef _LIST_H_
#define _LIST_H_

extern char *QueryCaption, *QueryText, *QueryDst;
extern int QueryLen;

extern HWND g_hWndList;
extern HWND hListView;
extern HWND g_hStatus;

#define IDC_LIST_LISTVIEW   301
#define IDC_LIST_REBAR      302
#define IDC_LIST_STATUS     303

LRESULT CALLBACK WndListProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI WinList(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
BOOL CALLBACK QueryDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
int GetSelectedItemText(int idx, char *buf, int len, int sub);

#endif