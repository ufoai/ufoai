/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#include "inventory.h"

void InventoryInterface::removeInvList (invList_t *invList)
{
	Com_DPrintf(DEBUG_SHARED, "I_RemoveInvList: remove one slot (%s)\n", invName);

	/* first entry */
	if (this->invList == invList) {
		invList_t *ic = this->invList;
		this->invList = ic->next;
		free(ic);
	} else {
		invList_t *ic = this->invList;
		invList_t* prev = NULL;
		while (ic) {
			if (ic == invList) {
				if (prev)
					prev->next = ic->next;
				free(ic);
				break;
			}
			prev = ic;
			ic = ic->next;
		}
	}
}

invList_t* InventoryInterface::addInvList (invList_t **invList)
{
	invList_t *newEntry;
	invList_t *list;

	Com_DPrintf(DEBUG_SHARED, "I_AddInvList: add one slot (%s)\n", invName);

	/* create the list */
	if (!*invList) {
		*invList = (invList_t*)alloc(sizeof(**invList));
		(*invList)->next = NULL; /* not really needed - but for better readability */
		return *invList;
	} else
		list = *invList;

	while (list->next)
		list = list->next;

	newEntry = (invList_t*)alloc(sizeof(*newEntry));
	list->next = newEntry;
	newEntry->next = NULL; /* not really needed - but for better readability */

	return newEntry;
}

/**
 * @brief Add an item to a specified container in a given inventory.
 * @note Set x and y to NONE if the item should get added to an automatically chosen free spot in the container.
 * @param[in,out] inv Pointer to inventory definition, to which we will add item.
 * @param[in] item Item to add to given container (needs to have "rotated" tag already set/checked, this is NOT checked here!)
 * @param[in] container Container in given inventory definition, where the new item will be stored.
 * @param[in] x The x location in the container.
 * @param[in] y The x location in the container.
 * @param[in] amount How many items of this type should be added. (this will overwrite the amount as defined in "item.amount")
 * @sa I_RemoveFromInventory
 * @return the @c invList_t pointer the item was added to, or @c NULL in case of an error (item wasn't added)
 */
invList_t *InventoryInterface::AddToInventory (inventory_t *const inv, const item_t* const item, const invDef_t *container, int x, int y, int amount)
{
	invList_t *ic;
	int checkedTo;

	if (!item->item)
		return NULL;

	if (amount <= 0)
		return NULL;

	assert(inv);
	assert(container);

	if (container->single && inv->c[container->id])
		return NULL;

	/* idEquip and idFloor */
	if (container->temp) {
		for (ic = inv->c[container->id]; ic; ic = ic->next)
			if (INVSH_CompareItem(&ic->item, item)) {
				ic->item.amount += amount;
				Com_DPrintf(DEBUG_SHARED, "I_AddToInventory: Amount of '%s': %i (%s)\n",
					ic->item.item->name, ic->item.amount, invName);
				return ic;
			}
	}

	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT) {
		/* No (sane) position in container given as parameter - find free space on our own. */
		INVSH_FindSpace(inv, item, container, &x, &y, NULL);
		if (x == NONE)
			return NULL;
	}

	checkedTo = INVSH_CheckToInventory(inv, item->item, container, x, y, NULL);
	assert(checkedTo);

	/* not found - add a new one */
	ic = addInvList(&inv->c[container->id]);

	/* Set the data in the new entry to the data we got via function-parameters.*/
	ic->item = *item;
	ic->item.amount = amount;

	/* don't reset an already applied rotation */
	if (checkedTo == INV_FITS_ONLY_ROTATED)
		ic->item.rotated = true;
	ic->x = x;
	ic->y = y;

	return ic;
}

/**
 * @param[in] i The inventory the container is in.
 * @param[in] container The container where the item should be removed.
 * @param[in] fItem The item to be removed.
 * @return true If removal was successful.
 * @return false If nothing was removed or an error occurred.
 * @sa I_AddToInventory
 */
bool InventoryInterface::RemoveFromInventory (inventory_t* const i, const invDef_t *container, invList_t *fItem)
{
	invList_t *ic, *previous;

	assert(i);
	assert(container);
	assert(fItem);

	ic = i->c[container->id];
	if (!ic)
		return false;

	/** @todo the problem here is, that in case of a move inside the same container
	 * the item don't just get updated x and y values but it is tried to remove
	 * one of the items => crap - maybe we have to change the inventory move function
	 * to check for this case of move and only update the x and y coordinates instead
	 * of calling the add and remove functions */
	if (container->single || ic == fItem) {
		this->cacheItem = ic->item;
		/* temp container like idEquip and idFloor */
		if (container->temp && ic->item.amount > 1) {
			ic->item.amount--;
			Com_DPrintf(DEBUG_SHARED, "I_RemoveFromInventory: Amount of '%s': %i (%s)\n",
				ic->item.item->name, ic->item.amount, invName);
			return true;
		}

		if (container->single && ic->next)
			Com_Printf("I_RemoveFromInventory: Error: single container %s has many items. (%s)\n", container->name, invName);

		/* An item in other containers than idFloor or idEquip should
		 * always have an amount value of 1.
		 * The other container types do not support stacking.*/
		assert(ic->item.amount == 1);

		i->c[container->id] = ic->next;

		/* updated invUnused to be able to reuse this space later again */
		removeInvList(ic);

		return true;
	}

	for (previous = i->c[container->id]; ic; ic = ic->next) {
		if (ic == fItem) {
			this->cacheItem = ic->item;
			/* temp container like idEquip and idFloor */
			if (ic->item.amount > 1 && container->temp) {
				ic->item.amount--;
				Com_DPrintf(DEBUG_SHARED, "I_RemoveFromInventory: Amount of '%s': %i (%s)\n",
					ic->item.item->name, ic->item.amount, invName);
				return true;
			}

			if (ic == i->c[container->id])
				i->c[container->id] = i->c[container->id]->next;
			else
				previous->next = ic->next;

			removeInvList(ic);

			return true;
		}
		previous = ic;
	}
	return false;
}

/**
 * @brief Conditions for moving items between containers.
 * @param[in] inv Inventory to move in.
 * @param[in] from Source container.
 * @param[in] fItem The item to be moved.
 * @param[in] to Destination container.
 * @param[in] tx X coordinate in destination container.
 * @param[in] ty Y coordinate in destination container.
 * @param[in,out] TU pointer to entity available TU at this moment
 * or @c NULL if TU doesn't matter (outside battlescape)
 * @param[out] icp
 * @return IA_NOTIME when not enough TU.
 * @return IA_NONE if no action possible.
 * @return IA_NORELOAD if you cannot reload a weapon.
 * @return IA_RELOAD_SWAP in case of exchange of ammo in a weapon.
 * @return IA_RELOAD when reloading.
 * @return IA_ARMOUR when placing an armour on the actor.
 * @return IA_MOVE when just moving an item.
 */
inventory_action_t InventoryInterface::MoveInInventory (inventory_t* const inv, const invDef_t *from, invList_t *fItem, const invDef_t *to, int tx, int ty, int *TU, invList_t ** icp)
{
	invList_t *ic;

	int time;
	int checkedTo = INV_DOES_NOT_FIT;
	bool alreadyRemovedSource = false;

	assert(to);
	assert(from);

	if (icp)
		*icp = NULL;

	if (from == to && fItem->x == tx && fItem->y == ty)
		return IA_NONE;

	time = from->out + to->in;
	if (from == to) {
		if (INV_IsFloorDef(from))
			time = 0;
		else
			time /= 2;
	}

	if (TU && *TU < time)
		return IA_NOTIME;

	assert(inv);

	/* Special case for moving an item within the same container. */
	if (from == to) {
		/* Do nothing if we move inside a scroll container. */
		if (from->scroll)
			return IA_NONE;

		ic = inv->c[from->id];
		for (; ic; ic = ic->next) {
			if (ic == fItem) {
				if (ic->item.amount > 1) {
					checkedTo = INVSH_CheckToInventory(inv, ic->item.item, to, tx, ty, fItem);
					if (checkedTo & INV_FITS) {
						ic->x = tx;
						ic->y = ty;
						if (icp)
							*icp = ic;
						return IA_MOVE;
					}
					return IA_NONE;
				}
			}
		}
	}

	/* If weapon is twohanded and is moved from hand to hand do nothing. */
	/* Twohanded weapon are only in CSI->idRight. */
	if (fItem->item.item->fireTwoHanded && INV_IsLeftDef(to) && INV_IsRightDef(from)) {
		return IA_NONE;
	}

	/* If non-armour moved to an armour slot then abort.
	 * Same for non extension items when moved to an extension slot. */
	if ((to->armour && !INV_IsArmour(fItem->item.item))
	 || (to->extension && !fItem->item.item->extension)
	 || (to->headgear && !fItem->item.item->headgear)) {
		return IA_NONE;
	}

	/* Check if the target is a blocked inv-armour and source!=dest. */
	if (to->single)
		checkedTo = INVSH_CheckToInventory(inv, fItem->item.item, to, 0, 0, fItem);
	else {
		if (tx == NONE || ty == NONE)
			INVSH_FindSpace(inv, &fItem->item, to, &tx, &ty, fItem);
		/* still no valid location found */
		if (tx == NONE || ty == NONE)
			return IA_NONE;

		checkedTo = INVSH_CheckToInventory(inv, fItem->item.item, to, tx, ty, fItem);
	}

	if (to->armour && from != to && !checkedTo) {
		item_t cacheItem2;
		invList_t *icTo;
		/* Store x/y origin coordinates of removed (source) item.
		 * When we re-add it we can use this. */
		const int cacheFromX = fItem->x;
		const int cacheFromY = fItem->y;

		/* Check if destination/blocking item is the same as source/from item.
		 * In that case the move is not needed -> abort. */
		icTo = INVSH_SearchInInventory(inv, to, tx, ty);
		if (fItem->item.item == icTo->item.item)
			return IA_NONE;

		/* Actually remove the ammo from the 'from' container. */
		if (!RemoveFromInventory(inv, from, fItem))
			return IA_NONE;
		else
			/* Removal successful - store this info. */
			alreadyRemovedSource = true;

		cacheItem2 = this->cacheItem; /* Save/cache (source) item. The cacheItem is modified in I_MoveInInventory. */

		/* Move the destination item to the source. */
		MoveInInventory(inv, to, icTo, from, cacheFromX, cacheFromY, TU, icp);

		/* Reset the cached item (source) (It'll be move to container emptied by destination item later.) */
		this->cacheItem = cacheItem2;
		checkedTo = INVSH_CheckToInventory(inv, this->cacheItem.item, to, 0, 0, fItem);
	} else if (!checkedTo) {
		/* Get the target-invlist (e.g. a weapon). We don't need to check for
		 * scroll because checkedTo is always true here. */
		ic = INVSH_SearchInInventory(inv, to, tx, ty);

		if (ic && !INV_IsEquipDef(to) && INVSH_LoadableInWeapon(fItem->item.item, ic->item.item)) {
			/* A target-item was found and the dragged item (implicitly ammo)
			 * can be loaded in it (implicitly weapon). */
			if (ic->item.ammoLeft >= ic->item.item->ammo && ic->item.ammo == fItem->item.item) {
				/* Weapon already fully loaded with the same ammunition -> abort */
				return IA_NORELOAD;
			}
			time += ic->item.item->reload;
			if (!TU || *TU >= time) {
				if (TU)
					*TU -= time;
				if (ic->item.ammoLeft >= ic->item.item->ammo) {
					/* exchange ammo */
					const item_t item = {NONE_AMMO, NULL, ic->item.ammo, 0, 0};
					/* Put current ammo in place of the new ammo unless floor - there can be more than 1 item */
					const int cacheFromX = INV_IsFloorDef(from) ? NONE : fItem->x;
					const int cacheFromY = INV_IsFloorDef(from) ? NONE : fItem->y;

					/* Actually remove the ammo from the 'from' container. */
					if (!RemoveFromInventory(inv, from, fItem))
						return IA_NONE;

					/* Add the currently used ammo in place of the new ammo in the "from" container. */
					if (AddToInventory(inv, &item, from, cacheFromX, cacheFromY, 1) == NULL)
						Sys_Error("Could not reload the weapon - add to inventory failed (%s)", invName);

					ic->item.ammo = this->cacheItem.item;
					if (icp)
						*icp = ic;
					return IA_RELOAD_SWAP;
				} else {
					/* Actually remove the ammo from the 'from' container. */
					if (!RemoveFromInventory(inv, from, fItem))
						return IA_NONE;

					ic->item.ammo = this->cacheItem.item;
					/* loose ammo of type ic->item.m saved on server side */
					ic->item.ammoLeft = ic->item.item->ammo;
					if (icp)
						*icp = ic;
					return IA_RELOAD;
				}
			}
			/* Not enough time -> abort. */
			return IA_NOTIME;
		}

		/* temp container like idEquip and idFloor */
		if (ic && to->temp) {
			/* We are moving to a blocked location container but it's the base-equipment floor or a battlescape floor.
			 * We add the item anyway but it'll not be displayed (yet)
			 * This is then used in I_AddToInventory below.*/
			/** @todo change the other code to browse trough these things. */
			INVSH_FindSpace(inv, &fItem->item, to, &tx, &ty, fItem);
			if (tx == NONE || ty == NONE) {
				Com_DPrintf(DEBUG_SHARED, "I_MoveInInventory - item will be added non-visible (%s)\n", invName);
			}
		} else {
			/* Impossible move -> abort. */
			return IA_NONE;
		}
	}

	/* twohanded exception - only CSI->idRight is allowed for fireTwoHanded weapons */
	if (fItem->item.item->fireTwoHanded && INV_IsLeftDef(to))
		to = &this->csi->ids[this->csi->idRight];

	switch (checkedTo) {
	case INV_DOES_NOT_FIT:
		/* Impossible move - should be handled above, but add an abort just in case */
		Com_Printf("I_MoveInInventory: Item doesn't fit into container.");
		return IA_NONE;
	case INV_FITS:
		/* Remove rotated tag */
		fItem->item.rotated = false;
		break;
	case INV_FITS_ONLY_ROTATED:
		/* Set rotated tag */
		fItem->item.rotated = true;
		break;
	case INV_FITS_BOTH:
		/* Leave rotated tag as-is */
		break;
	}

	/* Actually remove the item from the 'from' container (if it wasn't already removed). */
	if (!alreadyRemovedSource)
		if (!RemoveFromInventory(inv, from, fItem))
			return IA_NONE;

	/* successful */
	if (TU)
		*TU -= time;

	assert(this->cacheItem.item);
	ic = AddToInventory(inv, &this->cacheItem, to, tx, ty, 1);

	/* return data */
	if (icp) {
		assert(ic);
		*icp = ic;
	}

	if (INV_IsArmourDef(to)) {
		assert(INV_IsArmour(this->cacheItem.item));
		return IA_ARMOUR;
	}

	return IA_MOVE;
}

/**
 * @brief Tries to add an item to a container (in the inventory inv).
 * @param[in] inv Inventory pointer to add the item.
 * @param[in] item Item to add to inventory.
 * @param[in] container Container id.
 * @sa INVSH_FindSpace
 * @sa I_AddToInventory
 */
bool InventoryInterface::TryAddToInventory (inventory_t* const inv, const item_t *const item, const invDef_t *container)
{
	int x, y;

	INVSH_FindSpace(inv, item, container, &x, &y, NULL);

	if (x == NONE) {
		assert(y == NONE);
		return false;
	} else {
		const int checkedTo = INVSH_CheckToInventory(inv, item->item, container, x, y, NULL);
		if (!checkedTo)
			return false;

		const bool rotated = checkedTo == INV_FITS_ONLY_ROTATED;
		item_t itemRotation = *item;
		itemRotation.rotated = rotated;

		return AddToInventory(inv, &itemRotation, container, x, y, 1) != NULL;
	}
}

/**
 * @brief Clears the linked list of a container - removes all items from this container.
 * @param[in] i The inventory where the container is located.
 * @param[in] container Index of the container which will be cleared.
 * @sa I_DestroyInventory
 * @note This should only be called for temp containers if the container is really a temp container
 * e.g. the container of a dropped weapon in tactical mission (ET_ITEM)
 * in every other case just set the pointer to NULL for a temp container like idEquip or idFloor
 */
void InventoryInterface::EmptyContainer (inventory_t* const i, const invDef_t *container)
{
	invList_t *ic;

	ic = i->c[container->id];

	while (ic) {
		invList_t *old = ic;
		ic = ic->next;
		removeInvList(old);
	}

	i->c[container->id] = NULL;
}

/**
 * @brief Destroys inventory.
 * @param[in] inv Pointer to the inventory which should be erased.
 * @note Loops through all containers in inventory. @c NULL for temp containers are skipped,
 * for real containers @c I_EmptyContainer is called.
 * @sa I_EmptyContainer
 */
void InventoryInterface::DestroyInventory (inventory_t* const inv)
{
	containerIndex_t container;

	if (!inv)
		return;

	for (container = 0; container < this->csi->numIDs; container++) {
		const invDef_t *invDef = &this->csi->ids[container];
		if (!invDef->temp)
			EmptyContainer(inv, invDef);
	}

	OBJZERO(*inv);
}

/**
 * @brief Get the state of the given inventory: the items weight and the min TU needed to make full use of all the items.
 * @param[in] inventory The pointer to the inventory to evaluate.
 * @param[out] slowestFd The TU needed to use the slowest fireDef in the inventory.
 * @note temp containers are excluded.
 */
float InventoryInterface::GetInventoryState (const inventory_t *inventory, int &slowestFd)
{
	float weight = 0;

	slowestFd = 0;
	for (int containerID = 0; containerID < this->csi->numIDs; containerID++) {
		if (this->csi->ids[containerID].temp)
			continue;
		for (invList_t *ic = inventory->c[containerID], *next; ic; ic = next) {
			next = ic->next;
			weight += INVSH_GetItemWeight(ic->item);
			const fireDef_t *fireDef = FIRESH_SlowestFireDef(ic->item);
			if (slowestFd == 0 || (fireDef && fireDef->time > slowestFd))
					slowestFd = fireDef->time;
		}
	}
	return weight;
}

#define WEAPONLESS_BONUS	0.4		/* if you got neither primary nor secondary weapon, this is the chance to retry to get one (before trying to get grenades or blades) */

/**
 * @brief Pack a weapon, possibly with some ammo
 * @param[in] chr The character that will get the weapon
 * @param[in] weapon The weapon type index in gi.csi->ods
 * @param[in] ed The equipment for debug messages
 * @param[in] missedPrimary if actor didn't get primary weapon, this is 0-100 number to increase ammo number.
 * @param[in] maxWeight The max weight this actor is allowed to carry.
 * @sa INVSH_LoadableInWeapon
 */
int InventoryInterface::PackAmmoAndWeapon (character_t* const chr, const objDef_t* weapon, int missedPrimary, const equipDef_t *ed, int maxWeight)
{
	inventory_t* const inv = &chr->i;
	const int speed = chr->score.skills[ABILITY_SPEED];
	const objDef_t *ammo = NULL;
	item_t item = {NONE_AMMO, NULL, NULL, 0, 0};
	bool allowLeft;
	bool packed;
	int ammoMult = 1;
	int tuNeed = 0;
	int maxTU;
	float weight;

	assert(!INV_IsArmour(weapon));
	item.item = weapon;

	/* are we going to allow trying the left hand */
	allowLeft = !(inv->c[csi->idRight] && inv->c[csi->idRight]->item.item->fireTwoHanded);

	if (weapon->oneshot) {
		/* The weapon provides its own ammo (i.e. it is charged or loaded in the base.) */
		item.ammoLeft = weapon->ammo;
		item.ammo = weapon;
		Com_DPrintf(DEBUG_SHARED, "I_PackAmmoAndWeapon: oneshot weapon '%s' in equipment '%s' (%s).\n",
				weapon->id, ed->id, invName);
	} else if (!weapon->reload) {
		item.ammo = item.item; /* no ammo needed, so fire definitions are in t */
	} else {
		/* find some suitable ammo for the weapon (we will have at least one if there are ammos for this
		 * weapon in equipment definition) */
		int totalAvailableAmmo = 0;
		int i;
		for (i = 0; i < csi->numODs; i++) {
			const objDef_t *obj = INVSH_GetItemByIDX(i);
			if (ed->numItems[i] && INVSH_LoadableInWeapon(obj, weapon)) {
				totalAvailableAmmo++;
			}
		}
		if (totalAvailableAmmo) {
			int randNumber = rand() % totalAvailableAmmo;
			for (i = 0; i < csi->numODs; i++) {
				const objDef_t *obj = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && INVSH_LoadableInWeapon(obj, weapon)) {
					randNumber--;
					if (randNumber < 0) {
						ammo = obj;
						break;
					}
				}
			}
		}

		if (!ammo) {
			Com_DPrintf(DEBUG_SHARED, "I_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s' (%s).\n",
					weapon->id, ed->id, invName);
			return 0;
		}
		/* load ammo */
		item.ammoLeft = weapon->ammo;
		item.ammo = ammo;
	}

	if (!item.ammo) {
		Com_Printf("I_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s' (%s).\n",
				weapon->id, ed->id, invName);
		return 0;
	}

	weight = GetInventoryState(inv, tuNeed) + INVSH_GetItemWeight(item);
	maxTU = GET_TU(speed, GET_ENCUMBRANCE_PENALTY(weight, chr->score.skills[ABILITY_POWER]));
	if (weight > maxWeight || tuNeed > maxTU) {
		Com_DPrintf(DEBUG_SHARED, "I_PackAmmoAndWeapon: weapon too heavy: '%s' in equipment '%s' (%s).\n",
				weapon->id, ed->id, invName);
		return 0;
	}

	/* now try to pack the weapon */
	packed = TryAddToInventory(inv, &item, &csi->ids[csi->idRight]);
	if (packed)
		ammoMult = 3;
	if (!packed && allowLeft)
		packed = TryAddToInventory(inv, &item, &csi->ids[csi->idLeft]);
	if (!packed)
		packed = TryAddToInventory(inv, &item, &csi->ids[csi->idBelt]);
	if (!packed)
		packed = TryAddToInventory(inv, &item, &csi->ids[csi->idHolster]);
	if (!packed)
		return 0;

	/* pack some more ammo in the backpack */
	if (ammo) {
		int num;
		int numpacked = 0;

		/* how many clips? */
		num = (1 + ed->numItems[ammo->idx])
			* (float) (1.0f + missedPrimary / 100.0);

		/* pack some ammo */
		while (num--) {
			item_t mun = {NONE_AMMO, NULL, NULL, 0, 0};
			weight = GetInventoryState(inv, tuNeed) + INVSH_GetItemWeight(item);
			maxTU = GET_TU(speed, GET_ENCUMBRANCE_PENALTY(weight, chr->score.skills[ABILITY_POWER]));

			mun.item = ammo;
			/* ammo to backpack; belt is for knives and grenades */
			if (weight <= maxWeight && tuNeed <= maxTU)
					numpacked += TryAddToInventory(inv, &mun, &csi->ids[csi->idBackpack]);
			/* no problem if no space left; one ammo already loaded */
			if (numpacked > ammoMult || numpacked * weapon->ammo > 11)
				break;
		}
	}

	return true;
}


/**
 * @brief Equip melee actor with item defined per teamDefs.
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] td Pointer to a team definition.
 * @note Weapons assigned here cannot be collected in any case. These are dummy "actor weapons".
 */
void InventoryInterface::EquipActorMelee (inventory_t* const inv, const teamDef_t* td)
{
	const objDef_t *obj;
	item_t item;

	assert(td->onlyWeapon);

	/* Get weapon */
	obj = td->onlyWeapon;

	/* Prepare item. This kind of item has no ammo, fire definitions are in item.t. */
	item.item = obj;
	item.ammo = item.item;
	item.ammoLeft = NONE_AMMO;
	/* Every melee actor weapon definition is firetwohanded, add to right hand. */
	if (!obj->fireTwoHanded)
		Sys_Error("INVSH_EquipActorMelee: melee weapon %s for team %s is not firetwohanded! (%s)",
				obj->id, td->id, invName);
	TryAddToInventory(inv, &item, &this->csi->ids[this->csi->idRight]);
}

/**
 * @brief Equip robot actor with default weapon. (defined in ugv_t->weapon)
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] weapon Pointer to the item which being added to robot's inventory.
 */
void InventoryInterface::EquipActorRobot (inventory_t* const inv, const objDef_t* weapon)
{
	item_t item;

	assert(weapon);

	/* Prepare weapon in item. */
	item.item = weapon;
	item.ammoLeft = weapon->ammo;

	/* Get ammo for item/weapon. */
	assert(weapon->numAmmos > 0);	/* There _has_ to be at least one ammo-type. */
	assert(weapon->ammos[0]);
	item.ammo = weapon->ammos[0];

	TryAddToInventory(inv, &item, &this->csi->ids[this->csi->idRight]);
}

/**
 * @brief Types of weapon that can be selected
 */
typedef enum {
	WEAPON_PARTICLE_OR_NORMAL = 0,	/**< primary weapon is a particle or normal weapon */
	WEAPON_OTHER = 1,				/**< primary weapon is not a particle or normal weapon */
	WEAPON_NO_PRIMARY = 2			/**< no primary weapon */
} equipPrimaryWeaponType_t;

/**
 * @brief Fully equip one actor. The equipment that is added to the inventory of the given actor
 * is taken from the equipment script definition.
 * @param[in] chr The character that will get the weapon.
 * @param[in] ed The equipment that is added from to the actors inventory
 * @param[in] maxWeight The max weight this actor is allowed to carry.
 * @note The code below is a complete implementation
 * of the scheme sketched at the beginning of equipment_missions.ufo.
 * Beware: If two weapons in the same category have the same price,
 * only one will be considered for inventory.
 */
void InventoryInterface::EquipActor (character_t* const chr, const equipDef_t *ed, int maxWeight)
{
	inventory_t* const inv = &chr->i;
	const teamDef_t* td = chr->teamDef;
	const int speed = chr->score.skills[ABILITY_SPEED];
	int i;
	const int numEquip = lengthof(ed->numItems);
	int repeat = 0;

	if (td->weapons) {
		equipPrimaryWeaponType_t primary = WEAPON_NO_PRIMARY;
		int sum;
		int missedPrimary = 0; /**< If actor has a primary weapon, this is zero. Otherwise, this is the probability * 100
								* that the actor had to get a primary weapon (used to compensate the lack of primary weapon) */
		const objDef_t *primaryWeapon = NULL;
		int hasWeapon = 0;
		/* Primary weapons */
		const int maxWeaponIdx = std::min(this->csi->numODs - 1, numEquip - 1);
		int randNumber = rand() % 100;
		for (i = 0; i < maxWeaponIdx; i++) {
			const objDef_t *obj = INVSH_GetItemByIDX(i);
			if (ed->numItems[i] && obj->weapon && obj->fireTwoHanded && obj->isPrimary) {
				randNumber -= ed->numItems[i];
				missedPrimary += ed->numItems[i];
				if (!primaryWeapon && randNumber < 0)
					primaryWeapon = obj;
			}
		}
		/* See if a weapon has been selected. */
		if (primaryWeapon) {
			hasWeapon += PackAmmoAndWeapon(chr, primaryWeapon, 0, ed, maxWeight);
			if (hasWeapon) {
				int ammo;

				/* Find the first possible ammo to check damage type. */
				for (ammo = 0; ammo < this->csi->numODs; ammo++)
					if (ed->numItems[ammo] && INVSH_LoadableInWeapon(&this->csi->ods[ammo], primaryWeapon))
						break;
				if (ammo < this->csi->numODs) {
					if (/* To avoid two particle weapons. */
						!(this->csi->ods[ammo].dmgtype == this->csi->damParticle)
						/* To avoid SMG + Assault Rifle */
						&& !(this->csi->ods[ammo].dmgtype == this->csi->damNormal)) {
						primary = WEAPON_OTHER;
					} else {
						primary = WEAPON_PARTICLE_OR_NORMAL;
					}
				}
				/* reset missedPrimary: we got a primary weapon */
				missedPrimary = 0;
			} else {
				Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: primary weapon '%s' couldn't be equipped in equipment '%s' (%s).\n",
						primaryWeapon->id, ed->id, invName);
				repeat = WEAPONLESS_BONUS > frand();
			}
		}

		/* Sidearms (secondary weapons with reload). */
		do {
			int randNumber = rand() % 100;
			const objDef_t *secondaryWeapon = NULL;
			for (i = 0; i < this->csi->numODs; i++) {
				const objDef_t *obj = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && obj->weapon && obj->reload && !obj->deplete && obj->isSecondary) {
					randNumber -= ed->numItems[i] / (primary == WEAPON_PARTICLE_OR_NORMAL ? 2 : 1);
					if (randNumber < 0) {
						secondaryWeapon = obj;
						break;
					}
				}
			}

			if (secondaryWeapon) {
				hasWeapon += PackAmmoAndWeapon(chr, secondaryWeapon, missedPrimary, ed, maxWeight);
				if (hasWeapon) {
					const float AKIMBO_CHANCE = 0.3; 	/**< if you got a one-handed secondary weapon (and no primary weapon),
															 this is the chance to get another one (between 0 and 1) */
					/* Try to get the second akimbo pistol if no primary weapon. */
					if (primary == WEAPON_NO_PRIMARY && !secondaryWeapon->fireTwoHanded && frand() < AKIMBO_CHANCE) {
						PackAmmoAndWeapon(chr, secondaryWeapon, 0, ed, maxWeight);
					}
				}
			}
		} while (!hasWeapon && repeat--);

		/* Misc items and secondary weapons without reload. */
		if (!hasWeapon)
			repeat = WEAPONLESS_BONUS > frand();
		else
			repeat = 0;
		/* Misc object probability can be bigger than 100 -- you're sure to
		 * have one misc if it fits your backpack */
		sum = 0;
		for (i = 0; i < this->csi->numODs; i++) {
			const objDef_t *obj = INVSH_GetItemByIDX(i);
			if (ed->numItems[i] && ((obj->weapon && obj->isSecondary
			 && (!obj->reload || obj->deplete)) || obj->isMisc)) {
				/* if ed->num[i] is greater than 100, the first number is the number of items you'll get:
				 * don't take it into account for probability
				 * Make sure that the probability is at least one if an item can be selected */
				sum += ed->numItems[i] ? std::max(ed->numItems[i] % 100, 1) : 0;
			}
		}
		if (sum) {
			do {
				int randNumber = rand() % sum;
				const objDef_t *secondaryWeapon = NULL;
				for (i = 0; i < this->csi->numODs; i++) {
					const objDef_t *obj = INVSH_GetItemByIDX(i);
					if (ed->numItems[i] && ((obj->weapon && obj->isSecondary
					 && (!obj->reload || obj->deplete)) || obj->isMisc)) {
						randNumber -= ed->numItems[i] ? std::max(ed->numItems[i] % 100, 1) : 0;
						if (randNumber < 0) {
							secondaryWeapon = obj;
							break;
						}
					}
				}

				if (secondaryWeapon) {
					int num = ed->numItems[secondaryWeapon->idx] / 100 + (ed->numItems[secondaryWeapon->idx] % 100 >= 100 * frand());
					while (num--) {
						hasWeapon += PackAmmoAndWeapon(chr, secondaryWeapon, 0, ed, maxWeight);
					}
				}
			} while (repeat--); /* Gives more if no serious weapons. */
		}

		/* If no weapon at all, bad guys will always find a blade to wield. */
		if (!hasWeapon) {
			int maxPrice = 0;
			const objDef_t *blade = NULL;
			Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: no weapon picked in equipment '%s', defaulting to the most expensive secondary weapon without reload. (%s)\n",
					ed->id, invName);
			for (i = 0; i < this->csi->numODs; i++) {
				const objDef_t *obj = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && obj->weapon && obj->isSecondary && !obj->reload) {
					if (obj->price > maxPrice) {
						maxPrice = obj->price;
						blade = obj;
					}
				}
			}
			if (maxPrice)
				hasWeapon += PackAmmoAndWeapon(chr, blade, 0, ed, maxWeight);
		}
		/* If still no weapon, something is broken, or no blades in equipment. */
		if (!hasWeapon)
			Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: cannot add any weapon; no secondary weapon without reload detected for equipment '%s' (%s).\n",
					ed->id, invName);

		/* Armour; especially for those without primary weapons. */
		repeat = (float) missedPrimary > frand() * 100.0;
	} else {
		return;
	}

	if (td->armour) {
		do {
			int randNumber = rand() % 100;
			for (i = 0; i < this->csi->numODs; i++) {
				const objDef_t *armour = INVSH_GetItemByIDX(i);
				if (ed->numItems[i] && INV_IsArmour(armour)) {
					randNumber -= ed->numItems[i];
					if (randNumber < 0) {
						const item_t item = {NONE_AMMO, NULL, armour, 0, 0};
						int tuNeed = 0;
						const float weight = GetInventoryState(inv, tuNeed) + INVSH_GetItemWeight(item);
						const int maxTU = GET_TU(speed, GET_ENCUMBRANCE_PENALTY(weight, chr->score.skills[ABILITY_POWER]));
						if (weight > maxWeight || tuNeed > maxTU)
							continue;
						if (TryAddToInventory(inv, &item, &this->csi->ids[this->csi->idArmour])) {
							repeat = 0;
							break;
						}
					}
				}
			}
		} while (repeat-- > 0);
	} else {
		Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: teamdef '%s' may not carry armour (%s)\n",
				td->name, invName);
	}

	{
		int randNumber = rand() % 10;
		for (i = 0; i < this->csi->numODs; i++) {
			if (ed->numItems[i]) {
				const objDef_t *miscItem = INVSH_GetItemByIDX(i);
				if (miscItem->isMisc && !miscItem->weapon) {
					randNumber -= ed->numItems[i];
					if (randNumber < 0) {
						const bool oneShot = miscItem->oneshot;
						const item_t item = {oneShot ? miscItem->ammo : NONE_AMMO, oneShot ? miscItem : NULL, miscItem, 0, 0};
						containerIndex_t container;
						int tuNeed;
						const fireDef_t *itemFd = FIRESH_SlowestFireDef(item);
						const float weight = GetInventoryState(inv, tuNeed) + INVSH_GetItemWeight(item);
						const int maxTU = GET_TU(speed, GET_ENCUMBRANCE_PENALTY(weight, chr->score.skills[ABILITY_POWER]));

						if (miscItem->headgear)
							container = this->csi->idHeadgear;
						else if (miscItem->extension)
							container = this->csi->idExtension;
						else
							container = this->csi->idBackpack;
						if (weight > maxWeight || tuNeed > maxTU || (itemFd && itemFd->time > maxTU))
							continue;
						TryAddToInventory(inv, &item, &this->csi->ids[container]);
					}
				}
			}
		}
	}
}

/**
 * @brief Calculate the number of used inventory slots
 * @return The number of free inventory slots
 */
int InventoryInterface::GetUsedSlots ()
{
	int i = 0;
	const invList_t* slot = invList;
	while (slot) {
		slot = slot->next;
		i++;
	}
	Com_DPrintf(DEBUG_SHARED, "Used inventory slots %i (%s)\n", i, invName);
	return i;
}

/**
 * @brief Initializes the inventory definition by linking the ->next pointers properly.
 * @param[in] name The name that is shown in the output
 * @param[out] interface The inventory interface pointer which should be initialized in this function.
 * @param[in] csi The client-server-information structure
 * @param[in] import Pointers to the lifecycle functions
 * @sa G_Init
 * @sa CL_InitLocal
 */
void InventoryInterface::InitInventory (const char *name, const csi_t* csi, const inventoryImport_t *import)
{
	const item_t item = {NONE_AMMO, NULL, NULL, 0, 0};

	OBJZERO(*this);

	this->import = import;
	this->invName = name;
	this->cacheItem = item;
	this->csi = csi;
	this->invList = NULL;
}

void INV_DestroyInventory (inventoryInterface_t *interface)
{
	if (interface->import == NULL)
		return;
	interface->import->FreeAll();
	OBJZERO(*interface);
}
