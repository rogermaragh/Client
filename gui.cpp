#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <stdlib.h> //atoi
#include <stdio.h>
#include "main.h"
#include "list.h"
#include "tree.h"
#include "plugin.h"
#include "tcp.h"
#include "gui.h"
#include "shared.h"
#include "resource.h"

#define THREADCOUNT 1

//Global variables
char *gpHost;
bool bConnected;
bool gbConnected;
bool bTransferring;
bool gbTransferring;
HINSTANCE ghInstance;
HWND ghWnd;
SOCKET hTcpSocket;
SOCKET ghTcpSocket;
bool Processing;
int ClientCount = 0;
Client *Clients = NULL;
char *RecvBuffer, *SendBuffer;
unsigned long RecvBufSize, SendBufSize;
HANDLE ghFile;
HANDLE ghMutex; 

DWORD WINAPI WriteToDatabase( LPVOID );


void free(void *pv)
{
	if (pv)
		HeapFree(GetProcessHeap(), 0, pv);
}


void *malloc(unsigned int cb)
{
	return HeapAlloc(GetProcessHeap(), 0, cb);
}


void *realloc(void *pv, unsigned int cb)
{
	if (!pv)
		return malloc(cb);

	if (cb == 0) {
		free(pv);
		return NULL;
	}

	return HeapReAlloc(GetProcessHeap(), 0, pv, cb);
}


void *operator new(unsigned int cb)
{
	return malloc(cb);
}


void operator delete(void *pv)
{
	free(pv);
}


extern "C" int _purecall(void)
{
	return 0;
}


int memcmp(const void *buf1, const void *buf2, unsigned int count)
{
	if (!count)
		return 0;

	while (--count && *(char *)buf1 == *(char *)buf2) {
		buf1 = (char *)buf1 + 1;
		buf2 = (char *)buf2 + 1;
	}

	return(*((unsigned char *)buf1) - *((unsigned char *)buf2));
}


void *memset(void *dst, int val, unsigned int count)
{
	void *start = dst;

	while (count--) {
		*(char *)dst = (char)val;
		dst = (char *)dst + 1;
	}

	return start;
}


void *memcpy(void *dst, const void *src, unsigned int count)
{
	void *ret = dst;

	while (count--) {
		*(char *)dst = *(char *)src;
		dst = (char *)dst + 1;
		src = (char *)src + 1;
	}

	return ret;
}


void *memmove(void *dst, const void *src, unsigned int count)
{
	void *ret = dst;

	if (dst <= src || (char *)dst >= ((char *)src + count)) {
		while (count--) {
			*(char *)dst = *(char *)src;
			dst = (char *)dst + 1;
			src = (char *)src + 1;
		}
	}
	else {
		dst = (char *)dst + count - 1;
		src = (char *)src + count - 1;

		while (count--) {
			*(char *)dst = *(char *)src;
			dst = (char *)dst - 1;
			src = (char *)src - 1;
		}
	}

	return ret;
}


void DownloadCleanup(SOCKET wParam)
{
	int i, j;

	for (i=ClientCount-1; i>=0; i--)
	{
		if (Clients[i].hSocket == wParam)
		{
			//closesocket(wParam);
			for (j=i+1; j<ClientCount; j++)
			{
				Clients[j-1] = Clients[j];
			}
			ClientCount--;
		}
	}
}

void DownloadSendData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal, unsigned short ci)
{
	unsigned int Size, Index, i;

	// Calculate total message length
	Size = 6 + datac * 4;
	for (i = 0; i < datac; i++)
		Size += datal[i];

	// Allocate memory for message
	char *SckData = (char *)malloc(Size + 4); // (+ 4 to compensate for message length header)

	memcpy(SckData, &Size, 4); // Insert 4 byte message length	
	memcpy(SckData + 4, &cmd, 2); // Insert 2 byte command	
	memcpy(SckData + 6, &datac, 4); // Insert 4 byte argument count

	// Insert arguments
	Index = 10;
	for (i = 0; i < datac; i++) {
		memcpy(SckData + Index, &datal[i], 4);
		Index += 4;
		memcpy(SckData + Index, data[i], datal[i]);
		Index += datal[i];
	}

	if (SendBufSize == 0) {

		// Add data to send buffer, and try to send

		SendBufSize = Size + 4; // (+ 4 to compensate for message length header)
		SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
		memcpy(SendBuffer, SckData, SendBufSize);

		int len = send(Clients[ci].hSocket, SendBuffer, SendBufSize, 0);

		if (len > 0) {
			SendBufSize -= len;
			memmove(SendBuffer, SendBuffer + len, SendBufSize);
			SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
		}
	} 
	else {

		// Add data to send buffer, FD_WRITE messages
		// will handle the rest

		SendBufSize += Size + 4; // (+ 4 to compensate for message length header)
		SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
		memcpy(SendBuffer + SendBufSize - Size - 4, SckData, Size + 4);
	}

	// Free memory
	free(SckData);
}

void DownloadSendStr(char *msg, unsigned short ci)
{
	unsigned long msglen = lstrlen(msg);
	DownloadSendData(4, 1, &msg, &msglen, ci);
}

DWORD WINAPI DownloadReceiveFileThread(LPVOID lpArgs)
{
	HANDLE hFile = ((FILETRANSFERARGS *)lpArgs)->hFile;
	SOCKET hSocket = ((FILETRANSFERARGS *)lpArgs)->hSocket;
	DWORD FileSize = ((FILETRANSFERARGS *)lpArgs)->FileSize;
	DWORD Position;
	DWORD BytesWritten, RecvBytes;
	char Buffer[4096], tmpbuff[512];
	int ret;
	TIMEVAL tm;
	fd_set fs;

	WSAAsyncSelect(hSocket, g_hWndMain, 0, 0);
	//	ioctlsocket(hSocket, FIONBIO, 0);

	linger lin;
	lin.l_onoff=lin.l_linger=0;

	int opt=1;
	setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_LINGER, (char *)&lin, sizeof(lin));

	u_long noblock=0;
	ioctlsocket(hSocket, FIONBIO, &noblock);

	free(lpArgs);
	Position = 0;

	FD_ZERO(&fs);
	FD_SET(hSocket, &fs);
	tm.tv_sec = 60;
	tm.tv_usec = 0;

	while (Position < FileSize)
	{
		if (select(0, &fs, NULL, NULL, &tm) > 0) {
			if (FD_ISSET(hSocket, &fs)) {

				if (FileSize-Position >= 4096)
					RecvBytes = 4096;
				else
					RecvBytes = FileSize-Position;

				ret = recv(hSocket, Buffer, RecvBytes, 0);
				//			SetRichText(CON_BROWN, "[%d] ret: %d\r\n", Position * 100 / FileSize, ret);
				if (ret > 0)
				{
					WriteFile(hFile, Buffer, ret, &BytesWritten, NULL);
					Position += ret;
					wsprintf(tmpbuff, "%d:%i", Position * 100 / FileSize, ret);
					SetWindowText(g_hWndMain, tmpbuff);
					//advance the progress bar %
					SendDlgItemMessage( g_hWndMain, IDC_PROGRESS1, 
						PBM_SETPOS, Position * 100 / FileSize, 0 );

				}
				else if (ret < 0)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
						goto CleanUp;
					Sleep(0);
				}
			}
		} else 
			goto CleanUp;
	}

CleanUp:
	WSAAsyncSelect(hSocket, g_hWndMain, WM_MAINSOCKET, FD_CLOSE | FD_READ | FD_WRITE);
	ioctlsocket(hSocket, FIONBIO, (u_long *)1);
	CloseHandle(hFile);
	bTransferring = false;
	return 0;
}


DWORD WINAPI DownloadSendFileThread(LPVOID lpArgs)
{
	HANDLE hFile = ((FILETRANSFERARGS *)lpArgs)->hFile;
	SOCKET hSocket = ((FILETRANSFERARGS *)lpArgs)->hSocket;
	DWORD FileSize = ((FILETRANSFERARGS *)lpArgs)->FileSize;
	DWORD Position = ((FILETRANSFERARGS *)lpArgs)->Offset;

	DWORD BytesRead, SendBytes;
	BYTE Buffer[4096];
	BYTE *Packet;
	int ret;
	TIMEVAL tm;
	fd_set fs;
	char tmpbuff[512];

	WSAAsyncSelect(hSocket, g_hWndMain, 0, 0);
	//	ioctlsocket(hSocket, FIONBIO, 0);

	linger lin;
	lin.l_onoff=lin.l_linger=0;

	int opt=1;
	setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_LINGER, (char *)&lin, sizeof(lin));

	u_long noblock=0;
	ioctlsocket(hSocket, FIONBIO, &noblock);

	free(lpArgs);
	Position = 0;

	SetFilePointer(hFile, Position, NULL, FILE_BEGIN);

	FD_ZERO(&fs);
	FD_SET(hSocket, &fs);
	tm.tv_sec = 60;
	tm.tv_usec = 0;

	while (Position < FileSize)
	{
		ReadFile(hFile, Buffer, 4096, &BytesRead, NULL);
		if (BytesRead > 0)
		{
			Position += BytesRead;
			Packet = Buffer;
			if (select(0, NULL, &fs, NULL, &tm) > 0) {
				if (FD_ISSET(hSocket, &fs)) {
					do
					{
						if (BytesRead >= 4096)
							SendBytes = 4096;
						else
							SendBytes = BytesRead;
						ret = send(hSocket, (char *)Packet, SendBytes, 0);
						//					SetRichText(CON_BROWN, "[%d] ret: %d\r\n", Position * 100 / FileSize, ret);
						wsprintf(tmpbuff, "%d:%i", Position * 100 / FileSize, ret);
						SetWindowText(g_hWndMain, tmpbuff);
						if (ret > 0)
						{
							BytesRead -= ret;
							Packet += ret;
						}
						else if (ret < 0)
						{
							if (WSAGetLastError() != WSAEWOULDBLOCK)
								goto CleanUp;
							Sleep(0);
						}
					} while (BytesRead > 0);
				}
			} else 
				goto CleanUp;
		}
	}

CleanUp:

	WSAAsyncSelect(hSocket, g_hWndMain, WM_MAINSOCKET, FD_CLOSE | FD_READ | FD_WRITE);
	ioctlsocket(hSocket, FIONBIO, (u_long *)1);
	CloseHandle(hFile);

	bTransferring = false;

	char *msg;
	msg = "wizard";
	unsigned long msglen = lstrlen(msg);

	for (int ci=0; ci<ClientCount; ci++)
	{
		if (Clients[ci].hSocket == hSocket)
		{
			Clients[ci].Transferring = true;
			Sleep(100);
			DownloadSendData(119, 1, &msg, &msglen, ci);
			break;
		}
	}

	return 0;
}


void DownloadStartSendThread(SOCKET wParam, unsigned short ci)
{
	FILETRANSFERARGS *lpArgs;
	HANDLE h;
	DWORD dw;

	lpArgs = (FILETRANSFERARGS *)malloc(sizeof(FILETRANSFERARGS));
	lpArgs->hFile = Clients[ci].hFile;
	lpArgs->hSocket = wParam;
	lpArgs->FileSize = GetFileSize(Clients[ci].hFile, NULL);
	lpArgs->Offset = 0;

	if (lpArgs->Offset < lpArgs->FileSize)
	{
		h = CreateThread(NULL, 0, SendFileThread, lpArgs, CREATE_SUSPENDED, &dw);
		if (h == NULL)
		{
			CloseHandle(Clients[ci].hFile);
		}
		else
		{
			ResumeThread(h);
			CloseHandle(h);
			Clients[ci].Transferring = false;
		}
	}
	else
	{
		CloseHandle(Clients[ci].hFile);
	}
}


void DownloadParseData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal, unsigned short ci)
{
	int i;
	unsigned long argl[10];
	char *argv[10], *temp;

	if (datac > 0) {
		for (i=0; i<datac; i++)
		{
			//			SetRichText(CON_RED, "cmd[%i] data[%i]: %s\r\n", cmd, i, data[i]);
		}
	}

	switch (cmd) {
	case 0:
		// Loopback cmd (used for ping)
		DownloadSendData(cmd, datac, data, datal, ci);
		break;
	case 119:
		Sleep(100);
		RECT r;
		GetWindowRect(g_hWndMain, &r);
		char *RectData[2];
		unsigned long RectLen[2];
		unsigned int RectArray[4];

		RectData[0] = "wizard";
		RectData[1] = (char *)RectArray;
		RectLen[1] = 16;

		RectArray[0] = r.top;
		RectArray[1] = r.bottom;
		RectArray[2] = r.left;
		RectArray[3] = r.right;
		RectLen[0] = lstrlen(RectData[0]);

		DownloadSendData(100, 2, RectData, RectLen, ci);
		//call for a paint
		InvalidateRect(g_hWndMain, NULL, FALSE);
		UpdateWindow(g_hWndMain);
		break;
	case 129:
		//		 Download file
		if (datac > 1) {
			HANDLE hFile;
			DWORD dw;
			FILETRANSFERARGS *pArgs;
			pArgs = (FILETRANSFERARGS *)malloc(sizeof(FILETRANSFERARGS));

			int filesize;
			filesize = atoi(data[1]);

			hFile = CreateFile(data[0], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			//			Sleep(1000);
			if (hFile != INVALID_HANDLE_VALUE) {
				bTransferring = true;
				pArgs->hFile = hFile;
				pArgs->hSocket = hTcpSocket;
				pArgs->FileSize = filesize;
				WSAAsyncSelect(hTcpSocket, g_hWndMain, WM_MAINSOCKET, 0);
				ioctlsocket(hTcpSocket, FIONBIO, 0);
				CreateThread(NULL, 0, DownloadReceiveFileThread, pArgs, 0, &dw);
			}
			//fixme: else?
		}
		break;
	default:
		// Invalid command
		DownloadSendStr("Invalid command.", ci);
		break;
	}
}


void DownloadProcessQueue(unsigned short ci)
{
	char *SckData, **data;
	unsigned long Len, Index, datac, i;
	unsigned long *datal;
	unsigned short cmd;

	if (!Processing && RecvBufSize >= 4) {
		Processing = true;

		// Extract 4 byte message length
		memcpy(&Len, RecvBuffer, 4);

		if (RecvBufSize >= Len + 4) {

			// Extract 2 byte command
			memcpy(&cmd, RecvBuffer + 4, 2);

			// Extract 4 byte argument count
			memcpy(&datac, RecvBuffer + 6, 4);

			// Allocate buffers for data
			data = (char **)malloc(datac * sizeof(char *));
			datal = (unsigned long *)malloc(datac * sizeof(unsigned long));
			Len -= 6;
			SckData = (char *)malloc(Len);

			// Copy data from receive buffer
			memcpy(SckData, RecvBuffer + 10, Len);

			// Shift receive buffer to the left			
			RecvBufSize -= Len + 10;
			memcpy(RecvBuffer, RecvBuffer + Len + 10, RecvBufSize);
			//memmove(RecvBuffer, RecvBuffer + Len + 10, RecvBufSize);
			//RecvBuffer = (char *)realloc(RecvBuffer, RecvBufSize);

			// Extract arguments
			Index = 0;
			for (i = 0; i < datac; i++) {
				memcpy(&Len, SckData + Index, 4);
				Index += 4;
				datal[i] = Len;
				data[i] = (char *)malloc(Len + 1);
				memcpy(data[i], SckData + Index, Len);
				data[i][Len] = '\0'; // Insert extra null for easy string manipulation
				Index += Len;
			}
			free(SckData);
			Processing = false;

			// Parse data
			DownloadParseData(cmd, datac, data, datal, ci);

			// Free memory
			for (i = 0; i < datac; i++)
				free(data[i]);
			free(data);
			free(datal);

			DownloadProcessQueue(ci);
		}
		Processing = false;
	}
}

void DownloadParseCommand(int argc, char **argv)
{
	if (_stricmp(argv[0], "download") == 0)
	{
		int port;

		if (bConnected == false)
		{
			port = 80;
			gpHost = "127.0.0.1";

			struct sockaddr_in sin;
			struct hostent *host;

			sin.sin_family = AF_INET;
			sin.sin_port = htons((unsigned short)port);
			sin.sin_addr.S_un.S_addr = inet_addr(gpHost);

			//resolve hostname if not ip
			if (sin.sin_addr.S_un.S_addr == INADDR_NONE)
			{
				host = gethostbyname(gpHost);
				if (host != NULL)
					memcpy(&sin.sin_addr.S_un.S_addr, host->h_addr_list[0], host->h_length);

			}

			hTcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (hTcpSocket != INVALID_SOCKET)
			{
				if (connect(hTcpSocket, (struct sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
				{
					closesocket(hTcpSocket);
					//					SetRichText(CON_RED, "TCP connect failed (%d).\r\n", WSAGetLastError());
					SetWindowText(g_hWndMain, "TCP connect failed.");
					bConnected = false;
				}
				else
				{
					//					SetRichText(CON_NORMAL, "Successfully connected to %s on port %d.\r\n", inet_ntoa(sin.sin_addr), port);
					SetWindowText(g_hWndMain, "Successfully connected.");
					bConnected = true;

					ClientCount++;
					Clients = (Client *)realloc(Clients, ClientCount * sizeof(Client));
					Clients[ClientCount - 1].hSocket = hTcpSocket;
					Clients[ClientCount - 1].hSin = sin;

					WSAAsyncSelect(hTcpSocket, g_hWndMain, WM_MAINSOCKET, FD_READ | FD_CLOSE);
					ioctlsocket(hTcpSocket, FIONBIO, (u_long *)1);

					Sleep(100);

					char *RectData[1];
					unsigned long RectLen[1];

					RectData[0] = argv[1];
					RectLen[0] = lstrlen(RectData[0]);

					DownloadSendData(128, 1, RectData, RectLen, ClientCount - 1);
				}
			}
		}
	}

	else if (stricmp(argv[0], "disconnect") == 0)
	{
		if (!bConnected)
		{
			SetRichText(CON_RED, "Cannot disconnect without connecting.\r\n");
		}
		else
		{
			closesocket(hTcpSocket);
			bConnected = false;
			SetRichText(CON_NORMAL, "Successfully disconnected from server.\r\n");
		}
	}

}

void RemoteDesktopCleanup(SOCKET wParam)
{
	int i, j;

	for (i=ClientCount-1; i>=0; i--)
	{
		if (Clients[i].hSocket == wParam)
		{
			//closesocket(wParam);
			for (j=i+1; j<ClientCount; j++)
			{
				Clients[j-1] = Clients[j];
			}
			ClientCount--;
		}
	}
}

void RemoteDesktopSendData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal, unsigned short ci)
{
	unsigned int Size, Index, i;

	// Calculate total message length
	Size = 6 + datac * 4;
	for (i = 0; i < datac; i++)
		Size += datal[i];

	// Allocate memory for message
	char *SckData = (char *)malloc(Size + 4); // (+ 4 to compensate for message length header)

	memcpy(SckData, &Size, 4); // Insert 4 byte message length	
	memcpy(SckData + 4, &cmd, 2); // Insert 2 byte command	
	memcpy(SckData + 6, &datac, 4); // Insert 4 byte argument count

	// Insert arguments
	Index = 10;
	for (i = 0; i < datac; i++) {
		memcpy(SckData + Index, &datal[i], 4);
		Index += 4;
		memcpy(SckData + Index, data[i], datal[i]);
		Index += datal[i];
	}

	if (SendBufSize == 0) {

		// Add data to send buffer, and try to send

		SendBufSize = Size + 4; // (+ 4 to compensate for message length header)
		SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
		memcpy(SendBuffer, SckData, SendBufSize);

		int len = send(Clients[ci].hSocket, SendBuffer, SendBufSize, 0);

		if (len > 0) {
			SendBufSize -= len;
			memmove(SendBuffer, SendBuffer + len, SendBufSize);
			SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
		}
	} 
	else {

		// Add data to send buffer, FD_WRITE messages
		// will handle the rest

		SendBufSize += Size + 4; // (+ 4 to compensate for message length header)
		SendBuffer = (char *)realloc(SendBuffer, SendBufSize);
		memcpy(SendBuffer + SendBufSize - Size - 4, SckData, Size + 4);
	}

	// Free memory
	free(SckData);
}

void RemoteDesktopSendStr(char *msg, unsigned short ci)
{
	unsigned long msglen = lstrlen(msg);
	RemoteDesktopSendData(4, 1, &msg, &msglen, ci);
}

DWORD WINAPI RemoteDesktopReceiveFileThread(LPVOID lpArgs)
{
	HANDLE hFile = ((FILETRANSFERARGS *)lpArgs)->hFile;
	SOCKET hSocket = ((FILETRANSFERARGS *)lpArgs)->hSocket;
	DWORD FileSize = ((FILETRANSFERARGS *)lpArgs)->FileSize;
	DWORD Position;
	DWORD BytesWritten, RecvBytes;
	char Buffer[4096], tmpbuff[512];
	int ret;
	TIMEVAL tm;
	fd_set fs;

	WSAAsyncSelect(hSocket, ghWnd, 0, 0);
	//	ioctlsocket(hSocket, FIONBIO, 0);

	linger lin;
	lin.l_onoff=lin.l_linger=0;

	int opt=1;
	setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_LINGER, (char *)&lin, sizeof(lin));

	u_long noblock=0;
	ioctlsocket(hSocket, FIONBIO, &noblock);

	free(lpArgs);
	Position = 0;

	FD_ZERO(&fs);
	FD_SET(hSocket, &fs);
	tm.tv_sec = 60;
	tm.tv_usec = 0;

	while (Position < FileSize)
	{
		if (select(0, &fs, NULL, NULL, &tm) > 0) {
			if (FD_ISSET(hSocket, &fs)) {

				if (FileSize-Position >= 4096)
					RecvBytes = 4096;
				else
					RecvBytes = FileSize-Position;

				ret = recv(hSocket, Buffer, RecvBytes, 0);
				//			SetRichText(CON_BROWN, "[%d] ret: %d\r\n", Position * 100 / FileSize, ret);
				if (ret > 0)
				{
					WriteFile(hFile, Buffer, ret, &BytesWritten, NULL);
					Position += ret;
					wsprintf(tmpbuff, "%d:%i", Position * 100 / FileSize, ret);
					SetWindowText(ghWnd, tmpbuff);
				}
				else if (ret < 0)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
						goto CleanUp;
					Sleep(0);
				}
			}
		} else 
			goto CleanUp;
	}

CleanUp:
	WSAAsyncSelect(hSocket, ghWnd, WM_MAINSOCKET, FD_CLOSE | FD_READ | FD_WRITE);
	ioctlsocket(hSocket, FIONBIO, (u_long *)1);
	CloseHandle(hFile);
	gbTransferring = false;
	return 0;
}

DWORD WINAPI RemoteDesktopSendFileThread(LPVOID lpArgs)
{
	HANDLE hFile = ((FILETRANSFERARGS *)lpArgs)->hFile;
	SOCKET hSocket = ((FILETRANSFERARGS *)lpArgs)->hSocket;
	DWORD FileSize = ((FILETRANSFERARGS *)lpArgs)->FileSize;
	DWORD Position = ((FILETRANSFERARGS *)lpArgs)->Offset;

	DWORD BytesRead, SendBytes;
	BYTE Buffer[4096];
	BYTE *Packet;
	int ret;
	TIMEVAL tm;
	fd_set fs;
	char tmpbuff[512];

	WSAAsyncSelect(hSocket, ghWnd, 0, 0);
	//	ioctlsocket(hSocket, FIONBIO, 0);

	linger lin;
	lin.l_onoff=lin.l_linger=0;

	int opt=1;
	setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt));
	setsockopt(hSocket, SOL_SOCKET, SO_LINGER, (char *)&lin, sizeof(lin));

	u_long noblock=0;
	ioctlsocket(hSocket, FIONBIO, &noblock);

	free(lpArgs);
	Position = 0;

	SetFilePointer(hFile, Position, NULL, FILE_BEGIN);

	FD_ZERO(&fs);
	FD_SET(hSocket, &fs);
	tm.tv_sec = 60;
	tm.tv_usec = 0;

	while (Position < FileSize)
	{
		ReadFile(hFile, Buffer, 4096, &BytesRead, NULL);
		if (BytesRead > 0)
		{
			Position += BytesRead;
			Packet = Buffer;
			if (select(0, NULL, &fs, NULL, &tm) > 0) {
				if (FD_ISSET(hSocket, &fs)) {
					do
					{
						if (BytesRead >= 4096)
							SendBytes = 4096;
						else
							SendBytes = BytesRead;
						ret = send(hSocket, (char *)Packet, SendBytes, 0);
						//					SetRichText(CON_BROWN, "[%d] ret: %d\r\n", Position * 100 / FileSize, ret);
						wsprintf(tmpbuff, "%d:%i", Position * 100 / FileSize, ret);
						SetWindowText(ghWnd, tmpbuff);
						if (ret > 0)
						{
							BytesRead -= ret;
							Packet += ret;
						}
						else if (ret < 0)
						{
							if (WSAGetLastError() != WSAEWOULDBLOCK)
								goto CleanUp;
							Sleep(0);
						}
					} while (BytesRead > 0);
				}
			} else 
				goto CleanUp;
		}
	}

CleanUp:

	WSAAsyncSelect(hSocket, ghWnd, WM_MAINSOCKET, FD_CLOSE | FD_READ | FD_WRITE);
	ioctlsocket(hSocket, FIONBIO, (u_long *)1);
	CloseHandle(hFile);

	gbTransferring = false;

	char *msg;
	msg = "wizard";
	unsigned long msglen = lstrlen(msg);

	for (int ci=0; ci<ClientCount; ci++)
	{
		if (Clients[ci].hSocket == hSocket)
		{
			Clients[ci].Transferring = true;
			Sleep(100);
			RemoteDesktopSendData(119, 1, &msg, &msglen, ci);
			break;
		}
	}

	return 0;
}

void RemoteDesktopStartSendThread(SOCKET wParam, unsigned short ci)
{
	FILETRANSFERARGS *lpArgs;
	HANDLE h;
	DWORD dw;

	lpArgs = (FILETRANSFERARGS *)malloc(sizeof(FILETRANSFERARGS));
	lpArgs->hFile = Clients[ci].hFile;
	lpArgs->hSocket = wParam;
	lpArgs->FileSize = GetFileSize(Clients[ci].hFile, NULL);
	lpArgs->Offset = 0;

	if (lpArgs->Offset < lpArgs->FileSize)
	{
		h = CreateThread(NULL, 0, SendFileThread, lpArgs, CREATE_SUSPENDED, &dw);
		if (h == NULL)
		{
			CloseHandle(Clients[ci].hFile);
		}
		else
		{
			ResumeThread(h);
			CloseHandle(h);
			Clients[ci].Transferring = false;
		}
	}
	else
	{
		CloseHandle(Clients[ci].hFile);
	}
}

void RemoteDesktopParseData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal, unsigned short ci)
{
	int i;
	unsigned long argl[10];
	char *argv[10], *temp;

	if (datac > 0) {
		for (i=0; i<datac; i++)
		{
			//			SetRichText(CON_RED, "cmd[%i] data[%i]: %s\r\n", cmd, i, data[i]);
		}
	}

	switch (cmd) {
	case 0:
		// Loopback cmd (used for ping)
		RemoteDesktopSendData(cmd, datac, data, datal, ci);
		break;
	case 119:
		Sleep(100);
		RECT r;
		GetWindowRect(ghWnd, &r);
		char *RectData[2];
		unsigned long RectLen[2];
		unsigned int RectArray[4];

		RectData[0] = "wizard";
		RectData[1] = (char *)RectArray;
		RectLen[1] = 16;

		RectArray[0] = r.top;
		RectArray[1] = r.bottom;
		RectArray[2] = r.left;
		RectArray[3] = r.right;
		RectLen[0] = lstrlen(RectData[0]);

		RemoteDesktopSendData(100, 2, RectData, RectLen, ci);
		//call for a paint
		InvalidateRect(ghWnd, NULL, FALSE);
		UpdateWindow(ghWnd);
		break;
	case 129:
		//		 Download file
		if (datac > 1) {
			HANDLE hFile;
			DWORD dw;
			FILETRANSFERARGS *pArgs;
			pArgs = (FILETRANSFERARGS *)malloc(sizeof(FILETRANSFERARGS));

			int filesize;
			filesize = atoi(data[1]);

			hFile = CreateFile(data[0], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			//			Sleep(1000);
			if (hFile != INVALID_HANDLE_VALUE) {
				gbTransferring = true;
				pArgs->hFile = hFile;
				pArgs->hSocket = ghTcpSocket;
				pArgs->FileSize = filesize;
				WSAAsyncSelect(ghTcpSocket, ghWnd, WM_MAINSOCKET, 0);
				ioctlsocket(ghTcpSocket, FIONBIO, 0);
				CreateThread(NULL, 0, RemoteDesktopReceiveFileThread, pArgs, 0, &dw);
			}
			//fixme: else?
		}
		break;
	default:
		// Invalid command
		RemoteDesktopSendStr("Invalid command.", ci);
		break;
	}
}

void RemoteDesktopProcessQueue(unsigned short ci)
{
	char *SckData, **data;
	unsigned long Len, Index, datac, i;
	unsigned long *datal;
	unsigned short cmd;

	if (!Processing && RecvBufSize >= 4) {
		Processing = true;

		// Extract 4 byte message length
		memcpy(&Len, RecvBuffer, 4);

		if (RecvBufSize >= Len + 4) {

			// Extract 2 byte command
			memcpy(&cmd, RecvBuffer + 4, 2);

			// Extract 4 byte argument count
			memcpy(&datac, RecvBuffer + 6, 4);

			// Allocate buffers for data
			data = (char **)malloc(datac * sizeof(char *));
			datal = (unsigned long *)malloc(datac * sizeof(unsigned long));
			Len -= 6;
			SckData = (char *)malloc(Len);

			// Copy data from receive buffer
			memcpy(SckData, RecvBuffer + 10, Len);

			// Shift receive buffer to the left			
			RecvBufSize -= Len + 10;
			memcpy(RecvBuffer, RecvBuffer + Len + 10, RecvBufSize);
			//memmove(RecvBuffer, RecvBuffer + Len + 10, RecvBufSize);
			//RecvBuffer = (char *)realloc(RecvBuffer, RecvBufSize);

			// Extract arguments
			Index = 0;
			for (i = 0; i < datac; i++) {
				memcpy(&Len, SckData + Index, 4);
				Index += 4;
				datal[i] = Len;
				data[i] = (char *)malloc(Len + 1);
				memcpy(data[i], SckData + Index, Len);
				data[i][Len] = '\0'; // Insert extra null for easy string manipulation
				Index += Len;
			}
			free(SckData);
			Processing = false;

			// Parse data
			RemoteDesktopParseData(cmd, datac, data, datal, ci);

			// Free memory
			for (i = 0; i < datac; i++)
				free(data[i]);
			free(data);
			free(datal);

			RemoteDesktopProcessQueue(ci);
		}
		Processing = false;
	}
}

LRESULT CALLBACK WndRemoteDesktopProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC	hDC, hMemDC;
	BITMAP bm;
	RECT r;
	PAINTSTRUCT ps;
	HBITMAP hBMP;
	HBITMAP hOrigBMP;

	switch(uMsg)
	{
	case WM_CREATE:
		break;
	case WM_NOTIFY:
		break;
	case WM_COMMAND:
		break;
	case WM_MAINSOCKET:
		{
			switch (lParam & 0xFFFF)
			{
			case FD_CLOSE:
				{
					RemoteDesktopCleanup(wParam);
					SetWindowText(ghWnd, "ghTcpSocket: Connection broken between socket and server.");
					gbConnected = false;
					break;
				}
			case FD_READ:
				{
					for (int i=0; i<ClientCount; i++)
						if (Clients[i].hSocket == wParam)
						{
							memset(Clients[i].SocketBuffer, 0, sizeof(Clients[i].SocketBuffer));
							int len = recv(wParam, Clients[i].SocketBuffer, sizeof(Clients[i].SocketBuffer), 0);
							if (len > 0)
							{
								RecvBufSize += len;
								RecvBuffer = (char *)realloc(RecvBuffer, RecvBufSize);
								memcpy(RecvBuffer + RecvBufSize - len, Clients[i].SocketBuffer, len);
								RemoteDesktopProcessQueue(i);
								//								SetRichText(CON_NORMAL, "Main (%i): %s\r\n", i, Clients[i].SocketBuffer);
							}
							break;
						}

				}
			case FD_WRITE:
				{
					int Len;

					for (int i=0; i<ClientCount; i++)
						if (Clients[i].hSocket == wParam)
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
	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
		}
		break;
	case WM_ERASEBKGND:
		return 1;
		break;
	case WM_MOVE:
		//call for a paint
		InvalidateRect(hWnd, NULL, FALSE);
		UpdateWindow(hWnd);
		break;
	case WM_PAINT:
		{
			hDC = BeginPaint(hWnd, &ps);

			//load image
			hBMP = (HBITMAP)LoadImage((HINSTANCE)hWnd, "captureqwsx.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
			//get a compatible DC
			//			hDC = GetDC(hWnd);
			//get mem DC
			hMemDC = CreateCompatibleDC(hDC);
			//select in our bitmap
			hOrigBMP = (HBITMAP)SelectObject(hMemDC, hBMP);
			//use hMemDC to draw bitmap to screen DC (hDC)
			GetObject(hBMP, sizeof(BITMAP), &bm);
			//			BitBlt(hDC, 0, 0, bm.bmWidth, bm.bmHeight, hMemDC, 0, 0, SRCCOPY);

			GetWindowRect(hWnd, &r);
			StretchBlt(hDC, 0, 0, r.right-r.left-8, r.bottom-r.top-32, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

			//clean up
			//put hMemDC back the way we created it
			SelectObject(hMemDC, hOrigBMP);
			//delete, release
			//			ReleaseDC(hWnd, hDC);
			DeleteObject(hBMP);
			DeleteObject(hMemDC);

			EndPaint(hWnd, &ps);
			break;
		}
	case WM_CLOSE:
		{
			// Do not destroy window yet
			ShowWindow(hWnd, SW_HIDE);
			char *rdbuffer[1];
			rdbuffer[0] = "disconnect";
			RemoteDesktopParseCommand(1, rdbuffer);
			return 0;
		}
	case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
	case WM_SIZE:
		break;
	case WM_GETMINMAXINFO:
		{
			//Set window size constraints
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = 500;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = 300;
			break;
		}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void RemoteDesktopParseCommand(int argc, char **argv)
{
	if (_stricmp(argv[0], "connect") == 0)
	{
		int port;

		if (gbConnected == false)
		{
			port = 80;
			gpHost = "127.0.0.1";

			struct sockaddr_in sin;
			struct hostent *host;

			sin.sin_family = AF_INET;
			sin.sin_port = htons((unsigned short)port);
			sin.sin_addr.S_un.S_addr = inet_addr(gpHost);

			//resolve hostname if not ip
			if (sin.sin_addr.S_un.S_addr == INADDR_NONE)
			{
				host = gethostbyname(gpHost);
				if (host != NULL)
					memcpy(&sin.sin_addr.S_un.S_addr, host->h_addr_list[0], host->h_length);

			}

			ghTcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (ghTcpSocket != INVALID_SOCKET)
			{
				if (connect(ghTcpSocket, (struct sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
				{
					closesocket(ghTcpSocket);
					//					SetRichText(CON_RED, "TCP connect failed (%d).\r\n", WSAGetLastError());
					SetWindowText(ghWnd, "TCP connect failed.");
					gbConnected = false;
				}
				else
				{
					//					SetRichText(CON_NORMAL, "Successfully connected to %s on port %d.\r\n", inet_ntoa(sin.sin_addr), port);
					SetWindowText(ghWnd, "Successfully connected.");
					gbConnected = true;

					ClientCount++;
					Clients = (Client *)realloc(Clients, ClientCount * sizeof(Client));
					Clients[ClientCount - 1].hSocket = ghTcpSocket;
					Clients[ClientCount - 1].hSin = sin;

					WSAAsyncSelect(ghTcpSocket, ghWnd, WM_MAINSOCKET, FD_READ | FD_CLOSE);
					ioctlsocket(ghTcpSocket, FIONBIO, (u_long *)1);

					Sleep(100);

					RECT r;
					GetWindowRect(ghWnd, &r);
					char *RectData[2];
					unsigned long RectLen[2];
					unsigned int RectArray[4];

					RectData[0] = "wizard";
					RectData[1] = (char *)RectArray;
					RectLen[1] = 16;

					RectArray[0] = r.top;
					RectArray[1] = r.bottom;
					RectArray[2] = r.left;
					RectArray[3] = r.right;
					RectLen[0] = lstrlen(RectData[0]);

					RemoteDesktopSendData(100, 2, RectData, RectLen, ClientCount - 1);
				}
			}
		}
	}

	else if (stricmp(argv[0], "disconnect") == 0)
	{
		if (!gbConnected)
		{
			SetRichText(CON_RED, "Cannot disconnect without connecting.\r\n");
		}
		else
		{
			closesocket(ghTcpSocket);
			gbConnected = false;
			SetRichText(CON_NORMAL, "Successfully disconnected from server.\r\n");
		}
	}
}

int WINAPI WinRemoteDesktop(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Initialize global variables
	ghInstance = GetModuleHandle(0);

	static char szClassName[] = "ConsoleRemoteDesktop";
	static char szWindowName[] = "ConsoleRemoteDesktop";

	// Define & register window class
	WNDCLASSEX WndClass;
	MSG msg;

	memset(&WndClass, 0, sizeof(WndClass));
	WndClass.cbSize = sizeof(WndClass);						// size of structure
	WndClass.lpszClassName = szClassName;					// name of window class
	WndClass.lpfnWndProc = WndRemoteDesktopProc;							// points to a window procedure
	WndClass.hInstance = ghInstance;						// handle to instance
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
	ghWnd = CreateWindowEx(WS_EX_WINDOWEDGE, szClassName, szWindowName, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 300, 0, 0, ghInstance, 0);

	if (!ghWnd)
	{
		return 0;
	}

	ShowWindow(ghWnd, nCmdShow);
	UpdateWindow(ghWnd);

	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
		if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)
			return false;

	return true;
}
