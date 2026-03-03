#ifndef _TCP_H_
#define _TCP_H


#include <winsock2.h>

#define WM_MAINSOCKET (WM_USER + 1)
#define WM_QUERYSOCKET (WM_USER + 2)
#define WM_FILESOCKET (WM_USER + 3)
#define WM_VIDEOSOCKET (WM_USER + 4)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProcessQueue(void);
void SendStr(char *cmd, char *msg);
void SendData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal);
void ParseData(unsigned short cmd, unsigned long datac, char **data, unsigned long *datal);

char* FileExtension(char *FileName);
char* FileSizeString(char Buffer[1024], DWORD Number);
void FileAttributesString(DWORD attr, char *buff);
int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
void UpdateListing(char *NewPath, bool NoCache);
void UpOneLevel(char *Path);
bool InputBox(char *caption, char *text, char *dst, int len);

void ProcessBuffer(char *buf);
DWORD WINAPI ReceiveFileThread(LPVOID lpArgs);
DWORD WINAPI SendFileThread(LPVOID lpArgs);
void ProcessMessages();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern SOCKET g_hTcpSocket, g_hUdpSocket, g_hFileSocket, g_hVideoSocket;
extern bool g_bConnected, g_bTransferring, Processing;
extern char *LocalFileName;
extern char *RecvBuffer, *SendBuffer;
extern unsigned long RecvBufSize, SendBufSize;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SMSG_HANDSHAKE_INIT		0
#define CMSG_LOOPBACK		1
#define CMSG_CONSOLE		2
#define CTM_DIRLIST_ERROR		3
#define CTM_DIRLIST_START			4
#define CTM_DIRLIST_ITEM		5
#define CTM_DIRLIST_END		6
#define CMSG_DIRLIST_CRC	7

void TCPDisconnect();
void TCPConnect(char *Host);
void TCPReconnect();
void StartSendThread(SOCKET wParam);

typedef struct
{
	char *FileName;
	int Data[3];
} FileEntry;

typedef struct
{
	char *Directory;
	FileEntry *FileList;
	unsigned long ListCount;
	unsigned char CRC[16];
} CacheEntry;

#define SIZE_COLUMN_WIDTH 70
#define ATTR_COLUMN_WIDTH 60

extern FileEntry *g_pCurrentList;
extern int g_iListCount, g_iTotalSize, g_iItemsToCome;
extern CacheEntry *g_pFileCache;
extern int g_iCacheCount;
extern char g_szCurrentPath[MAX_PATH + 1];
extern LARGE_INTEGER g_Frequency, g_iTiming;
extern bool g_bCheckingUpdate;
extern HANDLE g_hFile;

void trim(char *s);
void rtrim( char * string, char * trim );
void ltrim( char * string, char * trim );

#endif