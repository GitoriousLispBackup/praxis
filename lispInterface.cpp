
#include "lispInterface.h"

#include <iostream>

#include <time.h>

#ifdef __PRAXIS_LINUX__
#include <unistd.h>
#endif

#ifdef __PRAXIS_WINDOWS__
#include <windows.h>

extern "C"
{
    extern HWND g_AppHWND;
}
#endif

s7_scheme * g_pLisp = 0;
std::string g_sLispOutput;
std::string g_sLispError;

int g_nLispTraceVerbosity = 0;

extern "C"
{
    extern int g_nLastBreakTime;
}

static s7_pointer our_sleep(s7_scheme *sc, s7_pointer args)
{
#ifdef __PRAXIS_LINUX__
    usleep(1000000);
#endif
#ifdef __PRAXIS_WINDOWS__
    Sleep(1000);
#endif
    return(s7_f(sc));
}

void lispInit()
{
    g_pLisp = s7_init();

    s7_define_function(g_pLisp, "sleep", our_sleep, 0, 0, false, "(sleep) sleeps");
}

//int g_nLastBreakTime = 0;

void lispEndHook(s7_scheme *sc, bool *all_done)
{
    if(g_nLispTraceVerbosity > 0)
    {
        std::string sCode = s7_object_to_c_string(sc, s7_get_main_stack_code(sc));
        s7_pointer curenv = s7_current_environment(sc);
        s7_pointer vars = s7_environment_to_list(sc, curenv);
        std::string sEnvironment = s7_object_to_c_string(sc, vars);
        int nStackDepth = s7_get_stack_depth(sc) - 2; // or 3

        char * sCaller1 = s7_get_main_stack_caller(sc);
        s7_pointer result = s7_get_value(sc);

        std::string sCaller = "";
        if(sCaller1 != 0)
            sCaller = std::string(sCaller1); // s7_object_to_c_string(sc, caller);

        std::string sResult = "";
        if(result != 0)
            sResult = s7_object_to_c_string(sc, result);

        for(int i = 0; i < nStackDepth; i++)
            std::cout << "-- ";

        std::cout << sResult << std::endl;
    }
}

void lispBeginHook(s7_scheme *sc, bool *all_done)
{
    if(g_nLispTraceVerbosity > 0)
    {
        std::string sCode = s7_object_to_c_string(sc, s7_get_main_stack_code(sc));
        s7_pointer curenv = s7_current_environment(sc);
        s7_pointer vars = s7_environment_to_list(sc, curenv);
        std::string sEnvironment = s7_object_to_c_string(sc, vars);
        int nStackDepth = s7_get_stack_depth(sc) - 2; // or 3

        char * sCaller1 = s7_get_main_stack_caller(sc);
        s7_pointer result = s7_get_value(sc);

        std::string sCaller = "";
        if(sCaller1 != 0)
            sCaller = std::string(sCaller1); // s7_object_to_c_string(sc, caller);

        std::string sResult = "";
        if(result != 0)
            sResult = s7_object_to_c_string(sc, result);

        for(int i = 0; i < nStackDepth; i++)
            std::cout << "-- ";

        std::cout << "[" << sCaller << "] " << sEnvironment << " : " << sCode << std::endl;
    }

#ifdef __PRAXIS_LINUX__
    extern bool g_bInterruptLisp;
    if(g_bInterruptLisp)
    {
        // Maybe do the time thing, and disregard if time since last is too low.
        // Time is something I need to figure out for Linux.
        // http://www.cplusplus.com/forum/unices/45299/
        // http://www.cplusplus.com/reference/ctime/
        // http://www.omnicode.com/qa.php?id=91
        // http://stackoverflow.com/questions/14783468/is-there-a-standard-library-equivalent-of-timegettime
        // http://en.cppreference.com/w/cpp/chrono (C++11)
        // http://www.gamedev.net/topic/315035-linux-timegettime-counterpart/
        // more boring:
        // http://unix.derkeiler.com/Newsgroups/comp.unix.programmer/2006-10/msg00535.html


        *all_done = true;
        g_bInterruptLisp = false;
    }
#endif

    // Need to do this some other way in Linux
    // async key press querying in SDL?
#ifdef __PRAXIS_WINDOWS__
    if(::GetAsyncKeyState(VK_LCONTROL) != 0 && ::GetAsyncKeyState(0x51) != 0) // Ctrl-Q pressed
    {
        // Break detected. Check the time since last break.

        // If its too low, ignore.
        int nTime = ::timeGetTime();
        if(nTime < (g_nLastBreakTime + 200))
            return;

        // If its high enough, then break.
        g_nLastBreakTime = nTime;

        *all_done = true;
    }
#endif
}

bool lispCall(std::string sCmd)
{
    g_sLispOutput = "";
    g_sLispError = "";

    s7_pointer old_port;
    int gc_loc = -1;
    const char *errmsg = NULL;

    /* trap error messages */
    old_port = s7_set_current_error_port(g_pLisp, s7_open_output_string(g_pLisp));
    if (old_port != s7_nil(g_pLisp))
        gc_loc = s7_gc_protect(g_pLisp, old_port);

    s7_set_begin_hook(g_pLisp, lispBeginHook);
    s7_set_end_hook(g_pLisp, lispEndHook);
    s7_pointer val = s7_eval_c_string(g_pLisp, sCmd.c_str());
    s7_set_begin_hook(g_pLisp, NULL);
    s7_set_end_hook(g_pLisp, NULL);

    g_sLispOutput = s7_object_to_c_string(g_pLisp, val);

    /* look for error messages */
    errmsg = s7_get_output_string(g_pLisp, s7_current_error_port(g_pLisp));

    if(errmsg)
        g_sLispError = errmsg;

    s7_close_output_port(g_pLisp, s7_current_error_port(g_pLisp));
    s7_set_current_error_port(g_pLisp, old_port);
    if (gc_loc != -1)
        s7_gc_unprotect_at(g_pLisp, gc_loc);

    return true;
}

std::string & lispGetError()
{
    return g_sLispError;
}

std::string & lispGetOutput()
{
    return g_sLispOutput;
}

void lispClearError()
{
    g_sLispError = "";
}

void lispClearOutput()
{
    g_sLispOutput = "";
}

void lispClose()
{
    // nuthin?
}
