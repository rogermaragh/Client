#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include "main.h"
#include "list.h"
#include "tree.h"
#include "plugin.h"
#include "resource.h"

//Global variables
int PluginCount = 0;
ConsolePlugin *Plugins = NULL;


void SendPluginMessage(char *PluginName, unsigned short cmd, unsigned short argc, 
	char **argv, unsigned long *argl, unsigned short ci)
{
	char **pargv;
	unsigned long *pargl;
	unsigned short i;

	pargv = (char **)malloc((argc) * sizeof(*pargv));
	pargl = (unsigned long *)malloc((argc) * sizeof(*pargl));

	if (!(pargv && pargl))
	{
		return;
	}

	for (i = 0; i < argc; i++)
	{
		pargv[i] = argv[i + 2];
		pargl[i] = (unsigned long)lstrlen(argv[i + 2]);
	}

	Plugins[ci].lspParseMsg(cmd, argc - 2, pargv, pargl, 0);

	free(pargv);
	free(pargl);
}


void lsGetCfg(char *Name, char **Value)
{
}

void lsSendMsg(unsigned short cmd, unsigned short argc, char **args, unsigned long *argl, unsigned short ci)
{
	switch (cmd)
	{
		case 100:
			SetRichText(CON_NORMAL, "%s\r\n", args[0]); 
			break;
		default:
		{
			for (int i=0; i<argc; i++)
				MessageBox(0, args[i], "lsSendMsg", 0);
		}
	}
}

void LoadPlugins()
{
	WIN32_FIND_DATA fileData;
	HANDLE hFile;
	HMODULE Handle;
	PluginInfo pi;
	PluginData pd;

	SetRichText(CON_GREEN, "Browsing for local plugins...\r\n");
	SetRichText(CON_BLUE, "usage: ");
	SetRichText(CON_GREEN, "sendmsg (plugin name) (cmd) (argv)\r\n");
	hFile = FindFirstFile("*.dll", &fileData);
	if (hFile != INVALID_HANDLE_VALUE)
		do {
			Handle = LoadLibrary(fileData.cFileName);
			if (Handle != 0)
			{
				CFT_lspGetInfo *pGetInfo = (CFT_lspGetInfo *)GetProcAddress(Handle, "lspGetInfo");
				CFT_lspInitialize *pInitialize = (CFT_lspInitialize *)GetProcAddress(Handle, "lspInitialize");
				CFT_lspCleanup *pCleanup = (CFT_lspCleanup *)GetProcAddress(Handle, "lspCleanup");
				CFT_lspParseMsg *pParseMsg = (CFT_lspParseMsg *)GetProcAddress(Handle, "lspParseMsg");
				if (pGetInfo && pInitialize && pCleanup && pParseMsg)
				{
					PluginCount++;
					Plugins = (ConsolePlugin *)realloc(Plugins, PluginCount * sizeof(ConsolePlugin));

					pGetInfo(&pi);
					
					Plugins[PluginCount - 1].lspGetInfo = pGetInfo;
					Plugins[PluginCount - 1].lspInitialize = pInitialize;
					Plugins[PluginCount - 1].lspCleanup = pCleanup;
					Plugins[PluginCount - 1].lspParseMsg = pParseMsg;
					Plugins[PluginCount - 1].Handle = Handle;
					Plugins[PluginCount - 1].Name = pi.Name;
                    strcpy((char *)Plugins[PluginCount - 1].fileName, fileData.cFileName);

					pd.pGetCfg = lsGetCfg;
					pd.pSendMsg = lsSendMsg;
					pInitialize(&pd);

					SetRichText(CON_PURPLE, "%s (%s) loaded...\r\n", Plugins[PluginCount - 1].Name, Plugins[PluginCount - 1].fileName);
				}
				else
				{
					FreeLibrary(Handle);
				}
			}
		} while (FindNextFile(hFile, &fileData) != 0);
}

void UnloadPlugins()
{
	for (int i = 0; i<PluginCount; i++)
		FreeLibrary(Plugins[i].Handle);
}
