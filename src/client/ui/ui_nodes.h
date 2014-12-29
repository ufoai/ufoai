/**
 * @file
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

#include "../../shared/ufotypes.h"
#include "../../common/scripts.h"
#include "../../common/scripts_lua.h"

/* prototype */
struct uiSprite_t;
struct value_s;
struct nodeKeyBinding_s;
struct uiCallContext_s;
struct uiModel_s;
struct uiBehaviour_t;
struct uiLuaCallback_t;
struct hashTable_s;

typedef struct uiExcludeRect_s {
	/** position of the exclude rect relative to node position */
	vec2_t pos;
	/** size of the exclude rect */
	vec2_t size;
	/** next exclude rect used by the node */
	struct uiExcludeRect_s* next;
} uiExcludeRect_t;

struct uiBox_t {
	vec2_t pos;
	vec2_t size;

	void clear() {
		Vector2Clear(pos);
		Vector2Clear(size);
	}

	void set(vec2_t pos, vec2_t size) {
		Vector2Copy(pos, this->pos);
		Vector2Copy(size, this->size);
	}

	void expand(int dist) {
		pos[0] -= dist;
		pos[1] -= dist;
		size[0] += 2 * dist;
		size[1] += 2 * dist;
	}

	/**
	 * Align an inner box to this box.
	 */
	void alignBox(uiBox_t& inner, align_t direction);
};

/**
 * @brief Atomic structure used to define most of the UI
 */
struct uiNode_t {
	/* common identification */
	char name[MAX_VAR];			/**< name from the script files */
	uiBehaviour_t* behaviour;
	uiNode_t const* super;		/**< Node inherited, else nullptr */
	bool dynamic;				/** If true, it use dynamic memory */
	bool indexed;				/** If true, the node name indexed into his window */

	/* common navigation */
	uiNode_t* firstChild; 		/**< first element of linked list of child */
	uiNode_t* lastChild;  		/**< last element of linked list of child */
	uiNode_t* next;				/**< Next element into linked list */
	uiNode_t* parent;			/**< Parent window, else nullptr */
	uiNode_t* root;				/**< Shortcut to the root node */

	/* common pos */
	uiBox_t box;

	/* common attributes */
	char* tooltip;				/**< holds the tooltip */
	struct uiKeyBinding_s* key;	/**< key bindings - used as tooltip */
	bool invis;					/**< true if the node is invisible */
	bool disabled;				/**< true if the node is inactive */
	vec4_t disabledColor;			/**< rgba The color to draw when the node is disabled. */
	bool invalidated;			/**< true if we need to update the layout */
	bool ghost;					/**< true if the node is not tangible */
	bool state;					/**< is node hovered */
	bool flash;					/**< is node flashing */
	float flashSpeed;			/**< speed of the flashing effect */
	int padding;				/**< padding for this node - default 3 - see bgcolor */
	int align;					/**< used to identify node position into a parent using a layout manager. Else it do nothing. */
	int num;					/**< used to identify child into a parent; not sure it is need @todo delete it */
	struct uiAction_s* visibilityCondition;	/**< cvar condition to display/hide the node */
	int deleteTime;				/**< delayed delete time */

	/** linked list of exclude rect, which exclude node zone for hover or click functions */
	uiExcludeRect_t* firstExcludeRect;

	/* other attributes */
	/** @todo needs cleanup */
	int contentAlign;			/**< Content alignment inside nodes */
	char* text;					/**< Text we want to display */
	char* font;					/**< Font to draw text */
	char* image;
	int border;					/**< border for this node - thickness in pixel - default 0 - also see bgcolor */
	vec4_t bgcolor;				/**< rgba */
	vec4_t bordercolor;			/**< rgba - see border and padding */
	vec4_t color;				/**< rgba */
	vec4_t selectedColor;		/**< rgba The color to draw the line specified by textLineSelected in. */
	vec4_t flashColor;			/**< rgbx The color of the flashing effect. */

	/* extended behaviour */
	hashTable_s* nodeMethods;		/**< hash map for storing lua defined node functions */

	/* common events */
	struct uiAction_s* onClick;
	struct uiAction_s* onRightClick;
	struct uiAction_s* onMiddleClick;
	struct uiAction_s* onWheel;
	struct uiAction_s* onMouseEnter;
	struct uiAction_s* onMouseLeave;
	struct uiAction_s* onWheelUp;
	struct uiAction_s* onWheelDown;
	struct uiAction_s* onChange;	/**< called when the widget change from an user action */

	/* common events lua based */
	/* note: if new events are added here, make sure the value is initialized to LUA_NOREF
	   @sa: UI_AllocNodeWithoutNew */
    LUA_EVENT lua_onClick; /**< references the event in lua: on_click (node, x, y) */
    LUA_EVENT lua_onRightClick; /**< references the event in lua: on_rightclick (node, x, y) */
    LUA_EVENT lua_onMiddleClick; /**< references the event in lua: on_middleclick (node, x, y) */
    LUA_EVENT lua_onWheelUp; /**< references the event in lua: on_wheelup (node, dx, dy) */
    LUA_EVENT lua_onWheelDown; /**< references the event in lua: on_wheeldown (node, dx, dy) */
    LUA_EVENT lua_onWheel; /**< references the event in lua: on_wheel (node, dx, dy) */
    LUA_EVENT lua_onFocusGained; /**< references the event in lua: on_focusgained (node) */
    LUA_EVENT lua_onFocusLost; /**< references the event in lua: on_focuslost (node) */
    LUA_EVENT lua_onKeyPressed; /**< references the event in lua: on_keypressed (node, key, unicode) */
    LUA_EVENT lua_onKeyReleased; /**< references the event in lua: on_keyreleased (node, key, unicode) */
    LUA_EVENT lua_onLoaded; /**< references the event in lua: on_loaded (node) */
    LUA_EVENT lua_onActivate; /**< references the event in lua: on_activate (node) */
    LUA_EVENT lua_onMouseEnter; /**< references the event in lua: on_mouseenter (node) */
    LUA_EVENT lua_onMouseLeave; /**< references the event in lua: on_mouseleave (node) */
    LUA_EVENT lua_onChange; /**< references the event in lua: on_change (node) */
    LUA_EVENT lua_onVisibleWhen; /**< references the event in lua: on_visible (node) */

	bool dragdrop; /**< set to true to enable dragdrop on this node */
	/** Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
    LUA_EVENT lua_onDragDropEnter; /**< references the event in lua: on_dragdropenter (node) */
	/** Send to the target when the DND is canceled */
    LUA_EVENT lua_onDragDropLeave; /**< references the event in lua: on_dragdropleave (node) */
	/** Send to the target when we enter first, return true if we can drop the DND here */
	LUA_EVENT lua_onDragDropMove; /**< references the event in lua: on_dragdropmove (node, x, y) */
	/** Send to the target to finalize the drop */
	LUA_EVENT lua_onDragDropDrop; /**< references the event in lua: on_dragdropdrop (node, x, y) */
	/** Sent to the source to finalize the drop */
	LUA_EVENT lua_onDragDropFinished; /**< references the event in lua: on_dragdropfinished (node, isdropped) */
};


/**
 * @brief Return extradata structure from a node
 * @param TYPE Extradata type
 * @param NODE Pointer to the node
 */
#define UI_EXTRADATA_POINTER(NODE, TYPE) ((TYPE*)((char*)NODE + sizeof(uiNode_t)))
#define UI_EXTRADATA(NODE, TYPE) (*UI_EXTRADATA_POINTER(NODE, TYPE))
#define UI_EXTRADATACONST_POINTER(NODE, TYPE) ((TYPE*)((const char*)NODE + sizeof(uiNode_t)))
#define UI_EXTRADATACONST(NODE, TYPE) (*UI_EXTRADATACONST_POINTER(NODE, const TYPE))

/* module initialization */
void UI_InitNodes(void);

/* nodes */
uiNode_t* UI_AllocNode(const char* name, const char* type, bool isDynamic);
uiNode_t* UI_GetNodeByPath(const char* path) __attribute__ ((warn_unused_result));
void UI_ReadNodePath(const char* path, const uiNode_t* relativeNode, const uiNode_t* iterationNode, uiNode_t** resultNode, const value_t** resultProperty, value_t* luaMethod = nullptr);
uiNode_t* UI_GetNodeAtPosition(int x, int y) __attribute__ ((warn_unused_result));
const char* UI_GetPath(const uiNode_t* node) __attribute__ ((warn_unused_result));
uiNode_t* UI_CloneNode(const uiNode_t* node, uiNode_t* newWindow, bool recursive, const char* newName, bool isDynamic) __attribute__ ((warn_unused_result));
bool UI_CheckVisibility(uiNode_t* node);
void UI_DeleteAllChild(uiNode_t* node);
void UI_DeleteNode(uiNode_t* node);

/* behaviours */
/* @todo move it to main */
uiBehaviour_t* UI_GetNodeBehaviour(const char* name) __attribute__ ((warn_unused_result));
uiBehaviour_t* UI_GetNodeBehaviourByIndex(int index) __attribute__ ((warn_unused_result));
int UI_GetNodeBehaviourCount(void) __attribute__ ((warn_unused_result));

