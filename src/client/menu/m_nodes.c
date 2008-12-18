/**
 * @file m_nodes.c
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

#include "m_main.h"
#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"
#include "../cl_input.h"
#include "../../game/q_shared.h"

#include "node/m_node_abstractnode.h"
#include "node/m_node_abstractscrollbar.h"
#include "node/m_node_abstractvalue.h"
#include "node/m_node_bar.h"
#include "node/m_node_base.h"
#include "node/m_node_button.h"
#include "node/m_node_checkbox.h"
#include "node/m_node_controls.h"
#include "node/m_node_cinematic.h"
#include "node/m_node_container.h"
#include "node/m_node_custombutton.h"
#include "node/m_node_image.h"
#include "node/m_node_item.h"
#include "node/m_node_linestrip.h"
#include "node/m_node_map.h"
#include "node/m_node_airfightmap.h"
#include "node/m_node_model.h"
#include "node/m_node_radar.h"
#include "node/m_node_selectbox.h"
#include "node/m_node_string.h"
#include "node/m_node_special.h"
#include "node/m_node_spinner.h"
#include "node/m_node_tab.h"
#include "node/m_node_tbar.h"
#include "node/m_node_text.h"
#include "node/m_node_textentry.h"
#include "node/m_node_vscrollbar.h"
#include "node/m_node_windowpanel.h"
#include "node/m_node_zone.h"

extern menuNode_t *focusNode;

/**
 * @brief List of all node behaviours, indexes by nodetype num.
 */
static nodeBehaviour_t nodeBehaviourList[MN_NUM_NODETYPE];

/*
 * @brief Get a property from a behaviour or his inheritance
 * @param[in] node Requested node
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t *MN_GetPropertyFromBehaviour (const nodeBehaviour_t *behaviour, const char* name)
{
	for (; behaviour; behaviour = behaviour->super) {
		const value_t *result;
		if (behaviour->properties == NULL)
			continue;
		result = MN_FindPropertyByName(behaviour->properties, name);
		if (result)
			return result;
	}
	return NULL;
}

/**
 * @brief Get a property definition from a node
 * @param[in] node Requested node
 * @param[in] name Property name we search
 * @todo We can delete it
 */
const value_t *MN_NodeGetPropertyDefinition (const menuNode_t* node, const char* name)
{
	return MN_GetPropertyFromBehaviour(node->behaviour, name);
}

#if 0
/** @todo (menu) to be integrated into MN_CheckNodeZone */
/**
 * @brief Check if the node is an image and if it is transparent on the given (global) position.
 * @param[in] node A menunode pointer to be checked.
 * @param[in] x X position on screen.
 * @param[in] y Y position on screen.
 * @return qtrue if an image is used and it is on transparent on the current position.
 */
static qboolean MN_NodeWithVisibleImage (menuNode_t* const node, int x, int y)
{
	byte *picture = NULL;	/**< Pointer to image (4 bytes == 1 pixel) */
	int width, height;	/**< Width and height for the pic. */
	int pic_x, pic_y;	/**< Position inside image */
	byte *color = NULL;	/**< Pointer to specific pixel in image. */
	vec2_t nodepos;

	if (!node || node->type != MN_PIC || !node->dataImageOrModel)
		return qfalse;

	MN_GetNodeAbsPos(node, nodepos);
	R_LoadImage(va("pics/menu/%s", path), &picture, &width, &height);

	if (!picture || !width || !height) {
		Com_DPrintf(DEBUG_CLIENT, "Couldn't load image %s in pics/menu\n", path);
		/* We return true here because we do not know if there is another image (withouth transparency) or another reason it might still be valid. */
		return qtrue;
	}

	/** @todo (menu) Get current location _inside_ image from global position. CHECKME */
	pic_x = x - nodepos[0];
	pic_y = y - nodepos[1];

	if (pic_x < 0 || pic_y < 0)
		return qfalse;

	/* Get pixel at current location. */
	color = picture + (4 * height * pic_y + 4 * pic_x); /* 4 means 4 values for each point */

	/* Return qtrue if pixel is visible (we check the alpha value here). */
	if (color[3] != 0)
		return qtrue;

	/* Image is transparent at this position. */
	return qfalse;
}
#endif

/**
 * @sa MN_LeftClick
 */
qboolean MN_CheckNodeZone (menuNode_t* const node, int x, int y)
{
	int sx, sy, tx, ty, i;
	vec2_t nodepos;

	/* skip all unactive nodes */
	if (node->behaviour->isVirtual)
		return qfalse;

	/* don't hover nodes if we are executing an action on geoscape like rotating or moving */
	if (mouseSpace >= MS_ROTATE && mouseSpace <= MS_SHIFT3DMAP)
		return qfalse;

	if (focusNode && mouseSpace != MS_NULL)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (node->invis || !MN_CheckCondition(node))
		return qfalse;

	MN_GetNodeAbsPos(node, nodepos);

	/* check for click action */
	if (!node->behaviour->leftClick && !node->behaviour->rightClick && !node->behaviour->middleClick && !node->behaviour->mouseWheel && !node->onClick && !node->onRightClick && !node->onMiddleClick && !node->onWheel && !node->onMouseIn && !node->onMouseOut && !node->onWheelUp && !node->onWheelDown)
		return qfalse;

	if (node->size[0] == 0 || node->size[1] == 0)
		return qfalse;

	sx = node->size[0];
	sy = node->size[1];
	tx = x - nodepos[0];
	ty = y - nodepos[1];

	if (node->align > 0 && node->align < ALIGN_LAST) {
		switch (node->align % 3) {
		/* center */
		case 1:
			tx = x - nodepos[0] + sx / 2;
			break;
		/* right */
		case 2:
			tx = x - nodepos[0] + sx;
			break;
		}
	}

	if (tx < 0 || ty < 0 || tx > sx || ty > sy)
		return qfalse;

	for (i = 0; i < node->excludeRectNum; i++) {
		if (x >= node->menu->pos[0] + node->excludeRect[i].pos[0] && x <= node->menu->pos[0] + node->excludeRect[i].pos[0] + node->excludeRect[i].size[0]
		 && y >= node->menu->pos[1] + node->excludeRect[i].pos[1] && y <= node->menu->pos[1] + node->excludeRect[i].pos[1] + node->excludeRect[i].size[1])
			return qfalse;
	}

	return qtrue;
}

/**
 * @todo Change the @c type to @c char* (behaviourName)
 */
menuNode_t* MN_AllocNode (const char* type)
{
	menuNode_t* node = &mn.menuNodes[mn.numNodes++];
	if (mn.numNodes >= MAX_MENUNODES)
		Sys_Error("MAX_MENUNODES hit");
	memset(node, 0, sizeof(*node));
	node->behaviour = MN_GetNodeBehaviour(type);
	return node;
}

/**
 * @brief Return a node behaviour by name
 * @note Use a dicotomic search. nodeBehaviourList must be sorted by name.
 */
nodeBehaviour_t* MN_GetNodeBehaviour (const char* name)
{
	unsigned char min = 0;
	unsigned char max = MN_NUM_NODETYPE;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const char diff = Q_strcmp(nodeBehaviourList[mid].name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return &nodeBehaviourList[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	Sys_Error("Node behaviour '%s' doesn't exist\n", name);
	return NULL;
}


/**
 * @brief Clone a node
 * @param[in] node to clone
 * @param[in] recursive True if we also must clone subnodeas
 * @param[in] newMenu Menu where the nodes must be add (this function only link node into menu, note menu into the new node)
 * @todo remember to update this function when we allow a tree of nodes
 */
menuNode_t* MN_CloneNode (const menuNode_t* node, menu_t *newMenu, qboolean recursive)
{
	menuNode_t* newNode = MN_AllocNode(node->behaviour->name);
	*newNode = *node;
	newNode->menu = newMenu;
	newNode->next = NULL;
	return newNode;
}

/* position of virtual function into node behaviour */
static const int virtualFunctions[] = {
	offsetof(nodeBehaviour_t, draw),
	offsetof(nodeBehaviour_t, drawTooltip),
	offsetof(nodeBehaviour_t, leftClick),
	offsetof(nodeBehaviour_t, rightClick),
	offsetof(nodeBehaviour_t, middleClick),
	offsetof(nodeBehaviour_t, mouseWheel),
	offsetof(nodeBehaviour_t, mouseMove),
	offsetof(nodeBehaviour_t, mouseDown),
	offsetof(nodeBehaviour_t, mouseUp),
	offsetof(nodeBehaviour_t, capturedMouseMove),
	offsetof(nodeBehaviour_t, loading),
	offsetof(nodeBehaviour_t, loaded),
	offsetof(nodeBehaviour_t, dndEnter),
	offsetof(nodeBehaviour_t, dndMove),
	offsetof(nodeBehaviour_t, dndLeave),
	offsetof(nodeBehaviour_t, dndDrop),
	offsetof(nodeBehaviour_t, dndFinished),
	0
};

static void MN_InitializeNodeBehaviour (nodeBehaviour_t* behaviour)
{
	if (behaviour->isInitialized)
		return;

	/** @todo check (when its possible) properties are ordered by name */
	/* check and update properties data */
	if (behaviour->properties) {
		int num = 0;
		const value_t* current = behaviour->properties;
		while (current->string != NULL) {
			num++;
			current++;
		}
		behaviour->propertyCount = num;
	}

	/* everything inherite 'abstractnode' */
	if (behaviour->extends == NULL && Q_strcmp(behaviour->name, "abstractnode") != 0) {
		behaviour->extends = "abstractnode";
	}

	if (behaviour->extends) {
		int i = 0;
		behaviour->super = MN_GetNodeBehaviour(behaviour->extends);
		MN_InitializeNodeBehaviour(behaviour->super);

		while (qtrue) {
			const int pos = virtualFunctions[i];
			size_t superFunc;
			size_t func;
			if (pos == 0)
				break;

			/* cache super function if we dont overwrite it */
			superFunc = *(size_t*)((char*)behaviour->super + pos);
			func = *(size_t*)((char*)behaviour + pos);
			if (func == 0 && superFunc != 0) {
				*(size_t*)((char*)behaviour + pos) = superFunc;
			}

			i++;
		}
	}

	behaviour->isInitialized = qtrue;
}

void MN_InitNodes (void)
{
	int i = 0;
	nodeBehaviour_t *current = nodeBehaviourList;

	/** @todo convert it with an array of function */
	/* compute list of node behaviours */
	MN_RegisterNullNode(current++);
	MN_RegisterAbstractNode(current++);
	MN_RegisterAbstractOptionNode(current++);
	MN_RegisterAbstractScrollbarNode(current++);
	MN_RegisterAbstractValueNode(current++);
	MN_RegisterAirfightMapNode(current++);
	MN_RegisterBarNode(current++);
	MN_RegisterBaseLayoutNode(current++);
	MN_RegisterBaseMapNode(current++);
	MN_RegisterButtonNode(current++);
	MN_RegisterCheckBoxNode(current++);
	MN_RegisterCinematicNode(current++);
	MN_RegisterConFuncNode(current++);
	MN_RegisterContainerNode(current++);
	MN_RegisterControlsNode(current++);
	MN_RegisterCustomButtonNode(current++);
	MN_RegisterCvarFuncNode(current++);
	MN_RegisterFuncNode(current++);
	MN_RegisterItemNode(current++);
	MN_RegisterLineStripNode(current++);
	MN_RegisterMapNode(current++);
	MN_RegisterWindowNode(current++); /** @todo 'menu', rename it according to the function when its possible */
	MN_RegisterModelNode(current++);
	MN_RegisterImageNode(current++); /** @todo 'pic', rename it according to the function when its possible */
	MN_RegisterRadarNode(current++);
	MN_RegisterSelectBoxNode(current++);
	MN_RegisterSpinnerNode(current++);
	MN_RegisterStringNode(current++);
	MN_RegisterTabNode(current++);
	MN_RegisterTBarNode(current++);
	MN_RegisterTextNode(current++);
	MN_RegisterTextEntryNode(current++);
	MN_RegisterVScrollbarNode(current++);
	MN_RegisterWindowPanelNode(current++);
	MN_RegisterZoneNode(current++);

	/* check for safe data: list must be sorted by alphabet */
	current = nodeBehaviourList;
	for (i = 0; i < MN_NUM_NODETYPE - 1; i++) {
		const nodeBehaviour_t *a = current;
		const nodeBehaviour_t *b = current + 1;
		if (Q_strcmp(a->name, b->name) >= 0) {
#ifdef DEBUG
			Sys_Error("MN_InitNodes: '%s' is before '%s'. Please order node behaviour registrations by name\n", a->name, b->name);
#else
			Sys_Error("MN_InitNodes: Error: '%s' is before '%s'\n", a->name, b->name);
#endif
		}
		current++;
	}

	/* finalyse node behaviour initialization */
	current = nodeBehaviourList;
	for (i = 0; i < MN_NUM_NODETYPE; i++) {
		MN_InitializeNodeBehaviour(current);
		current++;
	}
}
