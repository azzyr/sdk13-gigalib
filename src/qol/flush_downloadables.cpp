#include <cbase.h>

#if defined (CLIENT_DLL) && defined(ENGINE_DETOURS)
class flushDLS : public CAutoGameSystem
{
public:
	flushDLS();
	bool Init() override;
	void PostInit() override;
};

#include <filesystem.h>

#include "tier0/valve_minmax_off.h"
#include <filesystem>
#include "tier0/valve_minmax_on.h"

#include <icvar.h>
#include <convar.h>

// #define Debug_FlushDLs yep

#ifdef Debug_FlushDLs
	Color grn(0, 255, 0, 255);
	Color ylw(255, 255, 0, 255);
	Color red(255, 0, 0, 255);
#endif

enum FLUSH_CUSTOM_CONTENT
{
	FLUSH_ALL = 1,
	FLUSH_SPRAYS = 2,
	FLUSH_MAP_OVERRIDES = 3,
};

void FlushContent(FLUSH_CUSTOM_CONTENT FLUSH)
{
	size_t totalflushed = 0;
	char path[MAX_PATH] = {};
	char glob[16] = {};

	if (FLUSH != FLUSH_MAP_OVERRIDES)
	{
		// set in memy init. probably should move that someday
		ConVar* _modpath = cvar->FindVar("_modpath");
		if (!_modpath)
		{
			Warning("NULL ConVar* _modpath? Report this to a developer! Failing...\n");
			return;
		}
		// yeah, i'm an empath
		char* empath = const_cast<char*>(_modpath->GetString());
		if (!empath || empath[0] == '\0')
		{
			Warning("NULL char* modpath? Report this to a developer! Failing...\n");
			return;
		}
		V_StripTrailingSlash(empath);
		// c++17 my beloved
		std::filesystem::path mpath = empath;
		if (!std::filesystem::exists(mpath))
		{
			Warning("Modpath doesn't exist? Report this to a developer! Failing...\n");
			return;
		}

		if (FLUSH == FLUSH_ALL)
		{
			mpath = mpath / "download";
		}
		else if (FLUSH == FLUSH_SPRAYS)
		{
			mpath = mpath / "download" / "user_custom";
		}
		else
		{
			Warning("Invalid FLUSH option %i? [0]\n", FLUSH);
			return;
		}

		std::string str_mpath = mpath.string();
		const char* ch_mpath = str_mpath.c_str();

		if (std::filesystem::exists(mpath) && std::filesystem::is_directory(mpath))
		{
			uintmax_t removed = std::filesystem::remove_all(mpath);
			Msg("Flushed %llu items total from %s!\n", removed, ch_mpath);
			bool mkdir = std::filesystem::create_directory(mpath);
			if (mkdir)
			{
				Msg("Successfully recreated folder %s.\n", ch_mpath);
			}
			else
			{
				Warning("Failed to recreate folder %s! Your game might crash.\n", ch_mpath);
			}
		}
		else
		{
			Warning("Couldn't find folder %s! Not flushing...\n", ch_mpath);
		}
		return;
	}
	else if (FLUSH == FLUSH_MAP_OVERRIDES)
	{
		V_strncpy(path, "download/maps", sizeof(path));
		V_strncpy(glob, "*.txt", sizeof(glob));
	}
	else
	{
		Warning("Invalid FLUSH option %i? [1]\n", FLUSH);
		return;
	}

	// smoosh em into one string
	char pathglob[MAX_PATH] = {};
	V_snprintf(pathglob, MAX_PATH, "%s%s%s", path, CORRECT_PATH_SEPARATOR_S, glob);

	// find files containing pathglob
	FileFindHandle_t ffhand = FILESYSTEM_INVALID_FIND_HANDLE;
	const char* filename = g_pFullFileSystem->FindFirstEx(pathglob, "GAME", &ffhand);

	// is it a real file? if it's not, that means there's nothin found here.
	if (!filename || ffhand == FILESYSTEM_INVALID_FIND_HANDLE)
	{
#ifdef Debug_FlushDLs
		ConColorMsg(red, "---> No files found in this path!\n\n");
#endif
		Msg("Removed all downloaded map overrides [%i items total]\n", totalflushed);
		return;
	}

	for (; filename != nullptr; filename = g_pFullFileSystem->FindNext(ffhand))
	{
		// get full path name relative to game folder
		char fullpath[MAX_PATH] = {};
		V_snprintf(fullpath, MAX_PATH, "%s%s%s", path, CORRECT_PATH_SEPARATOR_S, filename);

		totalflushed++;
		g_pFullFileSystem->RemoveFile(fullpath);

#ifdef Debug_FlushDLs
		ConColorMsg(ylw, "--->---> Found file = %s\n", filename);
		ConColorMsg(ylw, "--->---> Fullpath of file = %s\n", fullpath);
#endif

		continue;
	}
	g_pFullFileSystem->FindClose(ffhand);

	Msg("Removed all downloaded map overrides [%i items total]\n", totalflushed);
}

void CC_FlushMapOverrides(const CCommand& args)
{
	FlushContent(FLUSH_MAP_OVERRIDES);
}

// There is no cvar. This is not optional
static ConCommand flush_map_overrides("flush_map_overrides", CC_FlushMapOverrides,
	"Delete all downloaded overrides for maps, including particles, soundscripts, and item schemas."
	"This does not delete maps with packed content, only \"bare\" .txt files."
	"This command is automatically run every time the client disconnects from a server, and on game boot.\n", FCVAR_NONE);

void CC_FlushSprays(const CCommand& args)
{
	FlushContent(FLUSH_SPRAYS);
}
ConVar cl_auto_flush_sprays("cl_auto_flush_sprays", "1", FCVAR_ARCHIVE, "Flush all sprays on client disconnect, and on game boot.\n");
static ConCommand flush_sprays("flush_sprays", CC_FlushSprays, "Delete all downloaded client sprays.\n", FCVAR_NONE);

void CC_FlushDLs(const CCommand& args)
{
	FlushContent(FLUSH_ALL);
}
ConVar cl_auto_flush_downloads("cl_auto_flush_downloads", "0", FCVAR_ARCHIVE, "Flush all downloaded files on client disconnect, and on game boot.\n");
static ConCommand flush_dls("flush_dls", CC_FlushDLs, "Delete all third party custom content downloaded from servers. This is irreversible!\n", FCVAR_NONE);

#if 0

#include <fstream>
void HandleInvalidFile(const std::filesystem::path fpath)
{
	// i can't tell if this is just masking the error that VS was giving me or not
	// but it was complaining about a dangling ptr with
	// const char* path = fpath.string().c_str();
	char* path = new char[MAX_PATH] {};
	V_strncpy(path, fpath.string().c_str(), MAX_PATH);
#ifndef Debug_FlushDLs
	bool rm = std::filesystem::remove(fpath);
	if (rm)
	{
		ConColorMsg(Color(255, 0, 0, 255), "Removed invalid file %s, please report the server you just left to the developers!\n", path);
	}
	else
	{
		Warning("COULDN'T REMOVE INVALID FILE %s!\n", path);
	}
#else
	ConColorMsg(red, "Not removing invalid file %s in dbg!\n", path);
#endif
}

#include <bspfile.h>
#include <vtf/vtf.h>
#include <studio.h>
void CheckDLValidity()
{
	ConVar* _modpath = cvar->FindVar("_modpath");
	if (!_modpath)
	{
		Warning("NULL ConVar* _modpath? Report this to a developer! Failing...\n");
		return;
	}
	// yeah, i'm an empath
	char* empath = const_cast<char*>(_modpath->GetString());
	if (!empath || empath[0] == '\0')
	{
		Warning("NULL char* modpath? Report this to a developer! Failing...\n");
		return;
	}
	V_StripTrailingSlash(empath);

	// c++17 my beloved
	std::filesystem::path mpath = empath;
#ifdef Debug_FlushDLs
	for (const auto& entry : std::filesystem::recursive_directory_iterator(mpath))
#else
	for (const auto& entry : std::filesystem::recursive_directory_iterator(mpath / "download"))
#endif
	{
		if (!entry.is_regular_file() || entry.is_symlink())
		{
			continue;
		}
		std::filesystem::path epath = entry.path();
		std::string strpath = epath.string();
		const char* charpath = strpath.c_str();
		const char* charext = V_GetFileExtension(charpath);
		if (!charext)
		{
			//Warning("-> no ext for %s\n", charpath);
			continue;
		}
		if
			(
				V_stricmp(charext, "bsp") != 0 // not bsp
				&& V_stricmp(charext, "vtf") != 0 // not vtf
				&& V_stricmp(charext, "mdl") != 0 // not mdl
				&& V_stricmp(charext, "pcf") != 0 // not pcf
				)
		{
			// Warning("-> not valid ext for %s\n", charpath);
			continue;
		}
		std::ifstream curfile;
		curfile.open(epath, std::ios::binary);
		curfile.unsetf(std::ios::skipws);
		if (!curfile.is_open())
		{
#ifdef Debug_FlushDLs
			Warning("-> file is not open?!\n");
#endif
			continue;
		}
		// UPDATE THIS IF YOU EVER ADD OTHER FILES WITH BIGGER HEADERS TO THIS FUNCTION
		constexpr size_t biggest_header = sizeof(dheader_t);
		constexpr size_t smallest_header = sizeof(VTFFileBaseHeader_t);

		// sanity check
		if (entry.file_size() <= smallest_header)
		{
			curfile.close();
			HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
			Warning("Downloaded file is too small!\n");
#endif
			continue;
		}

		// need it to be unsigned so we can memcmp on it properly
		constexpr unsigned char fheader[biggest_header + 1] = {};

		// get first <x> bytes from pos 0 - don't overrun the file if we can't get all of them!
		// this can happen for really tiny vtfs!!
		curfile.seekg(0).read((char*)fheader, Min((uintmax_t)(biggest_header), entry.file_size()));

		if (V_stricmp(charext, "bsp") == 0)
		{
			// sanity check - smallest i have seen is like 10kbytes
			if (entry.file_size() <= (4 * 1000))
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("Downloaded file is too small!\n");
#endif
				continue;
			}

			// make a new header struct
			dheader_t* thisheader = new dheader_t;
			// copy data we already grabbed from this file into this local var
			memcpy((void*)(thisheader), fheader, sizeof(dheader_t));
			// now we can act on it like an actual struct (which it is)
			if (!thisheader || !thisheader->ident)
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("[BSP] Invalid header/ident");
#endif
			}
			delete thisheader;
			continue;
		}
		else if (V_stricmp(charext, "vtf") == 0)
		{
			// sanity check - smallest i have seen is 96 bytes
			if (entry.file_size() <= 64)
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("Downloaded file is too small!\n");
#endif
				continue;
			}

			// etc
			VTFFileBaseHeader_t* thisheader = new VTFFileBaseHeader_t;
			memcpy((void*)(thisheader), fheader, sizeof(VTFFileBaseHeader_t));
			if (!thisheader || !thisheader->headerSize)
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("[VTF] Invalid header/headerSize");
#endif
			}

			delete thisheader;
			continue;
		}
		else if (V_stricmp(charext, "mdl") == 0)
		{
			// sanity check
			if (entry.file_size() <= sizeof(studiohdr_t))
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("Downloaded file is too small!\n");
#endif
				continue;
			}

			studiohdr_t* thisheader = new studiohdr_t;
			memcpy((void*)(thisheader), fheader, sizeof(studiohdr_t));
			if (!thisheader || !thisheader->checksum || !thisheader->hitboxsetindex)
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("[MDL] Invalid header/headerSize");
#endif
			}
			delete thisheader;
			continue;
		}
		else if (V_stricmp(charext, "pcf") == 0)
		{
			// https://developer.valvesoftware.com/wiki/PCF_File_Format
			// too lazy to properly parse this since it's not in a .h and valve dev wiki gives conflicting answers
			// header should do for now
			// it's 43 and not 44 since we ignore the \0 at the end of the str
			int headersize = 43;
			const char* ob_pcf_header = "<!-- dmx encoding binary 2 format pcf 1 -->";

			// sanity check
			if (entry.file_size() <= headersize)
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("Downloaded file is too small!\n");
#endif
				continue;
			}
			if (memcmp(fheader, ob_pcf_header, headersize) != 0)
			{
				curfile.close();
				HandleInvalidFile(epath);
#ifdef Debug_FlushDLs
				Warning("[PCF] Invalid header");
#endif
				continue;
			}
			continue;
		}
	}
}
static ConCommand verify_dls("verify_dls", CheckDLValidity, "Verify integrity of files downloaded from community servers.\n", FCVAR_NONE);
#endif

static flushDLS g_flushDLS;
flushDLS::flushDLS() : CAutoGameSystem("flushDLS")
{
}
bool flushDLS::Init()
{
	return true;
}

void flushDLS::PostInit()
{
	// we want to do this asap... but we need to wait on memy...
	FlushContent(FLUSH_MAP_OVERRIDES);

	if (cvar->FindVar("cl_auto_flush_downloads")->GetBool())
	{
		engine->ClientCmd_Unrestricted("flush_dls\n");
	}
	else if (cvar->FindVar("cl_auto_flush_sprays")->GetBool())
	{
		engine->ClientCmd_Unrestricted("flush_sprays\n");
	}
}
// </sappho>
#endif
