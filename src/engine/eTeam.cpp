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

#include "eTeam.h"
#include "tSysTime.h"
#include "rFont.h"
#include "nConfig.h"
#include "eWarmup.h"
#include "eLadderLog.h"

#include <set>
#include <climits>

#include "nProtoBuf.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "eTeam.pb.h"
#pragma GCC diagnostic pop

#include "aa_config.h"

tString & operator << ( tString &s, const eTeam * team)
{
    if ( !team )
        return s << tOutput("$player_spectator_message");
    else
        return s << team->GetColoredName();
}
std::ostream & operator << ( std::ostream &s, const eTeam * team)
{
    if ( !team )
        return s << tOutput("$player_spectator_message");
    else
        return s << team->GetColoredName();
}

#define TEAMCOLORS 8

static unsigned short se_team_rgb[TEAMCOLORS][3]=
{ {  4,  8, 15 } , // blue
    { 15, 13,  0 } , // gold
    { 15,  1,  1 } , // red
    {  0, 15,  4 } , // green
    { 15,  4, 15 } , // violet
    {  4, 15, 15 } , // ugly green
    { 15, 15, 15 } , // white
    {  7,  7,  7 }   // black
};

static tString se_team_name[TEAMCOLORS]=
{
    tString("$team_name_blue"),
    tString("$team_name_gold"),
    tString("$team_name_red"),
    tString("$team_name_green"),
    tString("$team_name_violet"),
    tString("$team_name_ugly"),
    tString("$team_name_white"),
    tString("$team_name_black")
};

// static tList<eTeam> se_ColoredTeams;
static eTeam * se_ColoredTeams[TEAMCOLORS]={0,0,0,0,0,0,0,0};

static int IMPOSSIBLY_LOW_SCORE=INT_MIN;

// class that creates config items for one team
// TEAM_(NAME|RED|GREEN|BLUE)_X
class eTeamColorConfig {
    typedef tSettingItem<tString> nameConf;
    typedef tSettingItem<unsigned short int> colorConf;
    colorConf *m_red, *m_green, *m_blue;
    nameConf *m_name;
    static int teamCount;
public:
    eTeamColorConfig() {
        std::ostringstream name(""), red(""), green(""), blue("");
        name  << "TEAM_NAME_"  << teamCount + 1;
        red   << "TEAM_RED_"   << teamCount + 1;
        green << "TEAM_GREEN_" << teamCount + 1;
        blue  << "TEAM_BLUE_"  << teamCount + 1;
        m_name  = new nameConf (name .str().c_str(), se_team_name[teamCount]);
        m_red   = new colorConf(red  .str().c_str(), se_team_rgb [teamCount][0]);
        m_green = new colorConf(green.str().c_str(), se_team_rgb [teamCount][1]);
        m_blue  = new colorConf(blue .str().c_str(), se_team_rgb [teamCount][2]);
        ++teamCount;
    }
    ~eTeamColorConfig() {
        delete m_name;
        delete m_red;
        delete m_green;
        delete m_blue;
    }
};

int eTeamColorConfig::teamCount = 0;

static eTeamColorConfig se_team_config[TEAMCOLORS];

//! Creates a color string inserter
inline static tColoredStringProxy ColorString( const eTeam * t )
{
    return tColoredStringProxy( t->R()/15.0f, t->G()/15.0f, t->B()/15.0f );
}

nNetObjectDescriptor<eTeam, Engine::TeamSync > eTeam_init(220);

//! returns the descriptor responsible for this class
nNetObjectDescriptorBase const & eTeam::DoGetDescriptor() const
{
    return eTeam_init;
}

int  eTeam::minTeams=0;					// minimum nuber of teams
int  eTeam::maxTeams=30;    			// maximum nuber of teams
int  eTeam::minPlayers=0;   			// minimum number of players per team
int  eTeam::maxPlayers=3;   			// maximum number of players per team
int  eTeam::maxImbalance=2;			// maximum difference of player numbers
bool eTeam::balanceWithAIs=true;		// use AI players to balance the teams?
bool eTeam::enforceRulesOnQuit=false;	// if the quitting of one player unbalances the teams, enforce the rules by redistributing

tList<eTeam> eTeam::teams;		//  list of all teams

static bool newTeamAllowed;		// is it allowed to create a new team currently?

static nSettingItem<bool> se_newTeamAllowed("NEW_TEAM_ALLOWED", newTeamAllowed );

static bool se_allowTeamNameColor  = true; // allow to name a team after a color
static bool se_allowTeamNamePlayer = true; // allow to name a team after the leader
static bool se_allowTeamNameLeader = false; // allow to name a team by it's leader

static tSettingItem<bool> se_allowTeamNameColorConfig("ALLOW_TEAM_NAME_COLOR", se_allowTeamNameColor );
static tSettingItem<bool> se_allowTeamNamePlayerConfig("ALLOW_TEAM_NAME_PLAYER", se_allowTeamNamePlayer );
static tSettingItem<bool> se_allowTeamNameCustomConfig("ALLOW_TEAM_NAME_LEADER", se_allowTeamNameLeader );

static REAL se_minReady = 0.51;
static tSettingItem<REAL> se_minReadyConf("WARMUP_MIN_READY_FRAC", se_minReady);

// update all internal information
void eTeam::UpdateStaticFlags()
{
    bool newTeamAllowedCurrent = teams.Len() >= maxTeams;

    if ( newTeamAllowedCurrent != newTeamAllowed )
    {
        se_newTeamAllowed.Set( newTeamAllowedCurrent );

        for (int i = teams.Len() - 1; i>=0; --i)
            teams(i)->Update();
    }
}

// number of rounds played (updated right after spawning)
int eTeam::RoundsPlayed() const
{
    return roundsPlayed;
}

// increase round counter
void eTeam::PlayRound()
{
    roundsPlayed++;
}

//update internal properties ( player count )
void eTeam::UpdateProperties()
{
    //	bool change = false;
    if ( nCLIENT != sn_GetNetState() )
    {
        //if ( maxPlayersLocal != maxPlayers )
        //{
        maxPlayersLocal = maxPlayers;
        //			change = true;
        //}

        //if ( maxImbalanceLocal != maxImbalance )
        //{
        maxImbalanceLocal = maxImbalance;
        //			change = true;
        //}

        //		if ( change )
        //		{
        //		}
    }

    numHumans = 0;
    numAIs = 0;
    int i;
    for ( i = players.Len()-1; i>=0; --i )
    {
        ePlayerNetID * player = players(i);
        if ( player->IsHuman() )
        {
            if ( player->IsActive() )
                ++numHumans;
        }
        else
            ++numAIs;
    }

    if ( nSERVER == sn_GetNetState() )
        RequestSync();
}

static eLadderLogWriter se_teamRenamedWriter( "TEAM_RENAMED", true, "old_name new_name" );
static eLadderLogWriter se_teamCreateWriter( "TEAM_CREATED", true, "team" );
static eLadderLogWriter se_teamDestroyWriter( "TEAM_DESTROYED", true, "team" );
static eLadderLogWriter se_teamAddWriter( "TEAM_PLAYER_ADDED", true, "team player" );
static eLadderLogWriter se_teamRemoveWriter( "TEAM_PLAYER_REMOVED", true, "team player" );

// update name and color
void eTeam::UpdateAppearance()
{
    tShortColor oldColor( color );
    bool empty = false;
    tString oldName = name;

    ePlayerNetID* oldest = OldestHumanPlayer();
    if ( !oldest )
    {
        oldest = OldestAIPlayer();
    }
    /* Logic:
    	  No more voting about teamnames.
    	  The teamname of the player who has been on the team for the longest time
    	  is used for a team!

          Color Teamname:
                -more than 1 player in the current team
    			-oldest player's custom teamname is empty
    			-if "Player Settings->Player 1->Name Team After Player" is off
          Custom Teamname:
    	        -oldest player's custom teamname is NOT empty
          Non-Color Teamname (e.g. "Player 1's Team"):
                -more than 1 player in the current team
    			-oldest/first player's custom teamname is empty
    			-if "Player Settings->Player 1->Name Team After Player" is on
    */
    bool nameTeamColor = players.Len() > 1 && (!oldest || oldest->teamname.Len()<=1 || !oldest->nameTeamAfterMe);

    //	if ( !IsHuman() )
    //		nameTeamColor = false;
    if ( !se_allowTeamNameColor )
        nameTeamColor = false;
    if ( !se_allowTeamNamePlayer )
        nameTeamColor = true;

    nameTeamColor = NameTeamAfterColor ( nameTeamColor );
    bool isCustomTeamName = false;

    tString updateName;
    if ( oldest )
    {
        if ( nameTeamColor )
        {
            // team name determined by color
            tOutput newname;
            newname << &se_team_name[ colorID ][0];

            updateName = newname;

            color.r_ = se_team_rgb[colorID][0];
            color.g_ = se_team_rgb[colorID][1];
            color.b_ = se_team_rgb[colorID][2];
        }
        else
        {
            // let oldest own the team
            if ( players.Len() > 1 )
            {
                // did the player set a custom teamname ?
                if (se_allowTeamNameLeader && oldest->teamname.Len()>1)
                {
                    // Use player's custom teamname
                    isCustomTeamName = true;
                    updateName = oldest->teamname;
                }
                else if ( oldest->IsHuman() )
                {
                    // name team after first/oldest player
                    tOutput newname;
                    newname.SetTemplateParameter( 1, oldest->GetName() );
                    newname << "$team_owned_by";
                    updateName = newname;
                }
                else
                {
                    // name team after oldest bot
                    tOutput newname;
                    newname.SetTemplateParameter( 1, oldest->GetName() );
                    newname << "$team_ai";
                    updateName = newname;
                }
            }
            else
            {
                // did the player set a custom teamname ?
                if (oldest->teamname.Len()>1)
                {
                    // use custom teamname
                    isCustomTeamName = true;
                    updateName = oldest->teamname;
                }
                else
                {
                    // use player name as teamname
                    updateName = oldest->GetName();
                }
            }

            color = oldest->color;

            // update colored player names
            if ( sn_GetNetState() != nSERVER )
            {
                for ( int i = players.Len()-1; i>=0; --i )
                {
                    players(i)->UpdateName();
                }
            }
        }
    }
    else
    {
        // empty team
        updateName = tOutput("$team_empty");
        color.r_ = color.g_ = color.b_ = 7;
        empty = true;
    }

    tString newNameFiltered = ePlayerNetID::FilterName( updateName );
    if( logName != newNameFiltered )
    {
        if( !empty && !lastEmpty_ )
        {
            se_teamRenamedWriter << logName << newNameFiltered;
            se_teamRenamedWriter.write();
        }
        else if( !empty )
        {
            se_teamCreateWriter << newNameFiltered;
            se_teamCreateWriter.write();
        }
        else if( !lastEmpty_ )
        {
            LogScoreDifference();
            se_teamDestroyWriter << logName;
            se_teamDestroyWriter.write();
        }
        logName = newNameFiltered;
    }
    lastEmpty_ = empty;

    // if the name has been changed then update it
    if (name!=updateName)
    {
        // Notify other players of the team name change if it's interesting
        if (
            sn_GetNetState() != nCLIENT
            && oldest
            && name != ""
            && name != tString( tOutput("$team_empty") )
            && ( lastWasCustomTeamName_ || ( updateName != oldest->GetName() && !nameTeamColor ) )
        )
        {
            tOutput message;
            tColoredString name;
            name << *oldest;
            name << tColoredString::ColorString(-1,-1,-1);
            message.SetTemplateParameter(1, name);

            tColoredString resetColor;
            resetColor << tColoredString::ColorString(color.r_,color.g_,color.b_);
            resetColor << updateName;
            resetColor << tColoredString::ColorString(-1,-1,-1);
            message.SetTemplateParameter(2, resetColor);
            message << "$team_renamed";
            sn_ConsoleOut(message);
        }
        name = updateName;
        lastWasCustomTeamName_ = isCustomTeamName;
    }

    if ( nSERVER == sn_GetNetState() )
        RequestSync();

    // update team members if the color changed
    if ( oldColor != color )
    {
        for ( int i = players.Len() - 1; i >= 0; --i )
        {
            players(i)->UpdateName();
        }
    }
}

//update internal properties ( player count )
void eTeam::Update()
{
    UpdateProperties();
    UpdateAppearance();
}

// sets the lock status
void eTeam::SetLocked( bool locked )
{
    if ( locked && !locked_ )
    {
        sn_ConsoleOut( tOutput( "$invite_team_locked", Name() ) );
    }
    if ( !locked && locked_ )
    {
        sn_ConsoleOut( tOutput( "$invite_team_unlocked", Name() ) );
    }

    locked_ = locked;
}

// returns the lock status
bool eTeam::IsLocked() const
{
    return locked_;
}

bool eTeam::IsLockedFor( const ePlayerNetID * p ) const
{
    bool isLocked = IsLocked();
#ifdef KRAWALL_SERVER
    isLocked = isLocked || ( p->GetAccessLevel() > ePlayerNetID::AccessLevelRequiredToPlay() );
#endif
    return isLocked;
}

static void se_UnlockAllTeams ( void )
{
    for ( int i = eTeam::teams.Len()-1; i>=0; --i )
    {
        (eTeam::teams(i))->SetLocked( false );
    }
}

static void se_UnlockAllTeamsConf ( std::istream & s )
{
    if ( se_NeedsServer( "UNLOCK_ALL_TEAMS", s ) )
    {
        return;
    }

    se_UnlockAllTeams();
}

static tConfItemFunc se_unlockAllTeamsConf("UNLOCK_ALL_TEAMS",&se_UnlockAllTeamsConf);
static tAccessLevelSetter se_unlockAllTemasConfLevel( se_unlockAllTeamsConf, tAccessLevel_Moderator );

// invite the player to join
void eTeam::Invite( ePlayerNetID * player )
{
    tASSERT( player );
    if ( !IsInvited( player ) && this->IsLockedFor( player ) )
    {
        sn_ConsoleOut( tOutput( "$invite_team_can_join", player->GetColoredName(), Name() ) );
    }
    else if ( !IsInvited( player ) )
    {
        sn_ConsoleOut( tOutput( "$invite_team_invite", player->GetColoredName(), Name() ) );
    }
    player->AddInvitation( this );
}

// revoke an invitation
void eTeam::UnInvite( ePlayerNetID * player )
{
    tASSERT( player );
    bool wasInvited = player->RemoveInvitation( this );
    if ( player->CurrentTeam() == this && this->IsLockedFor( player ) )
    {
        sn_ConsoleOut( tOutput( "$invite_team_kick", player->GetColoredName(), Name() ) );
        player->SetTeam(0);
    }
    else if ( wasInvited )
    {
        sn_ConsoleOut( tOutput( "$invite_team_uninvite", player->GetColoredName(), Name() ) );
    }
}

// check if a player is invited
bool eTeam::IsInvited( ePlayerNetID const * player ) const
{
    return player->invitations_.find( const_cast< eTeam * >( this ) ) != player->invitations_.end();
}

std::vector< const ePlayerNetID * > eTeam::InterestingInvitedPlayers() const
{
    std::vector< const ePlayerNetID * > invites;
    for ( int i = se_PlayerNetIDs.Len() - 1; i >= 0; --i )
    {
        ePlayerNetID *p = se_PlayerNetIDs( i );
        if ( IsInvited( p ) && p->CurrentTeam() != this )
            invites.push_back( p );
    }
    return invites;
}

void eTeam::AddScore ( int s )
{
    if( se_warmup.IsWarmupMode() )
        return;

    score += s;

    if ( nSERVER == sn_GetNetState() )
        RequestSync();
}

void eTeam::ResetScore ( )
{
    score = 0;

    if ( nSERVER == sn_GetNetState() )
        RequestSync();
}

void eTeam::SetScore ( int s )
{
    score = s;

    if ( nSERVER == sn_GetNetState() )
        RequestSync();
}

void eTeam::AddScore(int points,
                     const tOutput& reasonwin,
                     const tOutput& reasonlose,
                     const tOutput& reasonfree)
{
    if( se_warmup.IsWarmupMode() ){
        return;
    }

    // delegate to player if this is a one-player team
    if ( players.Len() == 1 && maxPlayersLocal == 1 )
    {
        players[0]->AddScore( points, reasonwin, reasonlose, reasonfree );
        return;
    }

    score += points;

    tOutput message;
    message.SetTemplateParameter(1, GetColoredName());
    message.SetTemplateParameter(2, abs(points));

    if (!points)
    {
        if (reasonfree.IsEmpty())
            return;
        else
            message.Append(reasonfree);
    }
    if (points>0)
    {
        if (reasonwin.IsEmpty())
            message << "$player_win_default";
        else
            message.Append(reasonwin);
    }
    else
    {
        if (reasonlose.IsEmpty())
            message << "$player_lose_default";
        else
            message.Append(reasonlose);
    }

    sn_ConsoleOut(message);

    if( points )
    {
        RequestSync(true);
        se_SaveToScoreFile(message);
    }
}

// *******************************************************************************
// *
// *	ResetScoreDifferences
// *
// *******************************************************************************
//!
//!
// *******************************************************************************

void eTeam::ResetScoreDifferences( void )
{
    for ( int i = teams.Len()-1; i>=0; --i )
    {
        eTeam* t = teams(i);
        if ( t->IsHuman() )
            t->lastScore_ = t->score;
    }
}

// *******************************************************************************
// *
// *	LogScoreDifferences
// *
// *******************************************************************************
//!
//!
// *******************************************************************************

void eTeam::LogScoreDifferences( void )
{
    for ( int i = teams.Len()-1; i>=0; --i )
    {
        eTeam* t = teams(i);
        t->LogScoreDifference();
    }
}

// *******************************************************************************
// *
// *	LogScoreDifference
// *
// *******************************************************************************
//!
//!
// *******************************************************************************

static eLadderLogWriter se_roundScoreTeamWriter( "ROUND_SCORE_TEAM", true, "score:int team" );

void eTeam::LogScoreDifference( void )
{
    LogScoreDifference( logName );
}

void eTeam::LogScoreDifference( const tString & teamName )
{
    if ( lastScore_ > IMPOSSIBLY_LOW_SCORE && IsHuman() )
    {
        tString ret;
        int scoreDifference = score - lastScore_;
        lastScore_ = IMPOSSIBLY_LOW_SCORE;
        se_roundScoreTeamWriter << scoreDifference << teamName;
        se_roundScoreTeamWriter.write();
    }
}

void eTeam::SwapTeamsNo(int a,int b){
    if (0>a || teams.Len()<=a)
        return;
    if (0>b || teams.Len()<=b)
        return;
    if (a==b)
        return;

    eTeam *A=teams(a);
    eTeam *B=teams(b);

    teams(b)=A;
    teams(a)=B;
    A->listID=b;
    B->listID=a;
}

void eTeam::SortByScore(){
    // bubble sort (AAARRGGH! but good for lists that change not much)

    bool inorder=false;
    while (!inorder){
        inorder=true;
        int i;
        for (i=teams.Len()-2;i>=0;i--)
            if (teams(i)->score < teams(i+1)->score){
                SwapTeamsNo(i,i+1);
                inorder=false;
            }
    }
}

tString eTeam::Ranking( int MAX, bool cut ){
    SortByScore();

    tString ret;

    if (teams.Len()>0){
        ret.SetPos(2, cut );
        ret << tOutput("$team_scoretable_name");
        ret.SetPos(25, cut );
        ret << tOutput("$team_scoretable_score");
        ret.SetPos(32, cut );
        ret << tOutput("$team_scoretable_members");
        ret.SetPos(41, cut );
        ret << tOutput("$team_scoretable_alive");
        ret << "\n";

        int max = teams.Len();
        if ( max > MAX && MAX > 0 )
        {
            max = MAX ;
        }
        for (int i=0;i<max;i++){
            tColoredString line;
            eTeam *t = teams(i);
            line << ColorString(t);
            tString name = t->Name();
            //name.RemoveHex();
            name.SetPos( 24, cut );

            line.SetPos(2, false );
            line << name;
            line.SetPos(25, false );
            line << t->score;
            line.SetPos(32, false );
            line << t->NumActivePlayers();
            line.SetPos(41, false);
            int alive=t->AlivePlayers();
            line << alive;
            ret << line << "\n";
        }
        if ( max < teams.Len() )
        {
            ret << " ...\n";
        }
    }
    // else
    //    ret << tOutput("$team_scoretable_nobody");
    return ret;
}
float eTeam::RankingGraph( float y, int MAX ){
    SortByScore();

    tColoredString ret;

    if (teams.Len()>0){
        tColoredString name;
        name << tColoredString::ColorString(1.,.5,.5)
        << tOutput("$team_scoretable_name");
        DisplayText(-.7, y, .06, name.c_str(), sr_fontScoretable, -1);
        tColoredString score;
        score << tOutput("$team_scoretable_score");
        DisplayText(-.3, y, .06, score.c_str(), sr_fontScoretable, -1);
        tColoredString members;
        members << tOutput("$team_scoretable_members");
        DisplayText(-.1, y, .06, members.c_str(), sr_fontScoretable, -1);
        tColoredString alive;
        alive << tOutput("$team_scoretable_alive");
        DisplayText(.3, y, .06, alive.c_str(), sr_fontScoretable, -1);
        y-=.06;

        int max = teams.Len();
        if ( max > MAX && MAX > 0 )
        {
            max = MAX ;
        }
        for(int i=0;i<max;i++){
            eTeam *t = teams(i);
            tColoredString name;
            name << ColorString(t) << t->Name();
            DisplayText(-.7, y, .06, name.c_str(), sr_fontScoretable, -1);
            tColoredString score;
            score << t->score;
            DisplayText(-.3, y, .06, score.c_str(), sr_fontScoretable, -1);
            tColoredString members;
            members << t->NumActivePlayers();
            DisplayText(-.1, y, .06, members.c_str(), sr_fontScoretable, -1);
            tColoredString alive;
            int alivep=t->AlivePlayers();
            if(alivep)
                alive << tColoredString::ColorString(0,1,0);
            else
                alive << tColoredString::ColorString(1,0,0);
            alive << alivep;
            DisplayText(.3, y, .06, alive.c_str(), sr_fontScoretable, -1);
            y-=.06;
        }
        if ( max < teams.Len() )
        {
            DisplayText(-.7, y, .06, "...", sr_fontScoretable, -1);
            y-=.06;
        }
    }
    return y;
}


// get the number of human players on the team
int	eTeam::NumHumanPlayers	(		) const
{
    return numHumans;
}

static int imbalance = 1;

// get the number of human players on the team
int	eTeam::NumAIPlayers	(		) const
{
    return numAIs;
}

// how many active players are there right now that can spawn next round?
int eTeam::NumActivePlayers(       ) const
{
    return numHumans + numAIs;
}

// make sure the limits on team number and such are met
void eTeam::EnforceConstraints()
{
    if ( maxImbalance < 1 )
        maxImbalance = 1;

    if ( minTeams > maxTeams )
        minTeams = maxTeams;

    Enforce( minTeams, maxTeams, maxImbalance );

    // reset imbalance count so players may try to switch teams in the next round
    if ( imbalance <= 0 )
        imbalance=0;
}

enum eTeamEliminationMode
{
    TEAM_ELIMINATION_SIZE  = 0, // eliminate smallest team
    TEAM_ELIMINATION_COLOR = 1, // eliminate ugliest team
    TEAM_ELIMINATION_SCORE = 2  // eliminate suckiest team
};

tCONFIG_ENUM( eTeamEliminationMode );

static eTeamEliminationMode se_teamEliminationMode = TEAM_ELIMINATION_SIZE;
static tSettingItem<eTeamEliminationMode> se_teamEliminationModeConf("TEAM_ELIMINATION_MODE", se_teamEliminationMode );

// make sure the limits on team number and such are met
void eTeam::Enforce( int minTeams, int maxTeams, int maxImbalance)
{
    if ( maxTeams < 1 )
        maxTeams = 1;

    /*
    // z-man: disabled for new "respect maxTeams and maxPlayers setting" logic
    if ( maxPlayers * maxTeams < se_PlayerNetIDs.Len() )
    {
        maxPlayers = ( se_PlayerNetIDs.Len()/maxTeams ) + 1;
    }
    */

    // nothing to be done on the clients
    if ( nCLIENT == sn_GetNetState() )
        return;

    if ( maxImbalance < 1 )
        maxImbalance = 1;

    if ( minTeams > maxTeams )
        minTeams = maxTeams;

    if ( minPlayers > maxPlayers )
        minPlayers = maxPlayers;

    bool balance = false;

    int giveUp = 10;
    while ( !balance && giveUp-- > 0 )
    {
        balance = true;

        // find the max and min number of players per team and the
        eTeam *max = NULL, *min = NULL, *ai = NULL, *lastColor = NULL, *last = NULL;
        int    maxP = minPlayers, minP = 100000;
        int    maxColorID = 0;

        int numTeams = 0;
        int numHumanTeams = 0;

        int i;
        for ( i = teams.Len()-1; i>=0; --i )
        {
            eTeam *t = teams(i);

            if ( t->BalanceThisTeam() )
            {
                int humans = t->NumHumanPlayers();

                numTeams++;

                if ( humans > 0 )
                    numHumanTeams++;
                else
                    ai = t;

                if ( humans > maxP )
                {
                    maxP = humans;
                    max  = t;
                }

                // mode 0: prefer unlocked teams as elimination victims, and of course smaller teams
                if ( ( humans > 0 || t->NumPlayers() == 0 ) && humans < minP && !t->IsLocked() )
                {
                    minP = humans;
                    min  = t;
                }

                // mode 1: keep the first teams (Team blue, Team gold, etc)
                if ( ( humans > 0 || t->NumPlayers() == 0 ) && t->colorID > maxColorID && t == se_ColoredTeams[t->colorID])
                {
                    maxColorID = t->colorID;
                    lastColor = t;
                }

                // mode 2: lowest score goes out
                if ( !last )
                {
                    last = t;
                }
            }
        }

        eTeam * teamToKill = NULL;
        if( (int)se_teamEliminationMode > 2 ) se_teamEliminationMode = TEAM_ELIMINATION_SIZE;
        // let negative values simply "lock" teams like you lock a server with a low MAX_CLIENTS
        switch ( se_teamEliminationMode )
        {
            case TEAM_ELIMINATION_SIZE:
                teamToKill = min;
                break;
            case TEAM_ELIMINATION_COLOR:
                teamToKill = lastColor;
                break;
            case TEAM_ELIMINATION_SCORE:
                teamToKill = last;
                break;
        }

        if ( ( numTeams > maxTeams && teamToKill ) || ( numTeams > minTeams && ai ) )
        {
            // too many teams. Destroy the smallest team.
            // better: destroy the AI team
            if ( ai )
                teamToKill = ai;

            for ( i = teamToKill->NumPlayers()-1; i>=0; --i )
            {
                // one player from the dismantled team.
                tJUST_CONTROLLED_PTR< ePlayerNetID > pni = teamToKill->Player(i);

                // just ignore AIs, they get removed later by the "balance with AIs" code once it notices all humans are gone from this team
                if ( !pni->IsHuman() )
                {
                    continue;
                }

                // find the second smallest team:
                eTeam* second = NULL;
                int secondMinP = maxPlayers; // the number of humans on that team
                for ( int j = teams.Len()-1; j>=0; --j )
                {
                    eTeam *t = teams(j);

                    if ( t->BalanceThisTeam() )
                    {
                        int humans = t->NumHumanPlayers();

                        if ( humans < secondMinP && t != teamToKill )
                        {
                            secondMinP = humans;
                            second = t;
                        }
                    }
                }

                if ( second )
                {
                    // put the player into the second smallest team, overriding balancing settings (they're likely to be in the way )
                    int imbBackup = second->maxImbalanceLocal;
                    second->maxImbalanceLocal = 99999;
                    pni->SetTeamForce( 0 );
                    pni->UpdateTeamForce();
                    pni->SetTeamForce( second );
                    pni->UpdateTeamForce();
                    second->maxImbalanceLocal = imbBackup;

                    balance = false;
                }
                else
                {
                    // no room, kick the player out
                    pni->SetTeamForce( NULL );
                    pni->UpdateTeamForce();

                    balance = false;
                }
            }
        }
        else if ( numTeams < minTeams )
        {
            // too few teams. Create a new one
            eTeam *newTeam = tNEW( eTeam );
            teams.Add( newTeam, newTeam->listID );
            newTeam->UpdateProperties();

            balance = false;
        }
        else if ( ( ( maxP - maxImbalance > minP || maxP > maxPlayers ) && minP < maxPlayers ) || ( minP == 0 && maxP > 1 ) )
        {
            // teams are unbalanced; move one player from the strongest team to the weakest
            if ( max )
            {
                ePlayerNetID* unluckyOne = max->YoungestHumanPlayer();
                unluckyOne->SetTeamForce( min );
                unluckyOne->UpdateTeamForce();
                balance = false;
            }
        }
        else if ( maxP > maxPlayers )
        {
            // teams too large. create a new team and put the last joiner of the strongest team in
            eTeam* newTeam = tNEW( eTeam );
            if ( max )
            {
                ePlayerNetID* unluckyOne = max->YoungestHumanPlayer();
                unluckyOne->SetTeamForce( newTeam );
                unluckyOne->UpdateTeamForce();

                balance = false;
            }
        }
    }
}

void eTeam::WritePlayers( eLadderLogWriter & writer, const eTeam *team )
{
    for ( int i = team->players.Len() - 1; i >= 0; --i )
    {
        writer << team->players( i )->GetLogName();
    }
}

static eLadderLogWriter se_onlinePlayerWriter( "ONLINE_PLAYER", true, "player ping:float team access_level:int total_score:int color_screen_name+" );
static eLadderLogWriter se_onlineAIWriter( "ONLINE_AI", true, "player team total_score:int color_screen_name+" );
static eLadderLogWriter se_onlineTeamWriter( "ONLINE_TEAM", true, "team total_score:int color_screen_name+" );
static eLadderLogWriter se_numHumansWriter( "NUM_HUMANS", false, "number_humans:int" );

// Writes the data for the ONLINE_PLAYER ladderlog event.
// Returns true if the player is a human and is active.
static bool se_WriteOnlinePlayerData( ePlayerNetID *player, eTeam *team )
{
    if ( !player->IsActive() )
        return false;

    bool isHuman = player->IsHuman();
    if ( isHuman )
    {
        se_onlinePlayerWriter << player->GetLogName();
        se_onlinePlayerWriter << player->ping;
        if ( team )
            se_onlinePlayerWriter << team->GetLogName();
        else
            se_onlinePlayerWriter << "";
        se_onlinePlayerWriter << player->GetAccessLevel();
        se_onlinePlayerWriter << player->Score();
        se_onlinePlayerWriter << player->GetUserName() << player->GetColoredName();
        se_onlinePlayerWriter.write();
    }
    else
    {
        se_onlineAIWriter << player->GetLogName();
        if ( team )
            se_onlineAIWriter << team->GetLogName();
        else
            se_onlineAIWriter << "";
        se_onlineAIWriter << player->Score();
        se_onlineAIWriter << player->GetUserName() << player->GetColoredName();
        se_onlineAIWriter.write();
    }
    return isHuman;    
}

void eTeam::WriteOnlinePlayers()
{
    if ( sn_GetNetState() != nSERVER )
        return;

    int numHumans = 0;

    // Write teams and players (ordered by launch position)
    for ( int i = teams.Len() - 1; i >= 0; --i )
    {
        eTeam *team = teams( i );
        se_onlineTeamWriter << team->GetLogName();
        se_onlineTeamWriter << team->Score();
        se_onlineTeamWriter << team->GetColoredName();
        se_onlineTeamWriter.write();

        for ( int j = 0; j < team->players.Len(); j++)
        {
            if ( se_WriteOnlinePlayerData( team->players( j ), team ) )
                numHumans++;
        }
    }

    // Write spectators
    for ( int i = se_PlayerNetIDs.Len() - 1; i >= 0; --i )
    {
        ePlayerNetID * p = se_PlayerNetIDs(i);
        if ( !p->CurrentTeam() )
            se_WriteOnlinePlayerData( p, NULL );
    }

    se_numHumansWriter << numHumans;
    se_numHumansWriter.write();
}

// inquire or set the ability to use a color as a team name
bool eTeam::NameTeamAfterColor ( bool wish )
{
    // reassign colors if colorID >= maxTeams
    if ( wish && colorID >= maxTeams && se_teamEliminationMode == TEAM_ELIMINATION_COLOR )
    {
        NameTeamAfterColor( false );
    }

    if ( wish && colorID < 0 )
    {
        for ( int i = 0; i < TEAMCOLORS; ++i )
        {
            if ( !se_ColoredTeams[i] )
            {
                se_ColoredTeams[i] = this;
                colorID = i;
                return true;
            }
        }
    }

    if ( !wish && colorID >= 0 )
    {
        se_ColoredTeams[ colorID ] = 0;
        colorID = -1;
    }

    return colorID >= 0;
}

// register a player
void eTeam::AddPlayer    ( ePlayerNetID* player )
{
    tASSERT( player );

    tJUST_CONTROLLED_PTR< eTeam > keepalive( this );

    if ( ! PlayerMayJoin( player ) )
        return;

    tJUST_CONTROLLED_PTR< eTeam > oldTeam( player->currentTeam );
    tString oldTeamName("Old Team (BUG)");
    if ( player->currentTeam )
    {
        oldTeamName = oldTeam->Name();
        player->currentTeam->RemovePlayerDirty( player );
        oldTeam->UpdateProperties();
        oldTeam->UpdateAppearance();
    }

    players.Add( player, player->teamListID );
    // bool teamChange = player->currentTeam;
    player->currentTeam = this;
    player->timeJoinedTeam = tSysTimeFloat();

    UpdateProperties();

    // print the new entry
    if ( players.Len() <= 1 )
    {
        UpdateAppearance();

        /*
        // print creation message
        tOutput message;
        message.SetTemplateParameter(1, player->GetName() );
        message.SetTemplateParameter(2, Name() );
        message << "$player_creates_team";

        sn_ConsoleOut( message );
        */
    }


    se_teamAddWriter << logName << player->GetLogName();
    se_teamAddWriter.write();

    // anounce joining if there are is more than one member now or if the team is color-named
    if ( sn_GetNetState() != nCLIENT )
    {
        // get colored player name
        tColoredString playerName;
        playerName << *player << tColoredString::ColorString(.5,1,.5);

        // tString playerNameNoColor = tColoredString::RemoveColors( player->GetName() );

        if ( ( players.Len() > 1 || colorID >= 0 ) && IsHuman() )
        {
            if ( oldTeam && oldTeam->players.Len() >= 1 )
            {
                sn_ConsoleOut( tOutput( "$player_changes_team",
                                        playerName,
                                        Name(),
                                        oldTeamName ) );
            }
            else
            {
                // print join message
                sn_ConsoleOut( tOutput( "$player_joins_team_start",
                                        playerName,
                                        Name() ) );
            }
        }
        else if ( oldTeam )
        {
            // or at least the leaving of the old team
            if ( oldTeam->players.Len() > 0 )
                sn_ConsoleOut( tOutput( "$player_leaves_team",
                                        playerName,
                                        oldTeamName ) );
        }
        else
        {
            // announce a generic join
            sn_ConsoleOut( tOutput( "$player_entered_game", playerName ) );
        }
    }

    if ( listID < 0 )
    {
        teams.Add ( this, listID );
    }

    player->UpdateName();
}

// register a player the dirty way
void eTeam::AddPlayerDirty   ( ePlayerNetID* player )
{
    tASSERT( player );

    if ( player->currentTeam )
    {
        player->currentTeam->RemovePlayerDirty ( player );
    }

    players.Add( player, player->teamListID );
    player->currentTeam = player->nextTeam = this;
    player->timeJoinedTeam = tSysTimeFloat();

    if ( listID < 0 )
    {
        teams.Add ( this, listID );
    }

    player->UpdateName();

    se_teamAddWriter << logName << player->GetLogName();
    se_teamAddWriter.write();
}

// deregister a player
void eTeam::RemovePlayerDirty ( ePlayerNetID* player )
{
    tASSERT( player );
    tASSERT( player->currentTeam == this );

    // remove player without shuffling the list
    for ( int i = players.Len()-2; i >= player->teamListID; --i )
    {
        ePlayerNetID * shuffle = players(i);
        players.Remove( shuffle, shuffle->teamListID );
        players.Add   ( shuffle, shuffle->teamListID );
    }
    tASSERT ( player->teamListID == players.Len()-1 );

    // now player has been shuffled to the back of the list without disturbing
    // the order of the other players and can be removed
    players.Remove ( player, player->teamListID );
    player->currentTeam = NULL;

    se_teamRemoveWriter << logName << player->GetLogName();
    se_teamRemoveWriter.write();

    // remove team from list
    if ( listID >= 0 && players.Len() == 0 )
    {
        teams.Remove( this, listID );

        // correctly log removal
        UpdateAppearance();

        // don't forget the colored team list
        if ( colorID >= 0 )
        {
            se_ColoredTeams[ colorID ] = 0;
            colorID = -1;
        }
    }
}

// deregister a player
void eTeam::RemovePlayer ( ePlayerNetID* player )
{
    tCONTROLLED_PTR( eTeam ) safety;
    safety = this; 						// avoid premature destruction of this team

    RemovePlayerDirty( player );

    player->UpdateName();

    // get colored player name
    tColoredString playerName;
    playerName << *player << tColoredString::ColorString(1,.5,.5);

    if ( sn_GetNetState() != nCLIENT )
    {
        if ( players.Len() > 0 || colorID >= 0  )
        {
            sn_ConsoleOut( tOutput( "$player_leaves_team",
                                    playerName,
                                    Name() ) );
        }
        else
        {
            // announce a generic leave
            sn_ConsoleOut( tOutput( "$player_leaving_game", playerName ) );
        }
    }

    UpdateProperties();

    // trigger enforcement of strong constraints on next balancing if the player is quitting
    if ( enforceRulesOnQuit && 0 == player->nextTeam && nCLIENT != sn_GetNetState() )
        imbalance = -10;
}


// see if the given player may join this team
bool eTeam::PlayerMayJoin( const ePlayerNetID* player ) const
{
    // a player who is already on the team can "join" the team
    if (player->currentTeam==this)
        return true;

    // AI players are always allowed to join, the logic that tries to put the AI into
    // this team is responsible for checking
    if ( !player->IsHuman() )
        return true;

    // suspended players cannot join
    if ( player->GetSuspended() > 0 )
        return false;

    // check for invitations. Not with those shoes!
    if ( IsLockedFor( player ) && !IsInvited( player ) )
    {
        return false;
    }

    int maxInb = maxImbalanceLocal;

    int minP = 10000; // minimum number of humans in a team after the player left
    if ( bool(player) && bool(player->currentTeam) )
    {
        minP = player->currentTeam->NumHumanPlayers() - 1;

        // allow leaving a team if it vanishes and the number of teams does not shrink below the minimum team count
        if ( minP == 0 && teams.Len() > minTeams )
            minP = 10000;
    }

    for ( int i = teams.Len()-1; i>=0; --i )
    {
        eTeam *t = teams(i);

        if ( !t->IsLocked() && t->BalanceThisTeam() )
        {
            int humans = t->NumHumanPlayers();

            if ( humans < minP )
            {
                minP = humans;
            }
        }
    }

    int maxPlayers = maxPlayersLocal;

    // we must have room           and the joining must not cause huge imbalance
    if ( numHumans < maxPlayers && ( sn_GetNetState() != nSERVER || minP + maxInb > numHumans ) )
        return true;

    // always allow circular swapping of players
    {
        std::set< eTeam const * > swapTargets; // teams players from this team want to swap into (recursively, if someone wants to swap to B and someone else from B wants to swap to C, C is on the list, too)
        swapTargets.insert( this );

        bool goon = true;
        while ( goon )
        {
            goon = false;
            for ( std::set< eTeam const * >::iterator iter = swapTargets.begin(); iter != swapTargets.end(); ++iter )
            {
                eTeam const * team = *iter;
                for ( int i = team->players.Len()-1; i>=0; --i )
                {
                    ePlayerNetID * otherPlayer = team->players(i);
                    eTeam * swapTeam = otherPlayer->NextTeam();
                    if ( swapTeam && swapTeam != otherPlayer->CurrentTeam() && swapTargets.find( swapTeam ) == swapTargets.end() )
                    {
                        goon = true;
                        swapTargets.insert( swapTeam );

                        // early return if we find a closed swap chain
                        if ( swapTeam == player->CurrentTeam() )
                            return true;
                    }
                }
            }
        }
    }

    // sorry, no way
    return false;
}


// is it allowed to create a new team?
bool eTeam::NewTeamAllowed	()
{
    return teams.Len() < maxTeams;
}

// if this flag is set, the center player is the boss of a team.
// if it isn't set, the oldest player is boss.
static bool se_centerPlayerIsBoss=true;
static tSettingItem<bool> se_centerPlayerIsBossConf("TEAM_CENTER_IS_BOSS", se_centerPlayerIsBoss );

// the oldest player
ePlayerNetID*	eTeam::OldestPlayer	(		) const
{
    ePlayerNetID* ret = NULL;

    for (int i= players.Len()-1; i>=0; i--)
    {
        ePlayerNetID* p = players(i);
        if (!ret || ret->timeJoinedTeam > p->timeJoinedTeam || se_centerPlayerIsBoss )
        {
            ret = p;
        }
    }

    return ret;
}

// the oldest human player
ePlayerNetID*	eTeam::OldestHumanPlayer(		) const
{
    ePlayerNetID* ret = NULL;

    for (int i= players.Len()-1; i>=0; i--)
    {
        ePlayerNetID* p = players(i);
        if ( p->IsHuman() && ( !ret || ret->timeJoinedTeam > p->timeJoinedTeam || se_centerPlayerIsBoss ) )
        {
            ret = p;
        }
    }

    if ( !ret )
    {
        // nobody? Darn. Look for a player that has this team set as next team.
        
        for (int i= se_PlayerNetIDs.Len()-1; i>=0; i--)
        {   
            ePlayerNetID* p = se_PlayerNetIDs(i);
            if ( p->NextTeam() == this && p->IsHuman() && ( !ret || ret->timeJoinedTeam > p->timeJoinedTeam || se_centerPlayerIsBoss ) )
            {
                ret = p;
            }
        }
    }

    return ret;
}

// the oldest AI player
ePlayerNetID*	eTeam::OldestAIPlayer	(		) const
{
    ePlayerNetID* ret = NULL;

    for (int i= players.Len()-1; i>=0; i--)
    {
        ePlayerNetID* p = players(i);
        if ( ( !p->IsHuman() ) && ( !ret || ret->timeJoinedTeam > p->timeJoinedTeam || se_centerPlayerIsBoss ) )
        {
            ret = p;
        }
    }

    return ret;
}

// the youngest player
ePlayerNetID*	eTeam::YoungestPlayer	(		) const
{
    ePlayerNetID* ret = NULL;

    for (int i= players.Len(); i>=0; i--)
    {
        ePlayerNetID* p = players(i);
        if (!ret || ret->timeJoinedTeam < p->timeJoinedTeam )
        {
            ret = p;
        }
    }

    return ret;
}

// the youngest human player
ePlayerNetID*	eTeam::YoungestHumanPlayer(		) const
{
    ePlayerNetID* ret = NULL;

    for (int i= players.Len()-1; i>=0; i--)
    {
        ePlayerNetID* p = players(i);
        if ( p->IsHuman() && ( !ret || ret->timeJoinedTeam < p->timeJoinedTeam ) )
        {
            ret = p;
        }
    }

    return ret;
}

// the youngest AI player
ePlayerNetID*	eTeam::YoungestAIPlayer	(		) const
{
    ePlayerNetID* ret = NULL;

    for (int i= players.Len()-1; i>=0; i--)
    {
        ePlayerNetID* p = players(i);
        if ( ( !p->IsHuman() ) && ( !ret || ret->timeJoinedTeam < p->timeJoinedTeam ) )
        {
            ret = p;
        }
    }

    return ret;
}

// is anyone still alive?
bool eTeam::Alive ( ) const
{
    for (int i= players.Len()-1; i>=0; --i)
    {
        ePlayerNetID* p = players(i);
        if ( p->Object() && p->Object()->Alive() )
        {
            return true;
        }
    }

    return false;
}

// how many of the current players are currently alive?
int eTeam::AlivePlayers ( ) const
{
    int ret = 0;
    for (int i= players.Len()-1; i>=0; --i)
    {
        ePlayerNetID* p = players(i);
        if ( p->Object() && p->Object()->Alive() )
        {
            ret++;
        }
    }

    return ret;
}

bool eTeam::IsReady ( ) const
{
    int ready = 0;
    int len = players.Len();
    for (int i = len-1; i>=0; --i)
    {
        ePlayerNetID * p = players(i);
        if ( !p->IsHuman() )
            len--;
        if ( p->ready )
            ready++;
    }
    if (len)
        return ready >= len * se_minReady;
    else
        return true;
}

// print out an understandable name in to s
void eTeam::PrintName(tString &s) const
{
    s << "Team " << name;
}



// we must not transmit an object that contains pointers to non-transmitted objects.
// this function is supposed to check that.
bool eTeam::ClearToTransmit(int user) const
{
    return true;
}


// syncronisation functions:

//! writes sync data (and initialization data if flat is set)
void eTeam::WriteSync( Engine::TeamSync & sync, bool init )
{
    nNetObject::WriteSync( *sync.mutable_base(), init );

    color.WriteSync( *sync.mutable_color() );
    sync.set_name( name );
    sync.set_max_players( maxPlayersLocal );
    sync.set_max_imbalance( maxImbalanceLocal );
    sync.set_score( score );
}


//! reads incremental sync data. Returns false if sync was invalid or old.
void eTeam::ReadSync( Engine::TeamSync const & sync, nSenderInfo const & init )
{
    nNetObject::ReadSync( sync.base(), init );

    color.ReadSync( sync.color() );
    name = sync.name();
    maxPlayersLocal = sync.max_players();
    maxImbalanceLocal = sync.max_imbalance();
    score = sync.score();

    // update colored player names
    if ( sn_GetNetState() != nSERVER )
    {
        for ( int i = players.Len()-1; i>=0; --i )
        {
            players(i)->UpdateName();
        }
    }
}

/*
// is the message newer	than the last accepted sync
bool eTeam::SyncIsNew(nMessage &m)
{
    return true;
}


// the extra information sent on creation:
// store sync message in m
// the information written by this function should
// be read from the message in the "message"- connstructor
void eTeam::WriteCreate(nMessage &m)
{
    nNetObject::WriteCreate(m);
}

*/

// con/desstruction
// default constructor
eTeam::eTeam()
        :colorID(-1),listID(-1), roundsPlayed(0)
{
    score = 0;
    lastScore_=IMPOSSIBLY_LOW_SCORE;
    locked_ = false;
    spawnPoint = NULL;
    maxPlayersLocal = maxPlayers;
    maxImbalanceLocal = maxImbalance;
    color.r_ = color.g_ = color.b_ = 32; // initialize color so it will be updated, guaranteed
    lastEmpty_=true;
    lastWasCustomTeamName_ = false;
    Update();
}


// remote constructor
eTeam::eTeam( Engine::TeamSync const & sync, nSenderInfo const & sender )
: nNetObject( sync.base(), sender ),
  colorID(-1),listID(-1)
{
    score = 0;
    lastScore_=IMPOSSIBLY_LOW_SCORE;
    locked_ = false;
    maxPlayersLocal = maxPlayers;
    maxImbalanceLocal = maxImbalance;
    color.r_ = color.g_ = color.b_ = 32; // initialize color so it will be updated, guaranteed
    lastEmpty_=true;
    lastWasCustomTeamName_ = false;
    Update();
}

// destructor
eTeam::~eTeam()
{
    // one last time
    UpdateAppearance();

    if ( listID >= 0 )
        teams.Remove( this, listID );

    if ( colorID >= 0 )
    {
        se_ColoredTeams[ colorID ] = 0;
        colorID = -1;
    }

    // revoke all invitations
    for ( int i = se_PlayerNetIDs.Len()-1; i >= 0; --i )
    {
        se_PlayerNetIDs(i)->RemoveInvitation( this );
    }
}
// *******************************************************************************
// *
// *	Enemies
// *
// *******************************************************************************
//!
//!		@param	team	the team to check
//!		@param	player	the player to check
//!		@return		    true if the player has an enemy in the team
//!
// *******************************************************************************

bool eTeam::Enemies( eTeam const * team, ePlayerNetID const * player )
{
    // nonexistant parties can't be enemies
    if (!player || !team)
        return false;

    // check if the player is a team member
    if ( player->CurrentTeam() == team )
        return false;

    // check if the player has a team
    if ( !player->currentTeam )
        return false;

    // go through player list; the player is an enemy if he is at least enemy with one of the menbers
    for (int i = team->players.Len()-1; i>=0; --i)
        if ( ePlayerNetID::Enemies( team->players(i), player ) )
            return true;

    return false;
}

// *******************************************************************************
// *
// *	Enemies
// *
// *******************************************************************************
//!
//!		@param	team1
//!		@param	team2
//!		@return
//!
// *******************************************************************************

bool eTeam::Enemies( eTeam const * team1, eTeam const * team2 )
{
    // nonexistant parties can't be enemies
    if (!team1 || !team2 || team1 == team2)
        return false;

    // go through player list; if one is an enemy, so is the team
    for (int i = team2->players.Len()-1; i>=0; --i)
        if ( Enemies( team1, team2->players(i) ) )
            return true;

    return false;
}

// *******************************************************************************
// *
// *	SwapPlayers
// *
// *******************************************************************************
//!
//!		@param	player1	first player to swap positions
//!		@param	player2	second player to swap positions
//!
// *******************************************************************************

void eTeam::SwapPlayers( ePlayerNetID * player1, ePlayerNetID * player2 )
{
    tASSERT( player1 );
    tASSERT( player2 );

    // swap IDs
    int id3 = player1->teamListID;
    player1->teamListID = player2->teamListID;
    player2->teamListID = id3;

    // adjust pointers from teams
    eTeam * team2 = player1->CurrentTeam();
    eTeam * team1 = player2->CurrentTeam();

    if ( team2 )
        team2->players[player2->teamListID] = player2;
    if ( team1 )
        team1->players[player1->teamListID] = player1;

    // swap teams
    player1->currentTeam = team1;
    player2->currentTeam = team2;

    // swap next teams (if current teams differ)
    team1 = player2->NextTeam();
    team2 = player1->NextTeam();
    if ( player1->currentTeam != player2->currentTeam )
    {
        player1->nextTeam = team1;
        player2->nextTeam = team2;
    }
}

// *******************************************************************************
// *
// *	Shuffle
// *
// *******************************************************************************
//!
//!		@param	startID	player ID to move around
//!		@param	stopID	player ID to move it to
//!
// *******************************************************************************

void eTeam::Shuffle( int startID, int stopID )
{
    tASSERT( 0 <= startID && startID < players.Len() );
    tASSERT( 0 <= stopID && stopID < players.Len() );

    if ( startID == stopID )
        return;
    
    ePlayerNetID *player = players[startID];
    eShuffleSpamTester & spam = player->GetShuffleSpam();
    
    if ( spam.ShouldAnnounce() )
    {
        sn_ConsoleOut( spam.ShuffleMessage( player, startID + 1, stopID + 1 ) );
    }

    // simply swap the one player over all the players in between.
    while ( startID < stopID )
    {
        SwapPlayers( players[startID], players[startID+1] );
        startID++;
    }
    while ( startID > stopID )
    {
        SwapPlayers( players[startID], players[startID-1] );
        startID--;
    }
    
    spam.Shuffle();
}

tColoredString eTeam::GetColoredName(void) const
{
    tColoredString ret;
    return ret << tColoredString::ColorString( R() / 15.0, G() / 15.0, B() / 15.0)
        << Name()
        << tColoredString::ColorString(-1, -1, -1);
}
