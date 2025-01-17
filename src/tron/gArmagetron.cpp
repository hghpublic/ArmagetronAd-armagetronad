/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

***************************************************************************

*/

#include "gStuff.h"
#include "tSysTime.h"
#include "tDirectories.h"
#include "tLocale.h"
#include "rViewport.h"
#include "rConsole.h"
#include "gGame.h"
#include "gLogo.h"
#include "gCommandLineJumpStart.h"

#include "eSoundMixer.h"

#include "rScreen.h"
#include "rSysdep.h"
#include "uInputQueue.h"
//#include "eTess.h"
#include "rTexture.h"
#include "tConfiguration.h"
#include "tRandom.h"
#include "tRecorder.h"
#include "tCommandLine.h"
#include "tToDo.h"
#include "eAdvWall.h"
#include "eGameObject.h"
#include "uMenu.h"
#include "ePlayer.h"
#include "gLanguageMenu.h"
#include "gAICharacter.h"
#include "gCycle.h"
//#include <unistd>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <bitset>
#include "tCrypto.h"

#include "nServerInfo.h"
#include "nSocket.h"
#include "tRuby.h"
#include "eLadderLog.h"
#ifndef DEDICATED
#include "rRender.h"
#include "rSDL.h"
#include <SDL_syswm.h>

static gCommandLineJumpStartAnalyzer sg_jumpStartAnalyzer;
#endif

#ifndef DEDICATED
#ifdef MACOSX_XCODE
#include "gOSXURLHandler.h"
#endif
#endif

// data structure for command line parsing
class gMainCommandLineAnalyzer: public tCommandLineAnalyzer
{
public:
    bool     fullscreen_;
    bool     windowed_;
    bool     use_directx_;
    bool     dont_use_directx_;

    gMainCommandLineAnalyzer()
    {
        windowed_ = false;
        fullscreen_ = false;
        use_directx_ = false;
        dont_use_directx_ = false;
    }


private:
    bool DoAnalyze( tCommandLineParser & parser, int pass ) override
    {
        if(pass > 0)
            return false;

        if ( parser.GetSwitch( "-fullscreen", "-f" ) )
        {
            fullscreen_=true;
        }
        else if ( parser.GetSwitch( "-window", "-w" ) ||  parser.GetSwitch( "-windowed") )
        {
            windowed_=true;
        }
#ifdef WIN32
        else if ( parser.GetSwitch( "+directx") )
        {
            use_directx_=true;
        }
        else if ( parser.GetSwitch( "-directx") )
        {
            dont_use_directx_=true;
        }
#endif
        else
        {
            return false;
        }

        return true;
    }

    void DoHelp( std::ostream & s ) override
    {                                      //
#ifndef DEDICATED
        s << "-f, --fullscreen             : start in fullscreen mode\n";
        s << "-w, --window, --windowed     : start in windowed mode\n\n";
#ifdef WIN32
        s << "+directx, -directx           : enable/disable usage of DirectX for screen\n"
        << "                               initialisation under MS Windows\n\n";
        s << "\n\nYes, I know this looks ugly. Sorry about that.\n";
#endif
#endif
    }
};

static gMainCommandLineAnalyzer commandLineAnalyzer;

// flag indicating whether directX is supposed to be used for input (defaults to false, crashes on my Win7)
static bool sr_useDirectX = false;
/*
extern bool sr_useDirectX; // rScreen.cpp
#ifdef WIN32
static tConfItem<bool> udx("USE_DIRECTX","makes use of the DirectX input "
                           "fuctions; causes some graphic cards to fail to work (VooDoo 3,...)",
                           sr_useDirectX);
#endif
*/

extern void exit_game_objects(eGrid *grid);

enum gConnection
{
    gLeave,
    gDialup,
    gISDN,
    gDSL
    // gT1
};

// initial setup menu
void sg_StartupPlayerMenu()
{
    uMenu firstSetup("$first_setup", false);
    firstSetup.SetBot(-.2);

    uMenuItemExit e2(&firstSetup, "$menuitem_accept", "$menuitem_accept_help");

    ePlayer * player = ePlayer::PlayerConfig(0);
    tASSERT( player );

    gConnection connection = gDSL;

    uMenuItemSelection<gConnection> net(&firstSetup, "$first_setup_net", "$first_setup_net_help", connection );
    if ( !st_FirstUse )
    {
        net.NewChoice( "$first_setup_leave", "$first_setup_leave_help", gLeave );
        connection = gLeave;
    }
    net.NewChoice( "$first_setup_net_dialup", "$first_setup_net_dialup_help", gDialup );
    net.NewChoice( "$first_setup_net_isdn", "$first_setup_net_isdn_help", gISDN );
    net.NewChoice( "$first_setup_net_dsl", "$first_setup_net_dsl_help", gDSL );

    tString keyboardTemplate("keys_cursor.cfg");

    uMenuItemSelection<tString> k(&firstSetup, "$first_setup_keys", "$first_setup_keys_help", keyboardTemplate );
    if ( !st_FirstUse )
    {
        k.NewChoice( "$first_setup_leave", "$first_setup_leave_help", tString("") );
        keyboardTemplate="";
    }

    k.NewChoice( "$first_setup_keys_cursor", "$first_setup_keys_cursor_help", tString("keys_cursor.cfg") );
    k.NewChoice( "$first_setup_keys_wasd", "$first_setup_keys_wasd_help", tString("keys_wasd.cfg") );
#if SDL_VERSION_ATLEAST(2,0,0)
#else
    k.NewChoice( "$first_setup_keys_zqsd", "$first_setup_keys_zqsd_help", tString("keys_zqsd.cfg") );
#endif
    k.NewChoice( "$first_setup_keys_cursor_single", "$first_setup_keys_cursor_single_help", tString("keys_cursor_single.cfg") );
    k.NewChoice( "$first_setup_keys_x", "$first_setup_keys_x_help", tString("keys_x.cfg") );

#ifdef DEBUG
    k.NewChoice( "none", "none", tString("") );
#endif

    tColor leave(0,0,0,0);
    tColor color(1,0,0);
    uMenuItemSelection<tColor> c(&firstSetup,
                                 "$first_setup_color",
                                 "$first_setup_color_help",
                                 color);

    if ( !st_FirstUse )
    {
        color = leave;
        c.NewChoice( "$first_setup_leave", "$first_setup_leave_help", leave );
    }

    c.NewChoice( "$first_setup_color_red", "", tColor(1,0,0) );
    c.NewChoice( "$first_setup_color_blue", "", tColor(0,0,1) );
    c.NewChoice( "$first_setup_color_green", "", tColor(0,1,0) );
    c.NewChoice( "$first_setup_color_yellow", "", tColor(1,1,0) );
    c.NewChoice( "$first_setup_color_orange", "", tColor(1,.5,0) );
    c.NewChoice( "$first_setup_color_purple", "", tColor(.5,0,1) );
    c.NewChoice( "$first_setup_color_magenta", "", tColor(1,0,1) );
    c.NewChoice( "$first_setup_color_cyan", "", tColor(0,1,1) );
    c.NewChoice( "$first_setup_color_white", "", tColor(1,1,1) );
    c.NewChoice( "$first_setup_color_dark", "", tColor(0,0,0) );

    if ( st_FirstUse )
    {
        for(int i=tRandomizer::GetInstance().Get(4); i>=0; --i)
        {
            c.LeftRight(1);
        }
    }

    uMenuItemString n(&firstSetup,
                      "$player_name_text",
                      "$player_name_help",
                      player->name, ePlayerNetID::MAX_NAME_LENGTH);

    uMenuItemExit e(&firstSetup, "$menuitem_accept", "$menuitem_accept_help");

    firstSetup.Enter();

    // apply network rates
    switch(connection)
    {
    case gDialup:
        sn_maxRateIn  = 6;
        sn_maxRateOut = 4;
        break;
    case gISDN:
        sn_maxRateIn  = 8;
        sn_maxRateOut = 8;
        break;
    case gDSL:
        sn_maxRateIn  = 64;
        sn_maxRateOut = 16;
        break;
    case gLeave:
        break;
    }

    // store color
    if( ! (color == leave) )
    {
        player->rgb[0] = int(color.r_*15);
        player->rgb[1] = int(color.g_*15);
        player->rgb[2] = int(color.b_*15);
    }

    // load keyboard layout
    if( keyboardTemplate.Len() > 1 )
    {
        std::ostringstream fullName;
#if SDL_VERSION_ATLEAST(2,0,0)
        fullName << "sdl2/";
#else
        fullName << "sdl1/";
#endif
        fullName << keyboardTemplate;

        std::ifstream s;
        if( tConfItemBase::OpenFile( s, fullName.str() ) )
        {
            tCurrentAccessLevel level( tAccessLevel_Owner, true );
            tConfItemBase::ReadFile( s );
        }
    }
}

#ifndef DEDICATED
static void welcome(){
    bool textOutBack = sr_textOut;
    sr_textOut = false;

#ifdef DEBUG_XXXX
    {
        for (int i = 20; i>=0; i--)
        {
            sr_ClearGL();
            {
                rTextField c(-.8,.6, .1, .1);
                tString s;
                s << ColorString(1,1,1);
                s << "Test";
                s << ColorString(1,0,0);
                s << "bla bla blubb blaa blaa blubbb blaaa blaaa blubbbb blaaaa blaaaa blubbbbb blaaaaa blaaaaa blubbbbbb blaaaaaa\n";
                c << s;
            }
            sr_SwapGL();
        }
    }
#endif

    REAL timeout = tSysTimeFloat() + .2;
    SDL_Event tEvent;

    if (st_FirstUse)
    {
        sr_LoadDefaultConfig();
        textOutBack = sr_textOut;
        sr_textOut = false;
        gLogo::SetBig(false);
        gLogo::SetSpinning(true);
    }
    else
    {
        bool showSplash = true;
#ifdef DEBUG
        showSplash = false;
#endif

        // Start the music up
        auto& mixer = eSoundMixer::GetMixer();
        mixer.SetMode(TITLE_TRACK);
        mixer.Update();

        // disable splash screen when recording (it's annoying)
        static const char * splashSection = "SPLASH";
        if ( tRecorder::IsRunning() )
        {
            showSplash = false;

            // but keep it for old recordings where the splash screen was always active
            if ( !tRecorder::Playback( splashSection, showSplash ) )
                showSplash = tRecorder::IsPlayingBack();
        }

#ifndef DEDICATED
        if ( sg_jumpStartAnalyzer.ShouldConnect() )
        {
            showSplash = false;
            gLogo::SetDisplayed(false);
            sg_jumpStartAnalyzer.Connect();
        }
#endif

#ifdef MACOSX_XCODE
        sg_StartAAURLHandler( showSplash );
#endif
        tRecorder::Record( splashSection, showSplash );

        if ( showSplash )
        {
            timeout = tSysTimeFloat() + 6;

            uInputProcessGuard inputProcessGuard;
            while((!su_GetSDLInput(tEvent) || tEvent.type!=SDL_KEYDOWN) &&
                    tSysTimeFloat() < timeout)
            {
                if ( sr_glOut )
                {
                    sr_ResetRenderState(true);
                    rViewport::s_viewportFullscreen.Select();

                    rSysDep::ClearGL();

                    uMenu::GenericBackground();

                    rSysDep::SwapGL();
                }

                tAdvanceFrame();
            }
        }

        // catch some keyboard input
        {
            uInputProcessGuard inputProcessGuard;
            while (su_GetSDLInput(tEvent)) ;
        }

        sr_textOut = textOutBack;
        return;
    }

    if ( sr_glOut )
    {
        rSysDep::ClearGL();
        //        rFont::s_defaultFont.Select();
        //        rFont::s_defaultFontSmall.Select();
        gLogo::Display();
        rSysDep::ClearGL();
    }
    rSysDep::SwapGL();

    sr_textOut = textOutBack;
    sg_StartupLanguageMenu();

    sr_textOut = textOutBack;
    sg_StartupPlayerMenu();

    st_FirstUse=false;


    sr_textOut = textOutBack;
}
#endif

void cleanup(eGrid *grid){
    static bool reentry=true;
    if (reentry){
        reentry=false;
        su_contInput=false;

        exit_game_objects(grid);
        /*
          for(int i=MAX_PLAYERS-1;i>=0;i--){
          if (playerConfig[i])
          destroy(playerConfig[i]->cam);
          }


          gNetPlayerWall::Clear();

          eFace::Clear();
          eEdge::Clear();
          ePoint::Clear();

          eFace::Clear();
          eEdge::Clear();
          ePoint::Clear();

          eGameObject::DeleteAll();


        */

#ifdef POWERPAK_DEB
        if (pp_out){
            PD_Quit();
            PP_Quit();
        }
#endif
        nNetObject::ClearAll();

        if (sr_glOut){
            rITexture::UnloadAll();
        }

        sr_glOut=false;
        sr_ExitDisplay();

#ifndef DEDICATED
        sr_RendererCleanup();
#endif

    }
}

#ifndef DEDICATED
static bool sg_active = true;
static void sg_DelayedActivation()
{
    Activate( sg_active );
}

#if SDL_VERSION_ATLEAST(2,0,0)
int filter(void*, SDL_Event *tEvent){
#else
int filter(const SDL_Event *tEvent){
#endif
    // recursion avoidance
    static bool recursion = false;
    if ( !recursion )
    {
        class RecursionGuard
        {
        public:
            RecursionGuard( bool& recursion )
                    :recursion_( recursion )
            {
                recursion = true;
            }

            ~RecursionGuard()
            {
                recursion_ = false;
            }

        private:
            bool& recursion_;
        };

        RecursionGuard guard( recursion );

        // boss key or OS X quit command
        if ((tEvent->type==SDL_KEYDOWN && tEvent->key.keysym.sym==SDLK_ESCAPE &&
                tEvent->key.keysym.mod & KMOD_SHIFT) ||
                (tEvent->type==SDL_KEYDOWN && tEvent->key.keysym.sym==SDLK_q &&
#if SDL_VERSION_ATLEAST(2,0,0)
                 tEvent->key.keysym.mod & KMOD_GUI) ||
#else
                 tEvent->key.keysym.mod & KMOD_META) ||
#endif
                (tEvent->type==SDL_QUIT)){
            // sn_SetNetState(nSTANDALONE);
            // sn_Receive();

            // register end of recording
            tRecorder::Record("END");

            st_SaveConfig();
            uMenu::quickexit=uMenu::QuickExit_Total;
            return false;
        }

        if(tEvent->type==SDL_MOUSEMOTION)
            if(tEvent->motion.x==sr_screenWidth/2 && tEvent->motion.y==sr_screenHeight/2)
                return 0;
        if (su_mouseGrab &&
                tEvent->type!=SDL_MOUSEBUTTONDOWN &&
                tEvent->type!=SDL_MOUSEBUTTONUP &&
                ((tEvent->motion.x>=sr_screenWidth-10  || tEvent->motion.x<=10) ||
                 (tEvent->motion.y>=sr_screenHeight-10 || tEvent->motion.y<=10)))
#if SDL_VERSION_ATLEAST(2,0,0)
            SDL_WarpMouseInWindow(sr_screen, sr_screenWidth/2, sr_screenHeight/2);
#else
            SDL_WarpMouse(sr_screenWidth/2,sr_screenHeight/2);
#endif

        // fetch alt-tab

#if SDL_VERSION_ATLEAST(2,0,0)
        if (tEvent->type==SDL_WINDOWEVENT)
        {
            // Jonathans fullscreen bugfix.
#ifdef MACOSX
            if(currentScreensetting.fullscreen ^ lastSuccess.fullscreen) return false;
#endif
	    if ( tEvent->window.event==SDL_WINDOWEVENT_FOCUS_GAINED || tEvent->window.event==SDL_WINDOWEVENT_FOCUS_LOST )
            {
                sg_active = tEvent->window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
                st_ToDo(sg_DelayedActivation);
            }

            // reload GL stuff if application gets reactivated
            if ( tEvent->window.event == SDL_WINDOWEVENT_FOCUS_GAINED )
#else //SDL_VERSION_ATLEAST(2,0,0)
        if (tEvent->type==SDL_ACTIVEEVENT)
        {
            // Jonathans fullscreen bugfix.
#ifdef MACOSX
            if(currentScreensetting.fullscreen ^ lastSuccess.fullscreen) return false;
#endif
            int flags = SDL_APPINPUTFOCUS;
            if ( tEvent->active.state & flags )
            {
                // con << tSysTimeFloat() << " " << "active: " << (tEvent->active.gain ? "on" : "off") << "\n";
                sg_active = tEvent->active.gain;
                st_ToDo(sg_DelayedActivation);
            }

            // reload GL stuff if application gets reactivated
            if ( tEvent->active.gain && tEvent->active.state & SDL_APPACTIVE )
#endif //SDL_VERSION_ATLEAST(2,0,0)
            {
                // just treat it like a screen mode change, gets the job done
                st_ToDo(rCallbackBeforeScreenModeChange::Exec);
                st_ToDo(rCallbackAfterScreenModeChange::Exec);
            }
            return false;
        }

        if (su_prefetchInput){
            return su_StoreSDLEvent(*tEvent);
        }

    }

    return 1;
}
#endif

//from game.C
void Update_netPlayer();

void sg_SetIcon()
{
#ifndef DEDICATED
#ifndef MACOSX
#ifdef  WIN32_X
    SDL_SysWMinfo	info;
    HICON			icon;
    // get the HWND handle
    SDL_VERSION( &info.version );
    if( SDL_GetWMInfo( &info ) )
    {
        icon = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( 1 ) );
        SetClassLong( info.window, GCL_HICON, (LONG) icon );
    }
#else
    rSurface tex( "textures/icon.png" );

    if (tex.GetSurface())
#if SDL_VERSION_ATLEAST(2,0,0)
        SDL_SetWindowIcon(sr_screen, tex.GetSurface());
#else
        SDL_WM_SetIcon(tex.GetSurface(),NULL);
#endif
#endif
#endif
#endif
}

class gAutoStringArray
{
public:
    ~gAutoStringArray()
    {
#ifdef HAVE_CLEARENV
        // Optional. Systems that don't have this function better make copies of putenv() arguments.
        clearenv();
#endif

        for ( std::vector< char * >::iterator i = strings.begin(); i != strings.end(); ++i )
        {
            free( *i );
        }
    }

    char * Store( char const * s )
    {
        char * ret = strdup( s );
        strings.push_back( ret );
        return ret;
    }
private:
    std::vector< char * > strings; // the stored raw C strings
};

// wrapper for putenv, taking care of the peculiarity that the argument
// is kept in use for the rest of the program's lifetime
void sg_PutEnv( char const * s )
{
    static gAutoStringArray store;
    putenv( store.Store( s ) );
}

namespace
{
tString sn_configurationSavedInVersion{"0.2.8"};
tConfItem<tString> sn_configurationSavedInVersionConf("SAVED_IN_VERSION",sn_configurationSavedInVersion);
}

int main(int argc,char **argv){
    //std::cout << "enter\n";
    //  net_test();

    bool dedicatedServer = false;

    //  std::cout << "Running " << argv[0] << "...\n";

    // tERR_MESSAGE( "Start!" );

    try
    {
        // Create this command line analyzer here instead of statically
        // so it will be the first to display in --help.
        tDefaultCommandLineAnalyzer defaultCommandLineAnalyzer;
        tCommandLineData commandLine( st_programVersion );

        // analyse command line
        // tERR_MESSAGE( "Analyzing command line." );
        if ( !commandLine.Analyse(argc, argv) )
            return 0;


        {
            // embed version in recording
            const char * versionSection = "VERSION";
            tString version( st_programVersion );
            tRecorder::Playback( versionSection, version );
            tRecorder::Record( versionSection, version );
#ifndef DEDICATED
            if(version != st_programVersion)
            {
#ifdef DEBUG
                tERR_WARN( "Recording from a different version, consider at high risk of desync." );
#endif
                tRecorder::ActivateProbablyDesyncedPlayback();
            }
#endif
        }

        {
            // read/write server/client mode from/to recording
            const char * dedicatedSection = "DEDICATED";
            if ( !tRecorder::PlaybackStrict( dedicatedSection, dedicatedServer ) )
            {
#ifdef DEDICATED
                dedicatedServer = true;
#endif
            }
            tRecorder::Record( dedicatedSection, dedicatedServer );
        }


        // while DGA mouse is buggy in XFree 4.0:
#ifdef linux
        // Sam 5/23 - Don't ever use DGA, we don't need it for this game.
        // no longer needed, the bug this compensated was fixed a long time
        // ago.
        /*
        if ( ! getenv("SDL_VIDEO_X11_DGAMOUSE") ) {
            sg_PutEnv("SDL_VIDEO_X11_DGAMOUSE=0");
        }
        */
#endif

#ifdef WIN32
#if !SDL_VERSION_ATLEAST(2,0,0)
        // disable DirectX by default; it causes problems with some boards.
        if (!getenv( "SDL_VIDEODRIVER") ) {
            if (!sr_useDirectX)
            {
                sg_PutEnv( "SDL_VIDEODRIVER=windib" );
            }
            else
            {
                sg_PutEnv( "SDL_VIDEODRIVER=directx" );
            }
        }
#endif
#endif

        // atexit(ANET_Shutdown);

#ifndef WIN32
#ifdef DEBUG
#define NOSOUND
#endif
#endif

#ifndef DEDICATED
        Uint32 flags = SDL_INIT_VIDEO;
#ifdef DEBUG
        flags |= SDL_INIT_NOPARACHUTE;
#endif // DEBUG
        if (SDL_Init(flags) < 0) {
            tERR_ERROR("Couldn't initialize SDL: " << SDL_GetError());
        }
        atexit(SDL_Quit);
        // su_KeyInit();

        su_KeyInit();

#ifndef NOJOYSTICK
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK))
            std::cout << "Error initializing joystick subsystem\n";
        else
        {
#ifdef DEBUG
            // std::cout << "Joystick(s) initialized\n";
#endif // DEBUG
            su_JoystickInit();
        }
#endif // NOJOYSTICK
#endif // DEDICATED

        // tERR_MESSAGE( "Initializing player data." );
        ePlayer::Init();

#ifdef HAVE_LIBRUBY
        tRuby::InitializeInterpreter();
        try {
            tRuby::Load(tDirectories::Data(), "scripts/initialize.rb");
        }
        catch (std::runtime_error & e) {
            std::cerr << e.what() << '\n';
        }
#endif

        // tERR_MESSAGE( "Loading configuration." );
        tLocale::Load("languages.txt");

        eLadderLogInitializer ladderlog;
        st_LoadConfig();

        // migrate user configuration from previous versions
        if(sn_configurationSavedInVersion != st_programVersion)
        {
            if(st_FirstUse)
            {
                sn_configurationSavedInVersion = "0.0";
            }

            tConfigMigration::Migrate(sn_configurationSavedInVersion);
        }
        if(tConfigMigration::SavedBefore(sn_configurationSavedInVersion, st_programVersion))
            sn_configurationSavedInVersion = st_programVersion;

        // record and play back the recording debug level
        tRecorderSyncBase::GetDebugLevelPlayback();

        if ( commandLineAnalyzer.fullscreen_ )
            currentScreensetting.fullscreen   = true;
        if ( commandLineAnalyzer.windowed_ )
            currentScreensetting.fullscreen   = false;
        if ( commandLineAnalyzer.use_directx_ )
            sr_useDirectX                       = true;
        if ( commandLineAnalyzer.dont_use_directx_ )
            sr_useDirectX                       = false;

        //gAICharacter::LoadAll(tString( "aiplayers.cfg" ) );
        gAICharacter::LoadAll( aiPlayersConfig );

        sg_LanguageInit();
        atexit(tLocale::Clear);

        if ( commandLine.Execute() )
        {
            gCycle::PrivateSettings();

            {
                std::ifstream t;

                if ( !tDirectories::Config().Open( t, "settings.cfg" ) )
                {
                    //		#ifdef WIN32
                    //                    tERR_ERROR( "Data files not found. You have to run Armagetron from its own directory." );
                    //		#else
                    tERR_ERROR( "Configuration files not found. Check your installation." );
                    //		#endif
                }
            }

            {
                std::ofstream s;
                if (! tDirectories::Var().Open( s, "scorelog.txt", std::ios::app ) )
                {
                    char const * error = "var directory not writable or does not exist. It should reside inside your user data directory and should have been created automatically on first start, but something must have gone wrong."
                    #ifdef WIN32
                                         " You can access your user data directory over one of the start menu entries we installed."
                    #else
                                         " Your user data directory is subdirectory named .armagetronad in your home directory."
                    #endif
                                         ;

                    tERR_ERROR( error );
                }
            }

            {
                std::ifstream t;

                if ( tDirectories::Data().Open( t, "moviepack/settings.cfg" ) )
                {
                    sg_moviepackInstalled=true;
                }
            }

#ifndef DEDICATED
            sr_glOut=1;
            //std::cout << "checked mp\n";

            sr_glRendererInit();

#if SDL_VERSION_ATLEAST(2,0,0)
            SDL_SetEventFilter(&filter, 0);
#else
            SDL_SetEventFilter(&filter);
#endif
            //std::cout << "set filter\n";

            tConsole::RegisterMessageCallback(&uMenu::Message);
            tConsole::RegisterIdleCallback(&uMenu::IdleInput);

            if (sr_InitDisplay()){

                sg_SetIcon();

                try
                {
#ifdef HAVE_GLEW
                    // initialize GLEW
                    {
                        GLenum err = glewInit();
                        if (GLEW_OK != err)
                        {
                            // Problem: glewInit failed, something is seriously wrong
                            throw tGenericException( (const char *)glewGetErrorString(err), "GLEW Error" );
                        }
                        con << "Status: Using GLEW " << glewGetString(GLEW_VERSION) << "\n";
                    }
#endif // HAVE_GLEW

                    //std::cout << "init disp\n";

                    //std::cout << "init sound\n";

                    welcome();

                    //std::cout << "atexit\n";

                    sr_con.autoDisplayAtSwap=false;

                    //std::cout << "sound started\n";

                    gLogo::SetBig(false);
                    gLogo::SetSpinning(true);

                    sn_bigBrotherString = renderer_identification + "VER=" + st_programVersion + "\n\n";

#ifdef HAVE_LIBRUBY
                    try {
                        // tRuby::Load(tDirectories::Data(), "scripts/menu.rb");
                        tRuby::Load(tDirectories::Data(), "scripts/ai.rb");
                    }
                    catch (std::runtime_error & e) {
                        std::cerr << e.what() << '\n';
                    }
#endif

                    MainMenu();

                    // remove all players
                    for ( int i = se_PlayerNetIDs.Len()-1; i>=0; --i )
                        se_PlayerNetIDs(i)->RemoveFromGame();

                    nNetObject::ClearAll();

                    rITexture::UnloadAll();
                }
                catch (tException const & e)
                {
                    gLogo::SetDisplayed(true);
                    uMenu::SetIdle(NULL);
                    sr_con.autoDisplayAtSwap=false;

                    // inform user of generic errors
                    tConsole::Message( e.GetName(), e.GetDescription(), 20 );
                }

                sr_ExitDisplay();
                sr_RendererCleanup();

                //std::cout << "exit\n";

                st_SaveConfig();

                //std::cout << "saved\n";

                //    cleanup(grid);
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            }

            eSoundMixer::ShutDown();

            SDL_Quit();
#else // DEDICATED
            sr_glOut=0;

            //  nServerInfo::TellMasterAboutMe();

            while (!uMenu::quickexit)
                sg_HostGame();
#endif // DEDICATED
            nNetObject::ClearAll();
            nServerInfo::DeleteAll();
        }

        ePlayer::Exit();

#ifdef HAVE_LIBRUBY
        tRuby::CleanupInterpreter();
#endif

        //	tLocale::Clear();
    }
    catch ( tCleanQuit const & e )
    {
        return 0;
    }
    catch ( tException const & e )
    {
        try
        {
            st_PresentError( e.GetName(), e.GetDescription() );
        }
        catch(...)
        {
        }

        return 1;
    }
    catch ( std::exception & e )
    {
        try
        {
            st_PresentError("", e.what());
        }
        catch(...)
        {
        }
    }
#ifdef _MSC_VER
#pragma warning ( disable : 4286 )
    // GRR. Visual C++ dones not handle generic exceptions with the above general statement.
    // A specialized version is needed. The best part: it warns about the code below being redundant.
    catch( tGenericException const & e )
    {
        try
        {
            st_PresentError( e.GetName(), e.GetDescription() );
        }
        catch(...)
        {
        }

        return 1;
    }
#endif
    catch(...)
    {
        return 1;
    }

    return 0;
}

#ifdef DEDICATED
// settings missing in the dedicated server
static void st_Dummy(std::istream &s){tString rest; rest.ReadLine(s);}
static tConfItemFunc st_Dummy10("MASTER_QUERY_INTERVAL", &st_Dummy);
static tConfItemFunc st_Dummy11("MASTER_SAVE_INTERVAL", &st_Dummy);
static tConfItemFunc st_Dummy12("MASTER_IDLE", &st_Dummy);
static tConfItemFunc st_Dummy13("MASTER_PORT", &st_Dummy);
#endif



