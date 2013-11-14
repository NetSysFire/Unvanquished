/*
===========================================================================

Copyright 2013 Unvanquished Developers

This file is part of Daemon.

Daemon is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

#include "../../engine/qcommon/q_shared.h"
#include "bg_public.h"

#ifdef GAME
#include "../game/g_local.h"
#endif

#ifdef CGAME
#include "../cgame/cg_local.h"
#endif

#ifdef UI
#include "../../engine/client/ui_api.h"
#endif

// -----------
// definitions
// -----------

typedef struct unlockable_s
{
	int      type;
	int      num;
	team_t   team;
	qboolean unlocked;
	qboolean statusKnown;
	qboolean unlockThreshold;
	qboolean lockThreshold;
} unlockable_t;

// ----
// data
// ----

qboolean         unlockablesDataAvailable;
team_t           unlockablesTeamKnowledge;

unlockable_t     unlockables[ NUM_UNLOCKABLES ];
int              unlockablesMask[ NUM_TEAMS ];

int              unlockablesTypeOffset[ UNLT_NUM_UNLOCKABLETYPES ];

// -------------
// local methods
// -------------

static const char *UnlockableHumanName( unlockable_t *unlockable )
{
	switch ( unlockable->type )
	{
		case UNLT_WEAPON:    return BG_Weapon( unlockable->num )->humanName;
		case UNLT_UPGRADE:   return BG_Upgrade( unlockable->num )->humanName;
		case UNLT_BUILDABLE: return BG_Buildable( unlockable->num )->humanName;
		case UNLT_CLASS:     return BG_ClassModelConfig( unlockable->num )->humanName;
	}

	Com_Error( ERR_FATAL, "UnlockableHumanName: Unlockable has unknown type" );
	return NULL;
}

static qboolean Disabled( unlockable_t *unlockable )
{
	switch ( unlockable->type )
	{
		case UNLT_WEAPON:    return BG_WeaponDisabled( unlockable->num );
		case UNLT_UPGRADE:   return BG_UpgradeDisabled( unlockable->num );
		case UNLT_BUILDABLE: return BG_BuildableDisabled( unlockable->num );
		case UNLT_CLASS:     return BG_ClassDisabled( unlockable->num );
	}

	Com_Error( ERR_FATAL, "Disabled: Unlockable has unknown type" );
	return qfalse;
}

static void InformUnlockableStatusChange( unlockable_t *unlockable, qboolean unlocked )
{
}

static void InformUnlockableStatusChanges( int *statusChanges, int count )
{
	char         text[ MAX_STRING_CHARS ];
	char         *textptr = text;
	int          unlockableNum;
	qboolean     firstPass = qtrue;
	unlockable_t *unlockable;

	for ( unlockableNum = 0; unlockableNum < NUM_UNLOCKABLES; unlockableNum++ )
	{
		unlockable = &unlockables[ unlockableNum ];

		if ( !statusChanges[ unlockableNum ] || Disabled( unlockable ) )
		{
			continue;
		}

		if ( firstPass )
		{
			if ( statusChanges[ unlockableNum ] > 0 )
			{
				Com_sprintf( text, sizeof( text ),
				             S_COLOR_GREEN "ITEM%s UNLOCKED: " S_COLOR_WHITE, ( count > 1 ) ? "S" : "" );
			}
			else
			{
				Com_sprintf( text, sizeof( text ),
				             S_COLOR_RED   "ITEM%s LOCKED: "   S_COLOR_WHITE, ( count > 1 ) ? "S" : "" );
			}

			textptr = text + strlen( text );
			firstPass = qfalse;
		}
		else
		{
			Com_sprintf( textptr, sizeof( text ) - ( textptr - text ), ", " );
			textptr += 2;
		}

		Com_sprintf( textptr, sizeof( text ) - ( textptr - text ), "%s", UnlockableHumanName( unlockable ) );
		textptr += strlen( textptr );
	}

	Com_Printf( "%s\n", text );
}


static INLINE qboolean Unlocked( unlockableType_t type, int itemNum )
{
	return unlockables[ unlockablesTypeOffset[ type ] + itemNum ].unlocked;
}

static INLINE void CheckStatusKnowledge( unlockableType_t type, int itemNum )
{
	unlockable_t dummy;

	if ( !unlockables[ unlockablesTypeOffset[ type ] + itemNum ].statusKnown )
	{
		dummy.type = type;
		dummy.num  = itemNum;

		Com_Printf( S_WARNING "Asked for the status of unlockable item %s but the status is unknown.\n",
		            UnlockableHumanName( &dummy ) );
	}
}

static float UnlockToLockThreshold( float unlockThreshold )
{
	float momentumHalfLife = 0.0f;
	float unlockableMinTime  = 0.0f;
#ifdef UI
	char  buffer[ MAX_TOKEN_CHARS ];
#endif

	// maintain cache
	static float lastMomentumHalfLife = 0.0f;
	static float lastunlockableMinTime  = 0.0f;
	static float lastMod                = 0.0f;

	// retrieve relevant settings
#ifdef GAME
	momentumHalfLife = g_momentumHalfLife.value;
	unlockableMinTime  = g_unlockableMinTime.value;
#endif
#ifdef CGAME
	momentumHalfLife = cgs.momentumHalfLife;
	unlockableMinTime  = cgs.unlockableMinTime;
#endif
#ifdef UI
	trap_Cvar_VariableStringBuffer( "ui_momentumHalfLife", buffer, sizeof( buffer ) );
	sscanf( buffer, "%f", &momentumHalfLife );
	trap_Cvar_VariableStringBuffer( "ui_unlockableMinTime",  buffer, sizeof( buffer ) );
	sscanf( buffer, "%f", &unlockableMinTime );
#endif

	// a half life time of 0 means there is no decrease, so we don't need to alter thresholds
	if ( momentumHalfLife <= 0.0f )
	{
		return unlockThreshold;
	}

	// do cache lookup
	if ( lastMomentumHalfLife == momentumHalfLife &&
	     lastunlockableMinTime  == unlockableMinTime  &&
	     lastMod > 0.0f )
	{
		return lastMod * unlockThreshold;
	}

	lastMomentumHalfLife = momentumHalfLife;
	lastunlockableMinTime  = unlockableMinTime;

	// ln(2) ~= 0.6931472
	lastMod = exp( -0.6931472f * ( unlockableMinTime / ( momentumHalfLife * 60.0f ) ) );

	return lastMod * unlockThreshold;
}

// ----------
// BG methods
// ----------

void BG_InitUnlockackables( void )
{
	unlockablesDataAvailable = qfalse;
	unlockablesTeamKnowledge = TEAM_NONE;

	memset( unlockables, 0, sizeof( unlockables ) );
	memset( unlockablesMask, 0, sizeof( unlockablesMask ) );

	unlockablesTypeOffset[ UNLT_WEAPON ]    = 0;
	unlockablesTypeOffset[ UNLT_UPGRADE ]   = WP_NUM_WEAPONS;
	unlockablesTypeOffset[ UNLT_BUILDABLE ] = unlockablesTypeOffset[ UNLT_UPGRADE ]   + UP_NUM_UPGRADES;
	unlockablesTypeOffset[ UNLT_CLASS ]     = unlockablesTypeOffset[ UNLT_BUILDABLE ] + BA_NUM_BUILDABLES;

#ifdef GAME
	G_UpdateUnlockables();
#endif
}

void BG_ImportUnlockablesFromMask( team_t team, int mask )
{
	int              unlockableNum, teamUnlockableNum = 0, itemNum = 0, unlockThreshold;
	unlockable_t     *unlockable;
	unlockableType_t unlockableType = 0;
	team_t           currentTeam;
	qboolean         newStatus;
	int              statusChanges[ NUM_UNLOCKABLES ];
#ifdef CGAME
	int statusChangeCount = 0;
#endif

	// maintain a cache to prevent redundant imports
	static int    lastMask = 0;
	static team_t lastTeam = TEAM_NONE;

	// just import if data is unavailable, cached mask is outdated or team has changed
	if ( unlockablesDataAvailable && team == lastTeam && mask == lastMask )
	{
		return;
	}

	// cache input
	lastMask = mask;
	lastTeam = team;

	// no status change yet
	memset( statusChanges, 0, sizeof( statusChanges ) );

	for ( unlockableNum = 0; unlockableNum < NUM_UNLOCKABLES; unlockableNum++ )
	{
		unlockable = &unlockables[ unlockableNum ];

		// also iterate over item types, itemNum is a per-type counter
		if ( unlockableType < UNLT_NUM_UNLOCKABLETYPES - 1 &&
		     unlockableNum == unlockablesTypeOffset[ unlockableType + 1 ] )
		{
			unlockableType++;
			itemNum = 0;
		}

		switch ( unlockableType )
		{
			case UNLT_WEAPON:
				currentTeam     = BG_Weapon( itemNum )->team;
				unlockThreshold = BG_Weapon( itemNum )->unlockThreshold;
				break;

			case UNLT_UPGRADE:
				currentTeam     = TEAM_HUMANS;
				unlockThreshold = BG_Upgrade( itemNum )->unlockThreshold;
				break;

			case UNLT_BUILDABLE:
				currentTeam     = BG_Buildable( itemNum )->team;
				unlockThreshold = BG_Buildable( itemNum )->unlockThreshold;
				break;

			case UNLT_CLASS:
				currentTeam     = TEAM_ALIENS;
				unlockThreshold = BG_Class( itemNum )->unlockThreshold;
				break;

			default:
				Com_Error( ERR_FATAL, "BG_ImportUnlockablesFromMask: Unknown unlockable type" );
		}

		unlockThreshold = MAX( unlockThreshold, 0 );

		unlockable->type            = unlockableType;
		unlockable->num             = itemNum;
		unlockable->team            = currentTeam;
		unlockable->unlockThreshold = unlockThreshold;
		unlockable->lockThreshold   = UnlockToLockThreshold( unlockThreshold );

		// retrieve the item's locking state
		if ( !unlockThreshold )
		{
			unlockable->statusKnown = qtrue;
			unlockable->unlocked    = qtrue;
		}
		else if ( currentTeam == team )
		{
			newStatus = mask & ( 1 << teamUnlockableNum );

#ifdef CGAME
			// inform client on status change
			if ( unlockablesTeamKnowledge == team && unlockable->statusKnown &&
			     unlockable->unlocked != newStatus )
			{
				InformUnlockableStatusChange( unlockable, newStatus );

				statusChanges[ unlockableNum ] = newStatus ? 1 : -1;
				statusChangeCount++;
			}
#endif

			unlockable->statusKnown = qtrue;
			unlockable->unlocked    = newStatus;

			teamUnlockableNum++;
		}
		else
		{
			unlockable->statusKnown = qfalse;
			unlockable->unlocked    = qfalse;
		}

		itemNum++;
	}

#ifdef CGAME
	// notify client about status changes
	if ( statusChangeCount )
	{
		InformUnlockableStatusChanges( statusChanges, statusChangeCount );
	}

	// export team and mask into cvar for UI
	trap_Cvar_Set( "ui_unlockables", va( "%d %d", team, mask ) );
#endif

	// we only know the state for one team
	unlockablesDataAvailable = qtrue;
	unlockablesTeamKnowledge = team;

	// save mask for later use
	unlockablesMask[ team ] = mask;
}

int BG_UnlockablesMask( team_t team )
{
	if ( unlockablesTeamKnowledge != team && unlockablesTeamKnowledge != TEAM_ALL )
	{
		Com_Error( ERR_FATAL, "G_GetUnlockablesMask: Requested mask for a team with unknown unlockable status" );
	}

	return unlockablesMask[ team ];
}

unlockableType_t BG_UnlockableType( int num )
{
	return ( (unsigned) num < NUM_UNLOCKABLES ) ? unlockables[ num ].type : UNLT_NUM_UNLOCKABLETYPES;
}

int BG_UnlockableTypeIndex( int num )
{
	return ( (unsigned) num < NUM_UNLOCKABLES ) ? unlockables[ num ].num : 0;
}

qboolean BG_WeaponUnlocked( weapon_t weapon )
{
	CheckStatusKnowledge( UNLT_WEAPON, ( int )weapon );

	return Unlocked( UNLT_WEAPON, ( int )weapon );
}

qboolean BG_UpgradeUnlocked( upgrade_t upgrade )
{
	CheckStatusKnowledge( UNLT_UPGRADE, ( int )upgrade );

	return Unlocked( UNLT_UPGRADE, ( int )upgrade );
}

qboolean BG_BuildableUnlocked( buildable_t buildable )
{
	CheckStatusKnowledge( UNLT_BUILDABLE, ( int )buildable );

	return Unlocked( UNLT_BUILDABLE, ( int )buildable );
}

qboolean BG_ClassUnlocked( class_t class_ )
{
	CheckStatusKnowledge( UNLT_CLASS, ( int )class_ );

	return Unlocked( UNLT_CLASS, ( int )class_ );
}

static int MomentumNextThreshold( int threshold )
{
	int next = 1 << 30;
	int i;

	for ( i = 0; i < NUM_UNLOCKABLES; ++i )
	{
		int thisThreshold = unlockables[ i ].unlocked ? unlockables[ i ].lockThreshold : unlockables[ i ].unlockThreshold;

		if ( thisThreshold > threshold && thisThreshold < next )
		{
			next = thisThreshold;
		}
	}

	return next < ( 1 << 30 ) ? next : 0;
}

momentumThresholdIterator_t BG_IterateMomentumThresholds( momentumThresholdIterator_t unlockableIter, team_t team, int *threshold, qboolean *unlocked )
{
	static const momentumThresholdIterator_t finished = { -1, 0 };

	if ( unlockableIter.num < 0 )
	{
		unlockableIter.num = -1;
	}

	for ( ++unlockableIter.num; unlockableIter.num < NUM_UNLOCKABLES; unlockableIter.num++ )
	{
		unlockable_t unlockable = unlockables[ unlockableIter.num ];
		int          thisThreshold = unlockable.unlocked ? unlockable.lockThreshold : unlockable.unlockThreshold;

		if ( unlockable.team == team && unlockable.unlockThreshold && ( !unlockableIter.threshold || unlockableIter.threshold == thisThreshold ) )
		{
			*unlocked = unlockable.unlocked;
			*threshold = thisThreshold;

			return unlockableIter;
		}
	}

	if ( unlockableIter.threshold )
	{
		unlockableIter.threshold = MomentumNextThreshold( unlockableIter.threshold );

		if ( unlockableIter.threshold )
		{
			unlockableIter.num = -1;
			return BG_IterateMomentumThresholds( unlockableIter, team, threshold, unlocked );
		}
	}

	return finished;
}

// ------------
// GAME methods
// ------------

#ifdef GAME
static void UpdateUnlockablesMask( void )
{
	int    unlockable, unlockableNum[ NUM_TEAMS ];
	team_t team;

	for ( team = TEAM_NONE + 1; team < NUM_TEAMS; team++ )
	{
		unlockableNum[ team ] = 0;
		unlockablesMask[ team ] = 0;
	}

	for ( unlockable = 0; unlockable < NUM_UNLOCKABLES; unlockable++ )
	{
		if ( unlockables[ unlockable ].unlockThreshold )
		{
			team = unlockables[ unlockable ].team;

			if ( unlockableNum[ team ] > 15 ) // 16 bit available for transmission
			{
				Com_Error( ERR_FATAL, "UpdateUnlockablesMask: Number of unlockable items for a team exceeded" );
			}

			if ( !unlockables[ unlockable ].statusKnown )
			{
				Com_Error( ERR_FATAL, "UpdateUnlockablesMask: Called before G_UpdateUnlockables" );
			}

			if ( unlockables[ unlockable ].unlocked )
			{
				unlockablesMask[ team ] |= ( 1 << unlockableNum[ team ] );
			}

			unlockableNum[ team ]++;
		}
	}
}
#endif

#ifdef GAME
void G_UpdateUnlockables( void )
{
	int              itemNum = 0, unlockableNum, unlockThreshold;
	float            momentum;
	unlockable_t     *unlockable;
	unlockableType_t unlockableType = 0;
	team_t           team;

	for ( unlockableNum = 0; unlockableNum < NUM_UNLOCKABLES; unlockableNum++ )
	{
		unlockable = &unlockables[ unlockableNum ];

		// also iterate over item types, itemNum is a per-type counter
		while ( unlockableType < UNLT_NUM_UNLOCKABLETYPES - 1 &&
		        unlockableNum == unlockablesTypeOffset[ unlockableType + 1 ] )
		{
			unlockableType++;
			itemNum = 0;
		}

		switch ( unlockableType )
		{
			case UNLT_WEAPON:
				team            = BG_Weapon( itemNum )->team;
				unlockThreshold = BG_Weapon( itemNum )->unlockThreshold;
				break;

			case UNLT_UPGRADE:
				team            = TEAM_HUMANS;
				unlockThreshold = BG_Upgrade( itemNum )->unlockThreshold;
				break;

			case UNLT_BUILDABLE:
				team            = BG_Buildable( itemNum )->team;
				unlockThreshold = BG_Buildable( itemNum )->unlockThreshold;
				break;

			case UNLT_CLASS:
				team            = TEAM_ALIENS;
				unlockThreshold = BG_Class( itemNum )->unlockThreshold;
				break;

			default:
				Com_Error( ERR_FATAL, "G_UpdateUnlockables: Unknown unlockable type" );
		}

		unlockThreshold = MAX( unlockThreshold, 0 );
		momentum = level.team[ team ].momentum;

		unlockable->type            = unlockableType;
		unlockable->num             = itemNum;
		unlockable->team            = team;
		unlockable->statusKnown     = qtrue;
		unlockable->unlockThreshold = unlockThreshold;
		unlockable->lockThreshold   = UnlockToLockThreshold( unlockThreshold );

		// calculate the item's locking state
		unlockable->unlocked = (
		    !unlockThreshold || momentum >= unlockThreshold ||
		    ( unlockable->unlocked && momentum >= unlockable->lockThreshold )
		);

		itemNum++;

		/*Com_Printf( "G_UpdateUnlockables: Team %s, Type %s, Item %s, Momentum %d, Threshold %d, "
		            "Unlocked %d, Synchronize %d\n",
		            BG_TeamName( team ), UnlockableTypeName( unlockable ), UnlockableName( unlockable ),
		            momentum, unlockThreshold, unlockable->unlocked, unlockable->synchronize );*/
	}

	// GAME knows about all teams
	unlockablesDataAvailable = qtrue;
	unlockablesTeamKnowledge = TEAM_ALL;

	// generate masks for network transmission
	UpdateUnlockablesMask();
}
#endif

// -------------
// CGAME methods
// -------------

#ifdef CGAME
void CG_UpdateUnlockables( playerState_t *ps )
{
	BG_ImportUnlockablesFromMask( ps->persistant[ PERS_TEAM ], ps->persistant[ PERS_UNLOCKABLES ] );
}
#endif

// ----------
// UI methods
// ----------

#ifdef UI
void UI_UpdateUnlockables( void )
{
	char   buffer[ MAX_TOKEN_CHARS ];
	team_t team;
	int    mask;

	trap_Cvar_VariableStringBuffer( "ui_unlockables", buffer, sizeof( buffer ) );
	sscanf( buffer, "%d %d", ( int * )&team, &mask );

	BG_ImportUnlockablesFromMask( team, mask );
}
#endif
