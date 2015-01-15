/**
 * @file
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_doorclose.h"

static void LET_DoorSlidingClose (le_t* le)
{
	assert(le->slidingSpeed > 0);
	LET_SlideDoor(le, -le->slidingSpeed);
}

static void LET_DoorRotatingClose (le_t* le)
{
	assert(le->rotationSpeed > 0);
	LET_RotateDoor(le, -le->rotationSpeed);
}

/**
 * @brief Callback for EV_DOOR_CLOSE event - rotates the inline model and recalc routing
 * @sa EV_DOOR_CLOSE
 * @sa G_ClientUseEdict
 * @sa Touch_DoorTrigger
 */
void CL_DoorClose (const eventRegister_t* self, dbuffer* msg)
{
	/* get local entity */
	int number;

	NET_ReadFormat(msg, self->formatString, &number);

	le_t* le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);

	if (le->type == ET_DOOR) {
		LE_SetThink(le, LET_DoorRotatingClose);
		le->think(le);
	} else if (le->type == ET_DOOR_SLIDING) {
		LE_SetThink(le, LET_DoorSlidingClose);
		le->think(le);
	} else {
		Com_Error(ERR_DROP, "Invalid door entity found of type: %i", le->type);
	}
}
