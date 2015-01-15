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

#pragma once

#include "ui_node_abstractvalue.h"

class uiCheckBoxNode : public uiAbstractValueNode {
public:
	void draw(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void onActivate(uiNode_t* node) override;
	void toggle(uiNode_t* node);
};

struct checkboxExtraData_t {
	abstractValueExtraData_t super;

	/** Sprite used as an icon for checked state */
	uiSprite_t* iconChecked;
	/** Sprite used as an icon for unchecked state */
	uiSprite_t* iconUnchecked;
	/** Sprite used as an icon for unknown state */
	uiSprite_t* iconUnknown;
	/** Sprite used as a background */
	uiSprite_t* background;
};

void UI_RegisterCheckBoxNode(uiBehaviour_t* behaviour);
void UI_CheckBox_SetBackgroundByName (uiNode_t* node, const char* name);
void UI_CheckBox_SetIconCheckedByName (uiNode_t* node, const char* name);
void UI_CheckBox_SetIconUncheckedByName (uiNode_t* node, const char* name);
void UI_CheckBox_SetIconUnknownByName (uiNode_t* node, const char* name);
void UI_CheckBox_Toggle (uiNode_t* node);

bool UI_CheckBox_ValueAsBoolean (uiNode_t* node);
int UI_CheckBox_ValueAsInteger (uiNode_t* node);
