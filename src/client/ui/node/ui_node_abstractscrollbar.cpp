/**
 * @file
 * @brief The <code>abstractscrollbar</code> is an abstract node (we can't instantiate it).
 * It exists to share same properties for vertical and horizontal scrollbar.
 * At the moment only the concrete <code>vscrollbar</code>.
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

#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_lua.h"

#include "ui_node_abstractscrollbar.h"

#include "../../../common/scripts_lua.h"

#define EXTRADATA_TYPE abstractScrollbarExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, abstractScrollbarExtraData_t)

/**
 * @brief Set the position of the scrollbar to a value
 */
void UI_AbstractScrollbarNodeSet (uiNode_t* node, int value)
{
	int pos = value;

	if (pos < 0) {
		pos = 0;
	} else if (pos > EXTRADATA(node).fullsize - EXTRADATA(node).viewsize) {
		pos = EXTRADATA(node).fullsize - EXTRADATA(node).viewsize;
	}
	if (pos < 0)
		pos = 0;

	/* nothing change */
	if (EXTRADATA(node).pos == pos)
		return;

	/* update status */
	EXTRADATA(node).pos = pos;

	/* fire change event */
	if (node->onChange) {
		UI_ExecuteEventActions(node, node->onChange);
	}
	if (node->lua_onChange != LUA_NOREF) {
		UI_ExecuteLuaEventScript(node, node->lua_onChange);
	}
}

void UI_RegisterAbstractScrollbarNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "abstractscrollbar";
	behaviour->isAbstract = true;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->manager = UINodePtr(new uiAbstractScrollbarNode());
	behaviour->lua_SWIG_typeinfo = UI_SWIG_TypeQuery("uiAbstractScrollbarNode_t *");

	/* Current position of the scroll. Image of the <code>viewpos</code> from <code>abstractscrollable</code> node. */
	UI_RegisterExtradataNodeProperty(behaviour, "current", V_INT, EXTRADATA_TYPE, pos);
	/* Image of the <code>viewsize</code> from <code>abstractscrollable</code> node. */
	UI_RegisterExtradataNodeProperty(behaviour, "viewsize", V_INT, EXTRADATA_TYPE, viewsize);
	/* Image of the <code>fullsize</code> from <code>abstractscrollable</code> node. */
	UI_RegisterExtradataNodeProperty(behaviour, "fullsize", V_INT, EXTRADATA_TYPE, fullsize);

	/* If true, hide the scroll when the position is 0 and can't change (when <code>viewsize</code> >= <code>fullsize</code>). */
	UI_RegisterExtradataNodeProperty(behaviour, "hidewhenunused", V_BOOL, EXTRADATA_TYPE, hideWhenUnused);
}
