/**
* Copyright [2013-2014] [OHsystem]
*
* We spent a lot of time writing this code, so show some respect:
* - Do not remove this copyright notice anywhere (bot, website etc.)
* - We do not provide support to those who removed copyright notice
*
* OHSystem is free software: You can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* You can contact the developers on: admin@ohsystem.net
* or join us directly here: http://ohsystem.net/forum/
*
* Visit us also on http://ohsystem.net/ and keep track always of the latest
* features and changes.
*
*
* This is modified from GHOST++: http://ghostplusplus.googlecode.com/
* Official GhostPP-Forum: http://ghostpp.com/
*/

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "game.h"
#include "stats.h"
#include "statsdota.h"
#include "statsw3mmd.h"

#include <stdio.h>
#include <cmath>
#include <string.h>
#include <time.h>
#include <boost/thread.hpp>

//
// sorting classes
//

class CGamePlayerSortAscByPing
{
public:
    bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const
    {
        return Player1->GetPing( false ) < Player2->GetPing( false );
    }
};

class CGamePlayerSortDescByPing
{
public:
    bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const
    {
        return Player1->GetPing( false ) > Player2->GetPing( false );
    }
};

//
// CGame
//

CGame :: CGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer, uint32_t nGameType, uint32_t nHostCounter ) : CBaseGame( nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, nOwnerName, nCreatorName, nCreatorServer, nGameType, nHostCounter ), m_DBBanLast( NULL ), m_Stats( NULL ) ,m_CallableGameAdd( NULL ), m_ForfeitTime( 0 ), m_ForfeitTeam( 0 ), m_EarlyDraw( false )
{
    m_DBGame = new CDBGame( 0, string( ), m_Map->GetMapPath( ), string( ), string( ), string( ), 0 );
    m_GameAlias = m_Map->GetAlias();
    m_GameAliasName = m_GHost->GetAliasName( m_GameAlias );
    m_lGameAliasName = m_GameAliasName;
    transform( m_lGameAliasName.begin( ), m_lGameAliasName.end( ), m_lGameAliasName.begin( ), ::tolower );
    if( m_lGameAliasName.find("lod") != string :: npos || m_lGameAliasName.find("dota") != string :: npos || m_lGameAliasName.find("imba") != string :: npos ) {
        m_Stats = new CStatsDOTA( this );
    } else if( m_Map->GetAlias() != 0 ) {
        m_Stats = new CStatsW3MMD( this, m_Map->GetMapStatsW3MMDCategory( ) );
    } else {
        CONSOLE_Print( m_GHost->m_Language->NoMapAliasRecordFound( ));
    }

    if( m_GHost->m_VoteMode &&! m_Map->GetPossibleModesToVote( ).empty( ) )
        GetVotingModes( m_Map->GetPossibleModesToVote( ) );

    m_LobbyLog.clear();
    m_GameLog.clear();
    m_ObservingPlayers = 0;
    m_LastLeaverTime = GetTime();
}

CGame :: ~CGame( )
{
    // autoban
    uint32_t EndTime = m_GameTicks / 1000;
    uint32_t Counter = 0;
    for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
    {
        if( IsAutoBanned( (*i)->GetName( ) ) )
        {
            uint32_t VictimLevel = 0;
            for( vector<CBNET *> :: iterator k = m_GHost->m_BNETs.begin( ); k != m_GHost->m_BNETs.end( ); ++k )
            {
                if( (*k)->GetServer( ) == (*i)->GetSpoofedRealm( ) || (*i)->GetSpoofedRealm( ) == m_GHost->m_WC3ConnectAlias)
                {
                    VictimLevel = (*k)->IsLevel( (*i)->GetName( ) );
                    break;
                }
            }

            uint32_t LeftTime = (*i)->GetLeft( );
            // make sure that draw games will not ban people who didnt leave.
            if( EndTime - LeftTime > 300 || m_EarlyDraw && LeftTime != EndTime )
            {
                if( m_EarlyDraw )
                    Counter++;
                if( Counter <= 2 && VictimLevel <= 2 )
                {
                    uint32_t BanTime = 0;
                    string Reason = m_GHost->m_Language->DisconnectedAt()+" ";
                    if((*i)->GetLeftReason( ).find("left")!=string::npos) {
                        Reason = m_GHost->m_Language->LeftAt()+" ";
                    }
                    switch((*i)->GetLeaverLevel( )) {
                        case 0:
                            BanTime = 7200;
                            break;
                        case 1:
                            BanTime = 86400;
                            break;
                        case 2:
                            BanTime = 259200;
                            break;
                        case 3:
                            BanTime = 604800;
                            break;
                        default:
                            BanTime = 604800;
                        break;
                    }
                    if( EndTime < 300 ) {
                        Reason += UTIL_ToString( LeftTime/60 ) + "/" + UTIL_ToString( EndTime/60 )+"min";
                    } else {
                        string EndMin = UTIL_ToString(EndTime/60);
                        string EndSec = UTIL_ToString(EndTime%60);
                        string LeftMin = UTIL_ToString(LeftTime/60);
                        string LeftSec = UTIL_ToString(LeftTime%60);
                        if(1==EndSec.size())
                            EndSec="0"+EndSec;
                        if(1==LeftSec.size())
                            LeftSec="0"+LeftSec;

                        Reason += LeftMin+":"+LeftSec + "/" + EndMin+":"+EndSec;
                    }
                    m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedBanAdd( (*i)->GetSpoofedRealm(), (*i)->GetName( ), (*i)->GetIP(), m_GameName, m_GHost->m_BotManagerName, Reason, BanTime, ""  ) );
                }
            }
        }
    }

    /* last update before the game is over */
    if( m_LogData != "" )
    {
        m_LogData = m_LogData + "1" + "\t" + "pl";
        //UPDATE SLOTS
        for( unsigned char i = 0; i < m_Slots.size( ); ++i )
        {
            if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[i].GetComputer( ) == 0 )
            {
                CGamePlayer *player = GetPlayerFromSID( i );
                if( player )
                    m_LogData = m_LogData + "\t" + player->GetName( );
                else if( !player && m_GameLoaded )
                    m_LogData = m_LogData + "\t" + "-";
            }
            else if( m_Slots[i].GetSlotStatus( ) == SLOTSTATUS_OPEN )
                m_LogData = m_LogData + "\t" + "-";
        }
        m_LogData = m_LogData + "\n";
        m_PairedLogUpdates.push_back( PairedLogUpdate( string( ), m_GHost->m_DB->ThreadedStoreLog( m_HostCounter, m_LogData,  m_AdminLog ) ) );
        m_LogData = string();
        m_AdminLog = vector<string>();
        m_PlayerUpdate = false;
        m_LastLogDataUpdate = GetTime();
    }
    
    if( m_CallableGameAdd && m_CallableGameAdd->GetReady( ) )
    {

        if( m_CallableGameAdd->GetResult( ) > 0 )
        {
            CONSOLE_Print( "[GAME: " + m_GameName + "] saving player/stats data to database" );

            // store the CDBGamePlayers in the database

            for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
                m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedGamePlayerAdd( m_CallableGameAdd->GetResult( ), (*i)->GetName( ), (*i)->GetIP( ), (*i)->GetSpoofed( ), (*i)->GetSpoofedRealm( ), (*i)->GetReserved( ), (*i)->GetLoadingTime( ), (*i)->GetLeft( ), (*i)->GetLeftReason( ), (*i)->GetTeam( ), (*i)->GetColour( ), (*i)->GetID() ) );

            if( m_Stats )
            {
                m_Stats->Save( m_GHost, m_GHost->m_DB, m_CallableGameAdd->GetResult( ) );
            }
        }
        else
            CONSOLE_Print( "[GAME: " + m_GameName + "] unable to save player/stats data to database" );

        m_GHost->m_DB->RecoverCallable( m_CallableGameAdd );
        delete m_CallableGameAdd;
        m_CallableGameAdd = NULL;
    }

    for( vector<PairedPUp> :: iterator i = m_PairedPUps.begin( ); i != m_PairedPUps.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedBanCheck> :: iterator i = m_PairedBanChecks.begin( ); i != m_PairedBanChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<Pairedpm> :: iterator i = m_Pairedpms.begin( ); i != m_Pairedpms.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedGSCheck> :: iterator i = m_PairedGSChecks.begin( ); i != m_PairedGSChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedRankCheck> :: iterator i = m_PairedRankChecks.begin( ); i != m_PairedRankChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedStreakCheck> :: iterator i = m_PairedStreakChecks.begin( ); i != m_PairedStreakChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedSCheck> :: iterator i = m_PairedSChecks.begin( ); i != m_PairedSChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedPWCheck> :: iterator i = m_PairedPWChecks.begin( ); i != m_PairedPWChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedPassCheck> :: iterator i = m_PairedPassChecks.begin( ); i != m_PairedPassChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedSS> :: iterator i = m_PairedSSs.begin( ); i != m_PairedSSs.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<PairedRegAdd> :: iterator i = m_PairedRegAdds.begin( ); i != m_PairedRegAdds.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); ++i )
        delete *i;

    delete m_DBGame;

    for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
        delete *i;

    delete m_Stats;

    // it's a "bad thing" if m_CallableGameAdd is non NULL here
    // it means the game is being deleted after m_CallableGameAdd was created (the first step to saving the game data) but before the associated thread terminated
    // rather than failing horribly we choose to allow the thread to complete in the orphaned callables list but step 2 will never be completed
    // so this will create a game entry in the database without any gameplayers and/or DotA stats

    if( m_CallableGameAdd )
    {
        CONSOLE_Print( "[GAME: " + m_GameName + "] game is being deleted before all game data was saved, game data has been lost" );
        m_GHost->m_Callables.push_back( m_CallableGameAdd );
    }
}

bool CGame :: Update( void *fd, void *send_fd )
{
    // update callables
    return CBaseGame :: Update( fd, send_fd );
}

void CGame :: EventPlayerDeleted( CGamePlayer *player )
{

    CBaseGame :: EventPlayerDeleted( player );

    // record everything we need to know about the player for storing in the database later
    // since we haven't stored the game yet (it's not over yet!) we can't link the gameplayer to the game
    // see the destructor for where these CDBGamePlayers are stored in the database
    // we could have inserted an incomplete record on creation and updated it later but this makes for a cleaner interface

    if( m_GameLoading || m_GameLoaded )
    {
        // todotodo: since we store players that crash during loading it's possible that the stats classes could have no information on them
        // that could result in a DBGamePlayer without a corresponding DBDotAPlayer - just be aware of the possibility
        unsigned char SID = GetSIDFromPID( player->GetPID( ) );
        unsigned char Team = 255;
        unsigned char Colour = 255;

        if( SID < m_Slots.size( ) )
        {
            Team = m_Slots[SID].GetTeam( );
            Colour = m_Slots[SID].GetColour( );
        }

		player->SetLeftTime( m_GameTicks / 1000 );
        m_DBGamePlayers.push_back( new CDBGamePlayer( player->GetID (), 0, player->GetName( ), player->GetExternalIPString( ), player->GetSpoofed( ) ? 1 : 0, player->GetSpoofedRealm( ), player->GetReserved( ) ? 1 : 0, player->GetFinishedLoading( ) ? player->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks : 0, m_GameTicks / 1000, player->GetLeftReason( ), Team, Colour, player->GetLeaverLevel() ) );

        // also keep track of the last player to leave for the !banlast command

        for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); ++i )
        {
            if( (*i)->GetName( ) == player->GetName( ) )
                m_DBBanLast = *i;
        }

        // if this was early leave, suggest to draw the game
        if( m_GameTicks < 1000 * 60 )
            SendAllChat( m_GHost->m_Language->UseDrawToDrawGame( ) );

        if( Team != 12 && m_GameOverTime == 0 && m_ForfeitTime == 0 )
        {
            // check if everyone on leaver's team left but other team has more than two players
            char sid, team;
            uint32_t CountAlly = 0;
            //the current player who is leaving isnt counted yet because of this, the player gets after this trigger deleted, thats why the autoend isnt triggering correctly.
            //on 5v3 games nothing triggers but on 4v3, when it is triggering with spread of 2. The enemycount need to be set to 1 to fix this
            // the count-process is done on the next vector-check
            uint32_t CountEnemy = 1;

            // in case a new player is leaving
            if( m_EndGame ) {
                m_EndGame = false;
                m_EndTicks = 0;
                m_StartEndTicks = 0;
                m_BreakAutoEndVotes = 0;
                m_BreakAutoEndVotesNeeded = 0;
                m_LoosingTeam = 0;
                for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
                {
                    (*i)->SetVotedForInterruption( false );
                }
            }

            for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++)
            {
                if( *i && !(*i)->GetLeftMessageSent( ) )
                {
                    char sid = GetSIDFromPID( (*i)->GetPID( ) );
                    if( sid != 255 )
                    {
                        char team = m_Slots[sid].GetTeam( );
                        if( team == Team )
                            CountAlly++;
                        else if(team!=12)
                            CountEnemy++;
                    }
                }
            }

            uint32_t spread = CountAlly > CountEnemy ? CountAlly - CountEnemy : CountEnemy - CountAlly;

            if( spread <= 1 && !player->GetSafeDrop()  && m_GHost->m_AutobanAll && player->GetLeavePerc () > 30)
            {
                m_AutoBans.push_back( player->GetName( ) );
                SendAllChat( m_GHost->m_Language->UserMayBanned( player->GetName( ) ) );
            }

            if( m_GHost->m_MaxAllowedSpread <= spread && m_Stats && CountAlly < CountEnemy )
            {
                if( m_GHost->m_HideMessages && GetTime( ) - m_LastLeaverTime >= 60 )
                    SendAllChat( m_GHost->m_Language->AutoEndHighSpread(UTIL_ToString(spread)) );
                m_LoosingTeam = Team;
                m_EndGame = true;
                m_BreakAutoEndVotesNeeded = CountAlly-1;
            }
            // here is no spread and we actually need the full remaining players
            else if( CountAlly+(CountEnemy-1) <= m_GHost->m_MinPlayerAutoEnd && m_Stats )
            {
                SendAllChat( m_GHost->m_Language->AutoEndTooFewPlayers( ) );
                m_LoosingTeam = Team;
                m_EndGame = true;
                m_BreakAutoEndVotesNeeded = CountAlly-1;
            }
            //this can be simple done by setting the trigger to 1 instead of 2
            //weired but this wasnt working correctly, this should make sure all things in every case if one side has left completely.
            else if( CountAlly == 0 && CountEnemy >= 1 || CountAlly >= 1 && CountEnemy == 0 )
            {
                // if less than one minute has elapsed, draw the game
                // this may be abused for mode voting and such, but hopefully not (and that's what bans are for)
                if( m_GameTicks < 1000 * 180 )
                {
                    m_Stats->SetWinner( ( Team + 1 ) % 2 );
                    if( m_GHost->m_HideMessages && GetTime( ) - m_LastLeaverTime >= 60 )
                        SendAllChat( m_GHost->m_Language->AutoEndToDraw( ) );
                    m_GameOverTime = GetTime();
                }

                // otherwise, if more than fifteen minutes have elapsed, give the other team the win
                else if( m_GameTicks > 1000 * 180 && m_Stats )
                {
                    if( m_GHost->m_HideMessages && GetTime( ) - m_LastLeaverTime >= 60 )
                        SendAllChat( m_GHost->m_Language->AutoEndOneTeamRemain( ) );
                    m_Stats->SetWinner( ( Team + 1 ) % 2 );
                    m_SoftGameOver = true;
                    m_LoosingTeam = Team;
                    m_GameOverTime = GetTime();
                }
            }

            if( m_EndGame && m_GHost->m_AutoEndTime != 0 )
            {
                string LTeam = m_LoosingTeam % 2  == 0 ? "Sentinel" : "Scourge";
                if( m_GHost->m_HideMessages && GetTime( ) - m_LastLeaverTime >= 60 )
                    SendAllChat(m_GHost->m_Language->AutoEndSpreadNotify( LTeam, UTIL_ToString(m_BreakAutoEndVotesNeeded) ) );
                m_EndTicks = GetTicks();
                m_StartEndTicks = GetTicks();
            }
        }

        // if stats and not solo, and at least two leavers in first four minutes, then draw the game
        if( !m_SoftGameOver && m_Stats && m_GameOverTime == 0 && Team != 12 && m_GameTicks < 1000 * 60 * 5 && m_GHost->m_EarlyEnd )
        {
            // check how many leavers, by starting from start players and subtracting each non-leaver player
            uint32_t m_NumLeavers = m_StartPlayers;

            for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++)
            {
                if( *i && !(*i)->GetLeftMessageSent( ) && *i != player )
                    m_NumLeavers--;
            }

            if( m_NumLeavers >= 2 )
            {
                SendAllChat( m_GHost->m_Language->AutoEndEarlyDrawOne() );
                SendAllChat( m_GHost->m_Language->AutoEndEarlyDrawTwo() );
                SendAllChat( m_GHost->m_Language->AutoEndEarlyDrawThree() );

                // make sure leavers will get banned
                m_EarlyDraw = true;
                m_SoftGameOver = true;
                m_GameOverTime = GetTime();
            }
        }
        m_LastLeaverTime = GetTime( );
    }
}

bool CGame :: EventPlayerAction( CGamePlayer *player, CIncomingAction *action )
{
    bool success = CBaseGame :: EventPlayerAction( player, action );

    // give the stats class a chance to process the action

    if( success && m_Stats && m_Stats->ProcessAction( action ) && m_GameOverTime == 0 )
    {
        CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started" );
        SendEndMessage( );
        m_GameOverTime = GetTime( );
    }
    return success;
}
bool CGame :: EventPlayerBotCommand( CGamePlayer *player, string command, string payload )
{
    return false;
}

void CGame :: EventGameStarted( )
{
    CBaseGame :: EventGameStarted( );

    // record everything we need to ban each player in case we decide to do so later
    // this is because when a player leaves the game an admin might want to ban that player
    // but since the player has already left the game we don't have access to their information anymore
    // so we create a "potential ban" for each player and only store it in the database if requested to by an admin

    if( m_DBBans.empty() )
    {
        for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
        {
            m_DBBans.push_back( new CDBBan( (*i)->GetJoinedRealm( ), (*i)->GetName( ), (*i)->GetExternalIPString( ), string( ), string( ), string( ), string( ), string(), string(), string(), string(), string() ) );
        }
    }
}

bool CGame :: IsGameDataSaved( )
{
    return m_CallableGameAdd && m_CallableGameAdd->GetReady( );
}

void CGame :: SaveGameData( )
{
    CONSOLE_Print( "[GAME: " + m_GameName + "] saving game data to database" );
    m_CallableGameAdd = m_GHost->m_DB->ThreadedGameAdd( m_GHost->m_BNETs.size( ) == 1 ? m_GHost->m_BNETs[0]->GetServer( ) : string( ), m_DBGame->GetMap( ), m_GameName, m_OwnerName, m_GameTicks / 1000, m_GameState, m_CreatorName, m_CreatorServer, m_GameType, m_LobbyLog, m_GameLog,m_DatabaseID );
    m_GHost->m_FinishedGames++;
    m_GHost->m_CheckForFinishedGames = GetTime();
    DoGameUpdate(true);
}

bool CGame :: IsAutoBanned( string name )
{
    for( vector<string> :: iterator i = m_AutoBans.begin( ); i != m_AutoBans.end( ); i++ )
    {
        if( *i == name )
            return true;
    }

    return false;
}

bool CGame :: CustomVoteKickReason( string reason )
{
    transform( reason.begin( ), reason.end( ), reason.begin( ), ::tolower );
    //Votekick reasons: maphack, fountainfarm, feeding, flaming, game ruin
    if( reason.find( "maphack" ) != string::npos || reason.find( "fountainfarm" ) != string::npos || reason.find( "feeding" ) != string::npos || reason.find( "flaming" ) != string::npos || reason.find( "gameruin" ) != string::npos || reason.find( "lagging" ) != string::npos )
        return true;

    return false;
}

string CGame :: GetRuleTags( )
{
    string Tags;
    uint32_t saver = 0;
    for( vector<string> :: iterator i = m_GHost->m_Rules.begin( ); i != m_GHost->m_Rules.end( ); i++ )
    {
        string tag;
        stringstream SS;
        SS << *i;
        SS >> tag;
        if( Tags.empty())
            Tags = tag;
        else
            Tags += ", " + tag;
        ++saver;
        if( saver > 10 )
        {
            CONSOLE_Print( "There to many rules, stopping after 10.");
            break;
        }
    }
    return Tags;
}

string CGame :: GetRule( string tag )
{
    transform( tag.begin( ), tag.end( ), tag.begin( ), ::tolower );
    uint32_t saver = 0;
    for( vector<string> :: iterator i = m_GHost->m_Rules.begin( ); i != m_GHost->m_Rules.end( ); i++ )
    {
        string rtag;
        string rule;
        stringstream SS;
        SS << *i;
        SS >> rtag;

        if( !SS.fail( ) && ( rtag == tag || rtag.find( tag ) != string :: npos))
        {
            if( SS.eof( ) )
                CONSOLE_Print( "[RULE: "+rtag+"] missing input #2 (Rule)." );
            else
            {
                getline( SS, rule );
                string :: size_type Start = rule.find_first_not_of( " " );

                if( Start != string :: npos )
                    rule = rule.substr( Start );

                return "["+rtag+"] "+rule;
            }
            return m_GHost->m_Language->WrongContactBotOwner();
        }
        ++saver;
        if( saver > 10 )
        {
            CONSOLE_Print( "There to many rules, stopping after 10.");
            return m_GHost->m_Language->WrongContactBotOwner();
            break;
        }
    }
    return m_GHost->m_Language->RuleTagNotify();
}

void CGame :: PlayerUsed(string thething, uint32_t thetype, string playername) {
    switch(thetype) {
        case 1:
            SendAllChat(m_GHost->m_Language->UserUsed1( playername, thething ) );
            break;
        case 2:
            SendAllChat(m_GHost->m_Language->UserUsed2( playername, thething ) );
            break;
        case 3:
            SendAllChat(m_GHost->m_Language->UserUsed3( playername, thething ) );
            break;
        case 4:
            SendAllChat(m_GHost->m_Language->UserUsed4( playername, thething ) );
            break;
        case 5:
            SendAllChat(m_GHost->m_Language->UserUsed5( playername, thething ) );
            break;
        default:
            SendAllChat(m_GHost->m_Language->UserUsed6( playername, thething ) );
        break;
    }
}
