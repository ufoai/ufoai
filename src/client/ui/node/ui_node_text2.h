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

#include "ui_node_abstractscrollable.h"
#include "ui_node_text.h"

class uiText2Node : public uiTextNode {
	void draw(uiNode_t* node) override;
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void onRightClick(uiNode_t* node, int x, int y) override;
	void onMouseMove(uiNode_t* node, int x, int y) override;
	void onLoading(uiNode_t* node) override;
	void onLoaded(uiNode_t* node) override;
	void updateCache (uiNode_t* node) override;
protected:
	void drawText (uiNode_t* node, const linkedList_t* list, bool noDraw);
};

struct uiBehaviour_t;
struct uiAction_s;

void UI_RegisterText2Node(uiBehaviour_t* behaviour);

typedef struct {
	textExtraData_t super;

	linkedList_t* lineSplit;

} text2ExtraData_t;

/**
 * @note super must be at 0 else inherited function will crash.
 */
CASSERT(offsetof(text2ExtraData_t, super) == 0);
