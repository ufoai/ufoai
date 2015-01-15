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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_sprite.h"
#include "ui_node_abstractnode.h"
#include "ui_node_option.h"

#include "../../client.h" /* gettext _() */

#include "../../../common/scripts_lua.h"

/**
 * Allow to check if a node is an option without string check
 */
const uiBehaviour_t* ui_optionBehaviour = nullptr;

#define EXTRADATA_TYPE optionExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const value_t* propertyCollapsed;


/**
 * @brief update option cache about child, according to collapse and visible status
 * @note can be a common function for all option node
 * @return number of visible elements
 */
int UI_OptionUpdateCache (uiNode_t* option)
{
	int count = 0;
	while (option) {
		int localCount = 0;
		assert(option->behaviour == ui_optionBehaviour);
		if (option->invis) {
			option = option->next;
			continue;
		}
		if (OPTIONEXTRADATA(option).collapsed) {
			OPTIONEXTRADATA(option).childCount = 0;
			option = option->next;
			count++;
			continue;
		}
		if (option->firstChild)
			localCount = UI_OptionUpdateCache(option->firstChild);
		OPTIONEXTRADATA(option).childCount = localCount;
		count += 1 + localCount;
		option = option->next;
	}
	return count;
}

void uiOptionNode::doLayout (uiNode_t* node)
{
	uiNode_t* child = node->firstChild;
	int count = 0;

	while (child && child->behaviour == ui_optionBehaviour) {
		UI_Validate(child);
		if (!child->invis) {
			if (EXTRADATA(child).collapsed)
				count += 1 + EXTRADATA(child).childCount;
			else
				count += 1;
		}
		child = child->next;
	}
	EXTRADATA(node).childCount = count;
	node->invalidated = false;
}

void uiOptionNode::onPropertyChanged (uiNode_t* node, const value_t* property)
{
	if (property == propertyCollapsed) {
		UI_Invalidate(node);
		return;
	}
	uiLocatedNode::onPropertyChanged(node, property);
}

/**
 * @brief Initializes an option with a very little set of values.
 * @param[in] option Context option
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 */
static void UI_InitOption (uiNode_t* option, const char* label, const char* value)
{
	assert(option);
	assert(option->behaviour == ui_optionBehaviour);
	Q_strncpyz(OPTIONEXTRADATA(option).label, label, sizeof(OPTIONEXTRADATA(option).label));
	Q_strncpyz(OPTIONEXTRADATA(option).value, value, sizeof(OPTIONEXTRADATA(option).value));
}

/**
 * @brief Initializes an option with a very little set of values.
 * @param[in] name The name of the new node
 * @param[in] label label displayed
 * @param[in] value value used when this option is selected
 */
uiNode_t* UI_AllocOptionNode (const char* name, const char* label, const char* value)
{
	uiNode_t* option;
	option = UI_AllocNode(name, "option", true);
	UI_InitOption(option, label, value);
	return option;
}

void UI_Option_SetLabel (uiNode_t* node, const char* text) {
	Q_strncpyz(OPTIONEXTRADATA(node).label, text, sizeof(OPTIONEXTRADATA(node).label));
}

void UI_Option_SetValue (uiNode_t* node, const char* text) {
	Q_strncpyz(OPTIONEXTRADATA(node).value, text, sizeof(OPTIONEXTRADATA(node).value));
}

void UI_Option_SetIconByName (uiNode_t* node, const char* name) {
	uiSprite_t* sprite = UI_GetSpriteByName(name);
	OPTIONEXTRADATA(node).icon = sprite;
}

void UI_RegisterOptionNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "option";
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->manager = UINodePtr(new uiOptionNode());
	behaviour->lua_SWIG_typeinfo = UI_SWIG_TypeQuery("uiOptionNode_t *");

	/**
	 * Displayed text
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "label", V_STRING, EXTRADATA_TYPE, label);

	/**
	 * Value of the option
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "value", V_STRING, EXTRADATA_TYPE, value);

	/**
	 * If true, child are not displayed
	 */
	propertyCollapsed = UI_RegisterExtradataNodeProperty(behaviour, "collapsed", V_BOOL, EXTRADATA_TYPE, collapsed);

	/* Icon used to display the node
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "icon", V_UI_SPRITEREF, EXTRADATA_TYPE, icon);
	UI_RegisterExtradataNodeProperty(behaviour, "flipicon", V_BOOL, EXTRADATA_TYPE, flipIcon);

	ui_optionBehaviour = behaviour;
}
