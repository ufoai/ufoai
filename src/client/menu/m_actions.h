/**
 * @file m_actions.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_MENU_M_ACTIONS_H
#define CLIENT_MENU_M_ACTIONS_H

#include "../../common/common.h"

typedef enum ea_s {
	EA_NULL,
	EA_CMD,
	EA_CALL,
	EA_SET,
	EA_IF,
	EA_NUM_EVENTACTION
} ea_t;

typedef enum {
	EA_VALUE,						/**< char string value */
	EA_RAWVALUE,					/**< parsed value */
	EA_CVARNAME,					/**< a cvar name */
	EA_PATHPROPERTY,				/**< a property identify by a path */
	EA_THISMENUNODENAMEPROPERTY,	/**< a property identify by a node name into 'this' menu */
	EA_THISPROPERTY,				/**< a property of 'this' node */
} ea_paramType;

typedef struct {
	unsigned char op;
	unsigned char param1;
	unsigned char param2;
} ea_opcode;

typedef struct menuAction_s {
	ea_opcode type;

	void *data;
	struct menuAction_s *next;
	const value_t *scriptValues;
} menuAction_t;

/* prevent preprocessing of 'printf' into __attribute__ */
#define _SAVE_PRINTF_ printf
#undef printf

#ifdef __GNUC__
void MN_ExecuteConfunc(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void MN_ExecuteConfunc(const char *fmt, ...);
#endif

#define printf _SAVE_PRINTF_

/* prototype */
struct menu_s;

void MN_ExecuteActions(const struct menu_s* const menu, const menuAction_t* const first);
void MN_ExecuteEventActions(const menuNode_t* source, const menuAction_t* firstAction);
void MN_ExecuteConFuncActions(const menuNode_t* source, const menuAction_t* firstAction);

qboolean MN_FocusNextActionNode(void);
qboolean MN_FocusExecuteActionNode(void);
void MN_FocusRemove(void);

menuAction_t *MN_SetMenuAction(menuAction_t** action, int type, const void *data);

#endif
