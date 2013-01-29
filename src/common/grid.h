/**
 * @file
 * @brief Battlescape grid functions
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

typedef struct pathing_s {
	/* TUs needed to move to this cell for the current actor */
	byte area[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte areaStored[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* Indicates where the actor would have moved from to get to the cell */
	dvec_t areaFrom[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* forbidden list */
	pos_t **fblist;	/**< pointer to forbidden list (entities are standing here) */
	int fblength;	/**< length of forbidden list (amount of entries) */
} pathing_t;

/*==========================================================
GRID ORIENTED MOVEMENT AND SCANNING
==========================================================*/

void Grid_RecalcRouting(mapTiles_t *mapTiles, Routing &routing, const char *name, const GridBox &box, const char **list);
void Grid_RecalcBoxRouting(mapTiles_t *mapTiles, Routing &routing, const GridBox &box, const char **list);
void Grid_CalcPathing(const Routing &routing, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t from, int distance, byte ** fb_list, int fb_length);
bool Grid_FindPath(const Routing &routing, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t from, const pos3_t targetPos, byte crouchingState, int maxTUs, byte ** fb_list, int fb_length);
void Grid_MoveStore(pathing_t *path);
pos_t Grid_MoveLength(const pathing_t *path, const pos3_t to, byte crouchingState, bool stored);
int Grid_MoveNext(const pathing_t *path, const pos3_t toPos, byte crouchingState);
unsigned int Grid_Ceiling(const Routing &routing, const actorSizeEnum_t actorSize, const pos3_t pos);
int Grid_Floor(const Routing &routing, const actorSizeEnum_t actorSize, const pos3_t pos);
int Grid_GetTUsForDirection(const int dir, const int crouched);
int Grid_Filled(const Routing &routing, const actorSizeEnum_t actorSize, const pos3_t pos);
pos_t Grid_Fall(const Routing &routing, const actorSizeEnum_t actorSize, const pos3_t pos);
bool Grid_ShouldUseAutostand (const pathing_t *path, const pos3_t toPos);
void Grid_PosToVec(const Routing &routing, const actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec);
