#ifndef _PLUGIN_H_
#define _PLUGIN_H

void LoadPlugins();
void UnloadPlugins();
void SendPluginMessage(char *PluginName, unsigned short cmd, unsigned short argc, 
	char **argv, unsigned long *argl, unsigned short ci);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (CFT_lsSendMsg)(unsigned short, unsigned short, char **, unsigned long *, unsigned short);
typedef void (CFT_lsGetCfg)(char *, char **);

typedef struct {
	char *Name;
} PluginInfo;

typedef struct {
	CFT_lsSendMsg *pSendMsg;
	CFT_lsGetCfg *pGetCfg;
} PluginData;

typedef void (CFT_lspGetInfo)(PluginInfo *);
typedef void (CFT_lspInitialize)(PluginData *);
typedef void (CFT_lspCleanup)(void);
typedef void (CFT_lspParseMsg)(unsigned short, unsigned short, char **, unsigned long *, unsigned short);

typedef struct {
    CFT_lspGetInfo *lspGetInfo;
	CFT_lspInitialize *lspInitialize;
	CFT_lspCleanup *lspCleanup;
	CFT_lspParseMsg *lspParseMsg;
	HMODULE Handle;
	char *Name;
	char fileName[MAX_PATH + 1];
} ConsolePlugin;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern int PluginCount;
extern ConsolePlugin *Plugins;

#endif