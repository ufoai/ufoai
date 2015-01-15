/**
 * @file
 * @brief HUD related routines.
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

#define ACTOR_HAND_CHAR_RIGHT (char)'r'
#define ACTOR_HAND_CHAR_LEFT (char)'l'

/** @param[in] hand Hand index (ACTOR_HAND_RIGHT, ACTOR_HAND_LEFT) */
#define ACTOR_GET_HAND_CHAR(hand) ((hand) == ACTOR_HAND_LEFT ? ACTOR_HAND_CHAR_LEFT : ACTOR_HAND_CHAR_RIGHT)
/** @param[in] hand Hand index (ACTOR_HAND_CHAR_RIGHT, ACTOR_HAND_CHAR_LEFT) */
#define ACTOR_GET_HAND_INDEX(hand) ((hand) == ACTOR_HAND_CHAR_LEFT ? ACTOR_HAND_LEFT : ACTOR_HAND_RIGHT)

extern cvar_t* cl_worldlevel;

/* check above for removal */

void HUD_InitStartup(void);
void HUD_InitUI(const char* optionWindowName);
void HUD_DisplayMessage(const char*  text);
void HUD_Update(void);
void HUD_UpdateActorStats(const struct le_s* le);
void HUD_UpdateCursor(void);
int HUD_ReactionFireGetTUs (const struct le_s* actor);
