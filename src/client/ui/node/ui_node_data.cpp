/**
 * @file
 * @brief The <code>data</code> behaviour allows to store a string/float number/integer into a node.
 * It is a virtual node which is not displayed nor activatable by input device like mouse.
 * The node can store 3 types, which have no relations together.
 * @code
 * data mystring { string "fooo" }
 * data myfloat { number 1.2 }
 * data myint { integer 2 }
 * @endcode
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "ui_node_data.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"

#include "../../../common/scripts_lua.h"

#define EXTRADATA_TYPE dataExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

void UI_RegisterDataNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "data";
	behaviour->isVirtual = true;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->manager = UINodePtr(new uiDataNode());
	behaviour->lua_SWIG_typeinfo = UI_SWIG_TypeQuery("uiDataNode_t *");

	/* Store a string into the node.
	 * @note you should note store a cvar ref
	 * @todo use a REF_STRING when it is possible
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "string");

	/* Store a float number into the node. */
	UI_RegisterExtradataNodeProperty(behaviour, "number", V_FLOAT, EXTRADATA_TYPE, number);

	/* Store a integer number into the node. */
	UI_RegisterExtradataNodeProperty(behaviour, "integer", V_INT, EXTRADATA_TYPE, number);
}
