/**
 * @file
 * @brief Header for single player market stuff.
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

/** market structure */
typedef struct market_s {
	int numItems[MAX_OBJDEFS];					/**< number of items on the market */
	int bidItems[MAX_OBJDEFS];					/**< price of item for selling */
	int askItems[MAX_OBJDEFS];					/**< price of item for buying */
	double currentEvolutionItems[MAX_OBJDEFS];	/**< evolution of the market */
	bool autosell[MAX_OBJDEFS];					/**< True if item has autosell enabled */
	int numAircraft[AIRCRAFTTYPE_MAX];			/**< number of Aircraft on the market */
	int bidAircraft[AIRCRAFTTYPE_MAX];			/**< price of Aircraft for selling */
	int askAircraft[AIRCRAFTTYPE_MAX];			/**< price of Aircraft for buying */
	double currentEvolutionAircraft[AIRCRAFTTYPE_MAX];	/**< evolution of the market */
} market_t;

bool BS_AircraftIsOnMarket(const aircraft_t* aircraft);
int BS_GetAircraftOnMarket(const aircraft_t* aircraft);
int BS_GetAircraftSellingPrice(const aircraft_t* aircraft);
int BS_GetAircraftBuyingPrice(const aircraft_t* aircraft);
bool BS_BuyAircraft(const aircraft_t* aircraftTemplate, base_t* base);
bool BS_SellAircraft(aircraft_t* aircraft);

bool BS_IsOnMarket(const objDef_t* item);
int BS_GetItemOnMarket(const objDef_t* od);
void BS_AddItemToMarket(const objDef_t* od, int amount);
int BS_GetItemSellingPrice(const objDef_t* od);
int BS_GetItemBuyingPrice(const objDef_t* od);
bool BS_BuyItem(const objDef_t* od, base_t* base, int count);
bool BS_SellItem(const objDef_t* od, base_t* base, int count);

bool BS_BuyUGV(const ugv_t* ugv, base_t* base);
bool BS_SellUGV(Employee* robot);

void BS_InitMarket(const struct campaign_s* campaign);
void CP_CampaignRunMarket(struct campaign_s* campaign);
