#ifndef SHARED_h
#define SHARED_h

#include <windows.h>
#include <winsock2.h>

// ------------- Function Implementations ------------------

typedef unsigned long ULONG;

struct MD5_CTX {
	ULONG i[2];
	ULONG buf[4];
	unsigned char in[64];
	unsigned char digest[16];
};

void MD5Init(MD5_CTX*);
void MD5Update(MD5_CTX*, unsigned char* input, unsigned length);
void MD5Final(unsigned char hash[16], MD5_CTX*);

#endif // MD5_h