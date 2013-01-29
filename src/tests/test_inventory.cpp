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


#include "test_inventory.h"
#include "test_shared.h"
#include "../common/common.h"
#include "../game/inventory.h"

static inventoryInterface_t i;
static const int TAG_INVENTORY = 5546;

static void FreeInventory (void *data)
{
	Mem_Free(data);
}

static void *AllocInventoryMemory (size_t size)
{
	return Mem_PoolAlloc(size, com_genericPool, TAG_INVENTORY);
}

static void FreeAllInventory (void)
{
	Mem_FreeTag(com_genericPool, TAG_INVENTORY);
}

static const inventoryImport_t inventoryImport = { FreeInventory, FreeAllInventory, AllocInventoryMemory };

static inline void ResetInventoryList (void)
{
	INV_DestroyInventory(&i);
	i.InitInventory("test", &csi, &inventoryImport);
}

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteInventory (void)
{
	TEST_Init();
	Com_ParseScripts(true);

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteInventory (void)
{
	TEST_Shutdown();
	return 0;
}

static void testItemAdd (void)
{
	inventory_t inv;
	const objDef_t *od;
	item_t item;
	const invDef_t *container;

	ResetInventoryList();

	OBJZERO(inv);

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.item = od;
	item.ammo = NULL;
	item.ammoLeft = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == false);

	CU_ASSERT_PTR_NOT_NULL(i.AddToInventory(&inv, &item, container, NONE, NONE, 1));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == true);
}

static void testItemDel (void)
{
	inventory_t inv;
	const objDef_t *od;
	item_t item;
	const invDef_t *container;
	invList_t *addedItem;

	ResetInventoryList();

	OBJZERO(inv);

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.item = od;
	item.ammo = NULL;
	item.ammoLeft = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == false);

	addedItem = i.AddToInventory(&inv, &item, container, NONE, NONE, 1);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == true);

	CU_ASSERT(i.RemoveFromInventory(&inv, container, addedItem));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == false);
}

static void testItemMove (void)
{
	inventory_t inv;
	const objDef_t *od;
	item_t item;
	const invDef_t *container, *containerTo;
	invList_t *addedItem;

	ResetInventoryList();

	OBJZERO(inv);
	OBJZERO(item);

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.item = od;
	item.ammo = NULL;
	item.ammoLeft = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == false);

	addedItem = i.AddToInventory(&inv, &item, container, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == true);

	containerTo = INVSH_GetInventoryDefinitionByID("backpack");
	CU_ASSERT_PTR_NOT_NULL(containerTo);

	CU_ASSERT_EQUAL(IA_MOVE, i.MoveInInventory(&inv, container, addedItem, containerTo, NONE, NONE, NULL, NULL));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == false);
	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerTo, &item) == true);
}

static void testItemReload (void)
{
	inventory_t inv;
	const objDef_t *od, *ad;
	item_t item, ammo, ammoFrom;
	const invDef_t *container, *containerFrom;
	invList_t *addedItem;

	ResetInventoryList();

	OBJZERO(inv);

	od = INVSH_GetItemByIDSilent("rpg");
	CU_ASSERT_PTR_NOT_NULL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL(container);

	item.item = od;
	item.ammo = NULL;
	item.ammoLeft = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == false);

	addedItem = i.AddToInventory(&inv, &item, container, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == true);

	ad = INVSH_GetItemByIDSilent("rpg_ammo");
	CU_ASSERT_PTR_NOT_NULL(ad);

	ammo.item = ad;
	ammo.ammo = NULL;
	ammo.ammoLeft = 0;

	containerFrom = INVSH_GetInventoryDefinitionByID("backpack");
	CU_ASSERT_PTR_NOT_NULL(containerFrom);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammo) == false);

	addedItem = i.AddToInventory(&inv, &ammo, containerFrom, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammo) == true);

	CU_ASSERT_EQUAL(IA_RELOAD, i.MoveInInventory(&inv, containerFrom, addedItem, container, NONE, NONE, NULL, NULL));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammo) == false);

	item.ammo = ad;
	item.ammoLeft = 1;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == true);

	ad = INVSH_GetItemByIDSilent("rpg_incendiary_ammo");
	CU_ASSERT_PTR_NOT_NULL(ad);

	ammoFrom.item = ad;
	ammoFrom.ammo = NULL;
	ammoFrom.ammoLeft = 0;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammoFrom) == false);

	addedItem = i.AddToInventory(&inv, &ammoFrom, containerFrom, NONE, NONE, 1);
	CU_ASSERT_PTR_NOT_NULL(addedItem);

	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammoFrom) == true);

	CU_ASSERT_EQUAL(IA_RELOAD_SWAP, i.MoveInInventory(&inv, containerFrom, addedItem, container, NONE, NONE, NULL, NULL));

	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammoFrom) == false);
	CU_ASSERT(INVSH_ExistsInInventory(&inv, containerFrom, &ammo) == true);

	item.ammo = ad;

	CU_ASSERT(INVSH_ExistsInInventory(&inv, container, &item) == true);
}

static bool testAddSingle (inventory_t *inv, const objDef_t *od, const invDef_t *container)
{
	item_t item;

	item.item = od;
	item.ammo = NULL;
	item.ammoLeft = 0;

	return i.TryAddToInventory(inv, &item, container);
}

static void testItemMassActions (void)
{
	inventory_t inv;
	const objDef_t *od;
	const invDef_t *container;
	bool addedItem;
	int i;

	ResetInventoryList();

	OBJZERO(inv);

	od = INVSH_GetItemByIDSilent("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	container = INVSH_GetInventoryDefinitionByID("right");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == true);

	/* second try should fail as the right container is a single container */
	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == false);

	container = INVSH_GetInventoryDefinitionByID("left");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	od = INVSH_GetItemByIDSilent("fraggrenade");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	addedItem = testAddSingle(&inv, od, container);
	CU_ASSERT(addedItem == true);

	container = INVSH_GetInventoryDefinitionByID("equip");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	for (i = 0; i < csi.numODs; i++) {
		int j;
		od = INVSH_GetItemByIDX(i);
		/* every item should be placable on the ground container and there should really be enough space */
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == true);
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == true);
		addedItem = testAddSingle(&inv, od, container);
		CU_ASSERT(addedItem == true);
		for (j = 0; j < od->numAmmos; j++) {
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
			addedItem = testAddSingle(&inv, od->ammos[j], container);
			CU_ASSERT(addedItem == true);
		}
	}
}

static void testItemToHeadgear (void)
{
	inventory_t inv;
	const objDef_t *od;
	const invDef_t *container;
	item_t item;

	ResetInventoryList();

	OBJZERO(inv);

	od = INVSH_GetItemByIDSilent("irgoggles");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	container = INVSH_GetInventoryDefinitionByID("headgear");
	CU_ASSERT_PTR_NOT_NULL_FATAL(container);

	item.item = od;
	item.ammo = NULL;
	item.ammoLeft = 0;

	CU_ASSERT_FALSE(INVSH_ExistsInInventory(&inv, container, &item));

	CU_ASSERT_PTR_NOT_NULL(i.AddToInventory(&inv, &item, container, NONE, NONE, 1));

	CU_ASSERT_TRUE(INVSH_ExistsInInventory(&inv, container, &item));

	CU_ASSERT_PTR_NULL(i.AddToInventory(&inv, &item, container, NONE, NONE, 1));
}

int UFO_AddInventoryTests (void)
{
	/* add a suite to the registry */
	CU_pSuite InventorySuite = CU_add_suite("InventoryTests", UFO_InitSuiteInventory, UFO_CleanSuiteInventory);
	if (InventorySuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(InventorySuite, testItemAdd) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemDel) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemMove) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemMassActions) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemReload) == NULL)
		return CU_get_error();
	if (CU_ADD_TEST(InventorySuite, testItemToHeadgear) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
