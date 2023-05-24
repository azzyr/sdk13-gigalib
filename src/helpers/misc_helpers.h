#ifndef MISC_HELPERS_H
#define MISC_HELPERS_H

#ifdef _WIN32
#pragma once
#endif
#include <cbase.h>

#include <obfuscate.h>
#include <vtf/vtf.h>
#include <netadr.h>
#include <inetchannelinfo.h>
#include <inetchannel.h>
#include <eiface.h>
#include <cdll_int.h>

#ifdef CLIENT_DLL
#include <cdll_client_int.h>
#include <icommandline.h>
#include <filesystem.h>

bool UTIL_CheckRealRemoteAddr(netadr_t netaddr);
bool UTIL_GetRealRemoteAddr(char* ipadr);
#endif // clientdll
#include <vector>
#include <string>
bool UTIL_IsFakePlayer(CBasePlayer* inplayer);
bool UTIL_IsVTFValid(const char* fileloc);
void UTIL_AddrToString(void* inAddr, char outAddrStr[11]);


// https://stackoverflow.com/a/57346888
//
std::vector<std::string> UTIL_SplitSTDString(const std::string& i_str, const std::string& i_delim);

// https://stackoverflow.com/a/49066369
#include <chrono>
// Get time stamp in milliseconds.
uint64_t millis();
// Get time stamp in microseconds.
uint64_t micros();
// Get time stamp in nanoseconds.
uint64_t nanos();


#endif

