/**
 * @file skse.cpp
 * @brief Implements SKSE plugin for SSE Journal
 * @internal
 *
 * This file is part of Skyrim SE Journal mod (aka Journal).
 *
 *   Journal is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Journal is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with Journal. If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Public API
 *
 * @details
 */

#include <sse-imgui/sse-imgui.h>
#include <sse-gui/sse-gui.h>
#include <utils/winutils.hpp>

#include <fstream>
#include <iomanip>
#include <chrono>
#include <array>
#include <cstdint>
typedef std::uint32_t UInt32;
typedef std::uint64_t UInt64;
#include <skse/PluginAPI.h>

//--------------------------------------------------------------------------------------------------

/// Given by SKSE to uniquely identify this DLL
static PluginHandle plugin = 0;

/// To communicate with the other SKSE plugins.
static SKSEMessagingInterface* messages = nullptr;

/// Log file in pre-defined location
static std::ofstream logfile;

/// Local initialization
static std::unique_ptr<sseimgui_api> sseimgui;

/// [shared] In order to hook upon D3D11
std::unique_ptr<ssegui_api> ssegui;

/// [shared] Table with pointers
imgui_api imgui;

//--------------------------------------------------------------------------------------------------

static void
open_log ()
{
    std::string path;
    if (known_folder_path (FOLDERID_Documents, path))
    {
        // Before plugins are loaded, SKSE takes care to create the directiories
        path += "\\My Games\\Skyrim Special Edition\\SKSE\\";
    }
    path += "sse-journal.log";
    logfile.open (path);
}

//--------------------------------------------------------------------------------------------------

decltype(logfile)&
log ()
{
    // MinGW 4.9.1 have no std::put_time()
    using std::chrono::system_clock;
    auto now_c = system_clock::to_time_t (system_clock::now ());
    auto loc_c = std::localtime (&now_c);
    logfile << '['
            << 1900 + loc_c->tm_year
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mon
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mday
            << ' ' << std::setw (2) << std::setfill ('0') << loc_c->tm_hour
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_min
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_sec
        << "] ";
    return logfile;
}

//--------------------------------------------------------------------------------------------------

void
journal_version (int* maj, int* min, int* patch, const char** timestamp)
{
    constexpr std::array<int, 3> ver = {
#include "../VERSION"
    };
    if (maj) *maj = ver[0];
    if (min) *min = ver[1];
    if (patch) *patch = ver[2];
    if (timestamp) *timestamp = JOURNAL_TIMESTAMP; //"2019-04-15T08:37:11.419416+00:00"
}

//--------------------------------------------------------------------------------------------------

static void
handle_sseimgui_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SSEIMGUI_API_VERSION)
    {
        log () << "Unsupported SSEIMGUI interface v" << m->type
               << " (it is not v" << SSEIMGUI_API_VERSION
                << "). Bailing out." << std::endl;
        return;
    }

    sseimgui.reset (new sseimgui_api (*reinterpret_cast<sseimgui_api*> (m->data)));
    imgui = sseimgui->make_imgui_api ();
    log () << "Accepted SSEIMGUI interface v" << SSEIMGUI_API_VERSION << std::endl;
}

//--------------------------------------------------------------------------------------------------

/// This is somewhere in SKSE Input Loaded message, before sse-imgui

static void
handle_ssegui_message (SKSEMessagingInterface::Message* m)
{
    if (!sseimgui)
        return;

    if (m->type != SSEGUI_API_VERSION)
    {
        log () << "Unsupported SSEGUI interface v" << m->type
               << " (it is not v" << SSEGUI_API_VERSION
               << "). Bailing out." << std::endl;
        return;
    }
    ssegui.reset (new ssegui_api (*reinterpret_cast<ssegui_api*> (m->data)));
    log () << "Accepted SSEGUI interface v" << SSEGUI_API_VERSION << std::endl;

    extern bool setup ();
    if (!setup ())
    {
        log () << "Unable to initialize SSE Journal" << std::endl;
        return;
    }

    extern void render (int); sseimgui->render_listener (&render, 0);
    log () << "All done." << std::endl;
}

//--------------------------------------------------------------------------------------------------

/// Post Load ensure SSEIMGUI is loaded and can accept listeners

static void
handle_skse_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SKSEMessagingInterface::kMessage_PostLoad)
        return;
    log () << "SKSE Post Load." << std::endl;
    messages->RegisterListener (plugin, "SSEGUI", handle_ssegui_message);
    messages->RegisterListener (plugin, "SSEIMGUI", handle_sseimgui_message);
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" __declspec(dllexport) bool SSEIMGUI_CCONV
SKSEPlugin_Query (SKSEInterface const* skse, PluginInfo* info)
{
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "sse-journal";
    journal_version ((int*) &info->version, nullptr, nullptr, nullptr);

    plugin = skse->GetPluginHandle ();

    if (skse->isEditor)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" __declspec(dllexport) bool SSEIMGUI_CCONV
SKSEPlugin_Load (SKSEInterface const* skse)
{
    open_log ();

    messages = (SKSEMessagingInterface*) skse->QueryInterface (kInterface_Messaging);
    messages->RegisterListener (plugin, "SKSE", handle_skse_message);

    int a, m, p;
    const char* b;
    journal_version (&a, &m, &p, &b);
    log () << "SSE-Journal "<< a <<'.'<< m <<'.'<< p <<" ("<< b <<')' << std::endl;
    return true;
}

//--------------------------------------------------------------------------------------------------


