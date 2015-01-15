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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_behaviour.h"
#include "ui_parse.h"

#include "../../common/hashtable.h"
#include "../../common/scripts.h"
#include "../../common/scripts_lua.h"
#include "../../common/swig_lua_runtime.h"

/**
 * Size of the temporary property-list allocation (per behaviour)
 */
#define LOCAL_PROPERTY_SIZE	128


/**
 * @brief Register a property to a behaviour.
 * It should not be used in the code
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param type Type of the property
 * @param pos position of the attribute (which store property memory) into the node structure
 * @param size size of the attribute (which store property memory) into the node structure
 * @see UI_RegisterNodeProperty
 * @see UI_RegisterExtradataNodeProperty
 * @return A link to the node property
 */
const struct value_s* UI_RegisterNodePropertyPosSize_ (uiBehaviour_t* behaviour, const char* name, int type, size_t pos, size_t size)
{
	value_t* property = (value_t*) UI_AllocHunkMemory(sizeof(value_t), STRUCT_MEMORY_ALIGN, false);
	if (property == nullptr)
		Com_Error(ERR_FATAL, "UI_RegisterNodePropertyPosSize_: UI memory hunk exceeded - increase the size");

	if (type == V_STRING || type == V_LONGSTRING || type == V_CVAR_OR_LONGSTRING || V_CVAR_OR_STRING) {
		size = 0;
	}

	property->string = name;
	property->type = (valueTypes_t) type;
	property->ofs = pos;
	property->size = size;

	if (behaviour->localProperties == nullptr) {
		/* temporary memory allocation */
		behaviour->localProperties = Mem_PoolAllocTypeN(value_t const*, LOCAL_PROPERTY_SIZE, ui_sysPool);
	}
	if (behaviour->propertyCount >= LOCAL_PROPERTY_SIZE - 1) {
		Com_Error(ERR_FATAL, "UI_RegisterNodePropertyPosSize_: Property memory of behaviour %s is full.", behaviour->name);
	}
	behaviour->localProperties[behaviour->propertyCount++] = property;
	behaviour->localProperties[behaviour->propertyCount] = nullptr;

	return property;
}

/**
 * @brief Register a node method to a behaviour.
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param function function to execute the node method
 * @return A link to the node property
 */
const struct value_s* UI_RegisterNodeMethod (uiBehaviour_t* behaviour, const char* name, uiNodeMethod_t function)
{
	return UI_RegisterNodePropertyPosSize_(behaviour, name, V_UI_NODEMETHOD, (size_t)function, 0);
}

/**
 * @brief Get a property from a behaviour or his inheritance
 * It use a dichotomic search.
 * @param[in] behaviour Context behaviour
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else nullptr
 */
const value_t* UI_GetPropertyFromBehaviour (const uiBehaviour_t* behaviour, const char* name)
{
	for (; behaviour; behaviour = behaviour->super) {
		unsigned char min = 0;
		unsigned char max = behaviour->propertyCount;

		while (min != max) {
			const int mid = (min + max) >> 1;
			const int diff = Q_strcasecmp(behaviour->localProperties[mid]->string, name);
			assert(mid < max);
			assert(mid >= min);

			if (diff == 0)
				return behaviour->localProperties[mid];

			if (diff > 0)
				max = mid;
			else
				min = mid + 1;
		}
	}
	return nullptr;
}

/**
 * @brief Get a property or lua based method from a node, node behaviour or inherted behaviour
 * @param[in] node The node holding the method
 * @param[in] name Property name we search
 * @param[out] out A reference to a value_t structure wich is filled if a lua based method is available. Set to
 * nullptr to onlys scan for properties and not for lua based methods.
 * @return A value_t with the requested name, else nullptr
 * @note This function first searches the local properties of the behaviour before looking into lua based functions.
 * @note If a lua function is found, .type is set to V_UI_NODEMETHOD_LUA, .string is set to the method name and .ofs is
 * set to the lua callback id of the function.
 * @note After use, be sure to free the allocated .string value!!!
 */
const value_t* UI_GetPropertyOrLuaMethod (const uiNode_t* node, const char* name, value_t *out) {
	// first scan behaviour properties
	const value_t* prop = UI_GetPropertyFromBehaviour (node->behaviour, name);
	if (prop) return prop;
	// next scan lua based functions
	if (out) {
		LUA_FUNCTION fn;
		if (UI_GetNodeMethod(node, name, fn)) {
			out->type = (valueTypes_t)V_UI_NODEMETHOD_LUA;
			out->string = Mem_StrDup(name);
			out->ofs = fn;
			out->size = 0;
		}
	}
    // nothing found, report it
    return nullptr;
}

/**
 * @brief Initialize a node behaviour memory, after registration, and before using it.
 * @param behaviour Behaviour to initialize
 * @note This method sets the SWIG type for this behaviour for the runtime conversion of uiNode_t* values
 * to the correct subclass.
 */
void UI_InitializeNodeBehaviour (uiBehaviour_t* behaviour)
{
	if (behaviour->isInitialized)
		return;

	/* everything inherits 'abstractnode' */
	if (behaviour->extends == nullptr && !Q_streq(behaviour->name, "abstractnode")) {
		behaviour->extends = "abstractnode";
	}

	if (!behaviour->manager && Q_strvalid(behaviour->name)) {
		Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: Behaviour '%s' expect a manager class", behaviour->name);
	}

	if (behaviour->extends) {
		/** @todo Find a way to remove that, if possible */
		behaviour->super = UI_GetNodeBehaviour(behaviour->extends);
		UI_InitializeNodeBehaviour(behaviour->super);

		/* cache super function if we don't overwrite it */
		if (behaviour->extraDataSize == 0)
			behaviour->extraDataSize = behaviour->super->extraDataSize;
	}

	/* sort properties by alphabet */
	if (behaviour->localProperties) {
		const value_t** oldmemory = behaviour->localProperties;
		behaviour->localProperties = (const value_t**) UI_AllocHunkMemory(sizeof(value_t*) * (behaviour->propertyCount+1), STRUCT_MEMORY_ALIGN, false);
		if (behaviour->localProperties == nullptr) {
			Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: UI memory hunk exceeded - increase the size");
		}

		const value_t* previous = nullptr;
		for (int i = 0; i < behaviour->propertyCount; i++) {
			const value_t* better = nullptr;
			/* search the next element after previous */
			for (const value_t** current = oldmemory; *current != nullptr; current++) {
				if (previous != nullptr && Q_strcasecmp(previous->string, (*current)->string) >= 0) {
					continue;
				}
				if (better == nullptr || Q_strcasecmp(better->string, (*current)->string) >= 0) {
					better = *current;
				}
			}
			previous = better;
			behaviour->localProperties[i] = better;
		}
		behaviour->localProperties[behaviour->propertyCount] = nullptr;
		Mem_Free(oldmemory);
	}

	/* property must not overwrite another property */
	if (behaviour->super && behaviour->localProperties) {
		const value_t** property = behaviour->localProperties;
		while (*property) {
			const value_t* p = UI_GetPropertyFromBehaviour(behaviour->super, (*property)->string);
#if 0	/**< @todo not possible at the moment, not sure its the right way */
			const uiBehaviour_t* b = UI_GetNodeBehaviour(current->string);
#endif
			if (p != nullptr)
				Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: property '%s' from node behaviour '%s' overwrite another property", (*property)->string, behaviour->name);
#if 0	/**< @todo not possible at the moment, not sure its the right way */
			if (b != nullptr)
				Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: property '%s' from node behaviour '%s' use the name of an existing node behaviour", (*property)->string, behaviour->name);
#endif
			property++;
		}
	}

	/* Sanity: A property must not be outside the node memory */
	if (behaviour->localProperties) {
		const int size = sizeof(uiNode_t) + behaviour->extraDataSize;
		const value_t** property = behaviour->localProperties;
		while (*property) {
			if ((*property)->type != V_UI_NODEMETHOD && (*property)->ofs + (*property)->size > size)
				Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: property '%s' from node behaviour '%s' is outside the node memory. The C code need a fix.", (*property)->string, behaviour->name);
			property++;
		}
	}

	behaviour->isInitialized = true;
}

/**
 * @brief Adds a lua based method to the list of available behaviour methods for calling.
 * @param[in] behaviour The behaviour to extend.
 * @param[in] name The name of the new method to add
 * @param[in] fcn The lua based function reference.
 * @note If the method name is already defined, the new method is not added and a warning will be issued
 * in the log.
 */
void UI_AddBehaviourMethod (uiBehaviour_t* behaviour, const char* name, LUA_METHOD fcn) {
	Com_Printf ("UI_AddBehaviourMethod: registering class method [%s] on behaviour [%s]\n", name, behaviour->name);

	/* the first method, so create a method table on this node */
	if (!behaviour->nodeMethods) {
		behaviour->nodeMethods = HASH_NewTable(true, true, false);
	}
	/* add the method */
    if (!HASH_Insert(behaviour->nodeMethods, name, strlen(name), &fcn, sizeof(fcn))) {
		Com_Printf("UI_AddBehaviourMethod: method [%s] already defined on this behaviour [%s]\n", name, behaviour->name);
    }
}

/**
 * @brief Finds the lua based method on this behaviour or its super.
 * @param[in] behaviour The node behaviour to examine.
 * @param[in] name The name of the method to find
 * @param[out] fcn A reference to a LUA_METHOD value to the corresponding lua based function or to LUA_NOREF if
 * the method is not found
 * @return True if the method is found, false otherwise.
 */
bool UI_GetBehaviourMethod (const uiBehaviour_t* behaviour, const char* name, LUA_METHOD &fcn) {
	fcn = LUA_NOREF;
	for(;behaviour;behaviour = behaviour->super) {
		if (behaviour->nodeMethods) {
            void* val=HASH_Get(behaviour->nodeMethods, name, strlen(name));
			if (val != nullptr) {
				fcn = *((LUA_METHOD *)val);
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief Returns true if a node method of given name is available on this behaviour or its super.
 * @param[in] behaviour The node behaviour to examine.
 * @param[in] name The name of the method to find
 * @return True if the method is found, false otherwise.
 */
bool UI_HasBehaviourMethod (uiBehaviour_t* behaviour, const char* name) {
	LUA_METHOD fn;
	return UI_GetBehaviourMethod(behaviour, name, fn);
}

