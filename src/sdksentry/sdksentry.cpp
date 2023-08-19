#include <cbase.h>

#if defined(CLIENT_DLL) && defined(SDKCURL) && defined(SDKSENTRY)
#include <misc_helpers.h>
#include <sdkCURL/sdkCURL.h>
#include <sdksentry/sdksentry.h>
#include <engine_memutils.h>
CSentry g_Sentry;

CSentry::CSentry()
{
    didinit     = false;
    didshutdown = false;
    //RUN_THIS_FUNC_WHEN_STEAM_INITS(&SetSteamID);
}

// #ifdef _DEBUG
void CC_SentryTest(const CCommand& args)
{
    sentry_value_t ctxinfo = sentry_value_new_object();
    sentry_value_set_by_key(ctxinfo, "test", sentry_value_new_string("test str"));
    SentryEvent("info", __FUNCTION__, "testEvent", ctxinfo);
}

ConCommand sentry_test("sentry_test", CC_SentryTest, "\n", FCVAR_NONE );
// #endif


void sentry_callback(IConVar* var, const char* pOldValue, float flOldValue)
{
    engine->ExecuteClientCmd("host_writeconfig");
}

ConVar cl_send_error_reports("cl_send_error_reports", "-1", FCVAR_ARCHIVE,
    "Enables/disables sending error reports to the developers to help improve the game.\n"
    "Error reports will include your SteamID, and any pertinent game info (class, loadout, current map, etc.) - we do not store any personally identifiable information.\n"
    "Read more at " VPC_QUOTE_STRINGIFY(SENTRY_PRIVACY_POLICY_URL) "\n"
    "-1 asks you again on game boot and disables reporting, 0 disables reporting and does not ask you again, 1 enables reporting.\n",
    sentry_callback
);

void CSentry__SentryURLCB__THUNK(const curlResponse* curlRepsonseStruct)
{
    g_Sentry.SentryURLCB(curlRepsonseStruct);
}

void CSentry::PostInit()
{
    DevMsg(2, "Sentry Postinit!\n");

    g_sdkCURL->CURLGet(VPC_QUOTE_STRINGIFY(SENTRY_URL), CSentry__SentryURLCB__THUNK);
}


void CSentry::SentryURLCB(const curlResponse* resp)
{
    if (!resp->completed || resp->failed || resp->respCode != 200)
    {
        Warning("Failed to get Sentry DSN. Sentry integration disabled.\n");
        Warning("completed = %i, failed = %i, statuscode = %i, bodylen = %i\n", resp->completed, resp->failed, resp->respCode, resp->bodyLen);
        return;
    }

    if (resp->bodyLen >= 256 || !resp->bodyLen)
    {
        Warning("sentry url response is >= 256chars || or is 0, bailing! len = %i\n", resp->bodyLen);
        return;
    }

    std::string body = resp->body;
    body = UTIL_StripCharsFromSTDString(body, '\n');
    body = UTIL_StripCharsFromSTDString(body, '\r');
    real_sentry_url = body;

    DevMsg(2, "sentry_dns %s\n", real_sentry_url.c_str());
    SentryInit();
}

// We ignore any crashes whenever the engine shuts down on linux.
// We shouldn't, but we do. Feel free to fix all the shutdown crashes if you want,
// then you can remove this
void CSentry::Shutdown()
{
#ifndef _WIN32
    didshutdown = true;
#endif
    LevelBreadcrumbs(__FUNCTION__);
}

#ifndef _WIN32
     #include <SDL.h>
#endif
// DO NOT THREAD THIS OR ANY FUNCTIONS CALLED BY IT UNDER ANY CIRCUMSTANCES
sentry_value_t SENTRY_CRASHFUNC(const sentry_ucontext_t* uctx, sentry_value_t event, void* closure)
{
    Assert(0);
    SentrySetTags();
    sentry_flush(1000);

    #ifndef _WIN32
    if (g_Sentry.didshutdown)
    {
        // Warning("NOT SENDING CRASH TO SENTRY.IO BECAUSE THIS IS A NORMAL SHUTDOWN! BUHBYE!\n");
        sentry_value_decref(event);
        return sentry_value_new_null();
    }
    #endif

    const char* crashdialogue =
    "\"BONK!\"\n\n"
    "We crashed. Sorry about that.\n"
    "If you've enabled crash reporting,\n"
    "we'll get right on that.\n";

    const char* crashtitle = 
    "SDK13Mod crash handler - written by sappho.io";


#ifdef _WIN32
    MessageBoxA(NULL, crashdialogue, crashtitle, \
        MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, crashtitle, crashdialogue, NULL);
#endif

    if (cl_send_error_reports.GetInt() <= 0 || !g_Sentry.didinit)
    {
        sentry_value_decref(event);
        return sentry_value_new_null();
    }
/*
    const char* const spew = Engine_GetSpew();

    std::ofstream confile;
    confile.open(g_Sentry.sentry_conlog.str());
    confile << spew;
    confile.flush();
    confile.close();
*/
    sentry_flush(1000);
    // sentry_close();
    // abort();
    return event;
}


// #define sentry_id_debug
// #define sentry_id_spewhashes

#include <minidump.h>


void CSentry::SentryInit()
{
    DevMsg(2, "Sentry init!\n");

#ifdef _WIN32
    //EnableCrashingOnCrashes();
#endif

    const char* mpath = ConVarRef("_modpath", false).GetString();
    if (!mpath)
    {
        Error("Couldn't get ConVarRef for _modpath!\n");
    }
    std::string modpath_ss( mpath );
#ifdef _WIN32
    // location of the crashpad handler (in moddir/bin)
    std::stringstream crash_exe;
    crash_exe << modpath_ss << CORRECT_PATH_SEPARATOR << "bin" << CORRECT_PATH_SEPARATOR << "crashpad_handler.exe";
#endif
    // location of the sentry workdir (we're just gonna stick it in mod dir/cache/sentry)
    std::stringstream sentry_db;
    sentry_db << modpath_ss << CORRECT_PATH_SEPARATOR << "cache" << CORRECT_PATH_SEPARATOR << "sentry";

    sentry_options_t* options               = sentry_options_new();
    constexpr char releaseVers[256]         = VPC_QUOTE_STRINGIFY(SENTRY_RELEASE_VERSION);
    sentry_options_set_traces_sample_rate   (options, 1);
    sentry_options_set_on_crash             (options, SENTRY_CRASHFUNC, (void*)NULL);
    sentry_options_set_dsn                  (options, real_sentry_url.c_str());
    sentry_options_set_release              (options, releaseVers);
    sentry_options_set_debug                (options, 1);
    sentry_options_set_max_breadcrumbs      (options, 1024);

    // only windows needs the crashpad exe
#ifdef _WIN32
    sentry_options_set_handler_path         (options, crash_exe.str().c_str() );
#endif

    sentry_options_set_database_path        (options, sentry_db.str().c_str());
    sentry_options_set_shutdown_timeout     (options, 9999);

    sentry_conlog = {};
    sentry_conlog << sentry_db.str() << CORRECT_PATH_SEPARATOR << "last_crash_console_log.txt";
    sentry_options_add_attachment(options, sentry_conlog.str().c_str());
 
/*
    char inventorypath[MAX_PATH] = {};
    V_snprintf(inventorypath, MAX_PATH, "%stf_inventory.txt", last_element);
    sentry_options_add_attachment(options, inventorypath);

    char steaminfpath[MAX_PATH] = {};
    V_snprintf(steaminfpath, MAX_PATH, "%ssteam.inf", last_element);
    sentry_options_add_attachment(options, steaminfpath);

    char configpath[MAX_PATH] = {};
    V_snprintf(configpath, MAX_PATH, "%scfg/config.cfg", last_element);
    sentry_options_add_attachment(options, configpath);

    char badipspath[MAX_PATH] = {};
    V_snprintf(badipspath, MAX_PATH, "%scfg/badips.txt", last_element);
    sentry_options_add_attachment(options, badipspath);
*/

    // Suprisingly, this just works to disable built in Valve crash stuff for linux, unfortunately on windows we need to do hacky stuff like relaunching the game
#ifndef _WIN32
    CommandLine()->AppendParm("-nominidumps",   "");
    CommandLine()->AppendParm("-nobreakpad",    "");
#endif

#if 0
#ifdef _WIN32
    HMODULE mod = GetModuleHandle("crashhandler.dll");
    if (mod)
    {
        bool freed = FreeLibrary(mod))
        if (freed)
        {
            Warning("freed\n");
        }
    }
#endif
#endif

//    sentry_reinstall_backend();
    int sentryinit = sentry_init(options);
    if (sentryinit != 0)
    {
        Warning("Sentry initialization failed!\n");
        CSentry::didinit = false;
        return;
    }
//    sentry_reinstall_backend();

    CSentry::didinit = true;

    // HAS TO RUN AFTER SENTRY INIT!!!
    SetSteamID();

    sentry_add_breadcrumb(sentry_value_new_breadcrumb(NULL, "SentryInit"));
    DevMsg(2, "Sentry initialization success!\n");


    ConVarRef cl_send_error_reports("cl_send_error_reports");
    // already asked
    if (cl_send_error_reports.GetInt() >= 0)
    {
        return;
    }

    // localize these with translations eventually
    const char* windowText = \
        "Do you want to send error reports to the developers?\n"
        "We collect no personally identifiable info, but check out our privacy policy at " VPC_QUOTE_STRINGIFY(SENTRY_PRIVACY_POLICY_URL) "\n"
        "You can change this later by changing the cl_send_error_reports cvar.";

    const char* windowTitle = \
        "Error reporting consent popup";

#ifdef _WIN32
    int msgboxID = MessageBox(
        NULL,
        windowText,
        windowTitle,
        MB_YESNO | MB_SETFOREGROUND | MB_TOPMOST
    );

    switch (msgboxID)
    {
    case IDYES:
        cl_send_error_reports.SetValue(1);
        break;
    case IDNO:
        cl_send_error_reports.SetValue(0);
        break;
    }
#else

    SDL_MessageBoxData messageboxdata   = {};
    messageboxdata.flags                = SDL_MESSAGEBOX_INFORMATION;
    messageboxdata.window               = nullptr;
    messageboxdata.title                = windowTitle;
    messageboxdata.message              = windowText;

    SDL_MessageBoxButtonData buttons[2] = {};
    buttons[0].flags                    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[0].buttonid                 = 0;
    buttons[0].text                     = "Yes";
    buttons[1].flags                    = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[1].buttonid                 = 1;
    buttons[1].text                     = "No";

    messageboxdata.numbuttons           = 2;
    messageboxdata.buttons              = buttons;
    messageboxdata.colorScheme          = nullptr;

    int buttonid                        = {};
    SDL_ShowMessageBox(&messageboxdata, &buttonid);

    // yes
    if (buttonid == 0)
    {
        cl_send_error_reports.SetValue(1);
    }
    // no
    else if (buttonid == 1)
    {
        cl_send_error_reports.SetValue(0);
    }
    // fall thru
    else
    {
        cl_send_error_reports.SetValue(-1);
    }
#endif
//    sentry_reinstall_backend();
}

void SetSteamID()
{
    sentry_value_t user = sentry_value_new_object();

    std::string steamid_string;
    if (!steamapicontext || !steamapicontext->SteamUser())
    {
        steamid_string = "none";
    }
    else
    {
        CSteamID c_steamid = steamapicontext->SteamUser()->GetSteamID();
        if (c_steamid.IsValid())
        {
            // i eated it all </3
            steamid_string = std::to_string(c_steamid.ConvertToUint64());

#if defined (sentry_id_debug)
            Warning("steamid_string -> %s\n", steamid_string.c_str());
#endif
        }
        else
        {
            steamid_string = "invalid";
        }
    }
    sentry_value_set_by_key(user, "id", sentry_value_new_string(steamid_string.c_str()));
    sentry_set_user(user);
}

#include <thread>
#include <util_shared.h>
void SentryMsg(const char* logger, const char* text, bool forcesend /* = false */)
{
    if ( (!forcesend && cl_send_error_reports.GetInt() <= 0) || !g_Sentry.didinit )
    {
        // Warning("NOT SENDING!\n");
        return;
    }

    std::thread sentry_msg_thread(_SentryMsgThreaded, logger, text, forcesend);
    sentry_msg_thread.detach();
}

void _SentryMsgThreaded(const char* logger, const char* text, bool forcesend /* = false */)
{
    SentrySetTags();

    sentry_capture_event
    (
        sentry_value_new_message_event
        (
            SENTRY_LEVEL_INFO,
            logger,
            text
        )
    );

    return;
}

// level should ALWAYS be fatal / error / warning / info / debug - prefer info unless you know better
// logger should always be __FUNCTION__
// message should be a message of "what you are doing" / "what you are logging"
// context info? You need to read the docs: https://docs.sentry.io/platforms/native/usage/#manual-events
void SentryEvent(const char* level, const char* logger, const char* message, sentry_value_t ctxinfo, bool forcesend /* = false */)
{
    if ( (!forcesend && cl_send_error_reports.GetInt() <= 0) || !g_Sentry.didinit )
    {
        // Warning("NOT SENDING!\n");
        return;
    }
    std::thread sentry_event_thread(_SentryEventThreaded, level, logger, message, ctxinfo, forcesend);
    sentry_event_thread.detach();
}

void _SentryEventThreaded(const char* level, const char* logger, const char* message, sentry_value_t ctxinfo, bool forcesend /* = false */)
{
    SentrySetTags();

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "level", sentry_value_new_string(level));
    sentry_value_set_by_key(event, "logger", sentry_value_new_string(logger));
    sentry_value_set_by_key(event, "message", sentry_value_new_string(message));

    sentry_value_t sctx = sentry_value_new_object();
    sentry_value_set_by_key(sctx, "event context", ctxinfo);

    sentry_value_set_by_key(event, "contexts", sctx);
    sentry_capture_event(event);

    sentry_flush(9999);
}

const std::vector<std::string> cvarList =
{
    "mat_dxlevel",
    "host_timescale",
    "sv_cheats",
};


void SentrySetTags()
{
    char mapname[128] = {};
    UTIL_GetMap(mapname);
    if (mapname && mapname[0])
    {
        sentry_set_tag("current map", mapname);
    }
    else
    {
        sentry_set_tag("current map", "none");
    }

    char ipadr[64] = {};
    UTIL_GetRealRemoteAddr(ipadr);
    if (ipadr && ipadr[0])
    {
        sentry_set_tag("server ip", ipadr);
    }
    else
    {
        sentry_set_tag("server ip", "none");
    }

	for (auto& element : cvarList)
	{
        ConVarRef cRef(element.c_str(), true);
        
        if (cRef.IsValid() && cRef.GetName() && cRef.GetString())
        {
            sentry_set_tag(cRef.GetName(), cRef.GetString());
        }
        else
        {
            Warning("Failed getting sentry tag for %s\n", element.c_str());
        }
	}


}

void SentryAddressBreadcrumb(void* address, const char* optionalName)
{
    char addyString[11] = {};
    UTIL_AddrToString(address, addyString);
    sentry_value_t addr_crumb = sentry_value_new_breadcrumb(NULL, NULL);

    sentry_value_t address_object = sentry_value_new_object();
    sentry_value_set_by_key(address_object, optionalName ? optionalName : _OBFUSCATE("variable address"), sentry_value_new_string(addyString));
    sentry_value_set_by_key
    (
        addr_crumb, "data", address_object
    );
    sentry_add_breadcrumb(addr_crumb);
}
#endif // defined(CLIENT_DLL) && defined(SDKCURL) && defined(SDKSENTRY)
