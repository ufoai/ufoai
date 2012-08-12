/**
 * @file
 * @brief Artificial Intelligence for opponents or human controlled actors that are panicked.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "g_local.h"
#include "g_ai.h"

static pathing_t *hidePathingTable;
static pathing_t *herdPathingTable;

void AI_Init (void)
{
	hidePathingTable = NULL;
	herdPathingTable = NULL;
}

/**
 * @brief Check whether friendly units are in the line of fire when shooting
 * @param[in] ent AI that is trying to shoot
 * @param[in] target Shoot to this location
 */
static bool AI_CheckFF (const edict_t * ent, const vec3_t target, float spread)
{
	edict_t *check = NULL;
	vec3_t dtarget, dcheck, back;
	float cosSpread;

	/* spread data */
	if (spread < 1.0)
		spread = 1.0;
	spread *= torad;
	cosSpread = cos(spread);
	VectorSubtract(target, ent->origin, dtarget);
	VectorNormalizeFast(dtarget);
	VectorScale(dtarget, PLAYER_WIDTH / spread, back);

	while ((check = G_EdictsGetNextLivingActorOfTeam(check, ent->team))) {
		if (ent != check) {
			/* found ally */
			VectorSubtract(check->origin, ent->origin, dcheck);
			if (DotProduct(dtarget, dcheck) > 0.0) {
				/* ally in front of player */
				VectorAdd(dcheck, back, dcheck);
				VectorNormalizeFast(dcheck);
				if (DotProduct(dtarget, dcheck) > cosSpread)
					return true;
			}
		}
	}

	/* no ally in danger */
	return false;
}

/**
 * @brief Check whether the fighter should perform the shoot
 * @todo Check whether radius and power of fd are to to big for dist
 * @todo Check whether the alien will die when shooting
 */
static bool AI_FighterCheckShoot (const edict_t* ent, const edict_t* check, const fireDef_t* fd, float *dist)
{
	/* check range */
	*dist = VectorDist(ent->origin, check->origin);
	if (*dist > fd->range)
		return false;

	/* if insane, we don't check more */
	if (G_IsInsane(ent))
		return true;

	/* don't shoot - we are to close */
	if (*dist < fd->splrad)
		return false;

	/* check FF */
	if (AI_CheckFF(ent, check->origin, fd->spread[0]))
		return false;

	return true;
}

/**
 * @brief Checks whether the AI controlled actor wants to use a door
 * @param[in] ent The AI controlled actor
 * @param[in] door The door edict
 * @returns true if the AI wants to use (open/close) that door, false otherwise
 * @note Don't start any new events in here, don't change the actor state
 * @sa Touch_DoorTrigger
 * @todo Finish implementation
 */
bool AI_CheckUsingDoor (const edict_t *ent, const edict_t *door)
{
	/* don't try to use the door in every case */
	if (frand() < 0.3)
		return false;

	/* not in the view frustum - don't use the door while not seeing it */
	if (!G_FrustumVis(door, ent->origin))
		return false;

	/* if the alien is trying to hide and the door is
	* still opened, close it */
	if (ent->hiding && door->doorState == STATE_OPENED)
		return true;

	/* aliens and civilians need different handling */
	switch (ent->team) {
	case TEAM_ALIEN: {
		/* only use the door when there is no civilian or phalanx to kill */
		edict_t *check = NULL;

		/* see if there are enemies */
		while ((check = G_EdictsGetNextLivingActor(check))) {
			float actorVis;
			/* don't check for aliens */
			if (check->team == ent->team)
				continue;
			/* check whether the origin of the enemy is inside the
			 * AI actors view frustum */
			if (!G_FrustumVis(check, ent->origin))
				continue;
			/* check whether the enemy is close enough to change the state */
			if (VectorDist(check->origin, ent->origin) > G_ActorSpotDist(ent))
				continue;
			actorVis = G_ActorVis(check->origin, check, ent, true);
			/* there is a visible enemy, don't use that door */
			if (actorVis > ACTOR_VIS_0)
				return false;
		}
		}
		break;
	case TEAM_CIVILIAN:
		/* don't use any door if no alien is inside the viewing angle  - but
		 * try to hide behind the door when there is an alien */
		break;
	default:
		gi.DPrintf("Invalid team in AI_CheckUsingDoor: %i for ent type: %i\n",
			ent->team, ent->type);
		break;
	}
	return true;
}

/**
 * @brief Checks whether it would be smart to change the state to STATE_CROUCHED
 * @param[in] ent The AI controlled actor to chech the state change for
 * @returns true if the actor should go into STATE_CROUCHED, false otherwise
 */
static bool AI_CheckCrouch (const edict_t *ent)
{
	edict_t *check = NULL;

	/* see if we are very well visible by an enemy */
	while ((check = G_EdictsGetNextLivingActor(check))) {
		float actorVis;
		/* don't check for civilians or aliens */
		if (check->team == ent->team || G_IsCivilian(check))
			continue;
		/* check whether the origin of the enemy is inside the
		 * AI actors view frustum */
		if (!G_FrustumVis(check, ent->origin))
			continue;
		/* check whether the enemy is close enough to change the state */
		if (VectorDist(check->origin, ent->origin) > G_ActorSpotDist(ent))
			continue;
		actorVis = G_ActorVis(check->origin, check, ent, true);
		if (actorVis >= ACTOR_VIS_50)
			return true;
	}
	return false;
}

/**
 * @brief Checks whether the given alien should try to hide because there are enemies close
 * enough to shoot the alien.
 * @param[in] ent The alien edict that should (maybe) hide
 * @return @c true if hide is needed or @c false if the alien thinks that it is not needed
 */
static bool AI_HideNeeded (const edict_t *ent)
{
	/* aliens will consider hiding if they are not brave, or there is a dangerous enemy in sight */
	if (ent->morale > mor_brave->integer) {
		edict_t *from = NULL;
		/* test if check is visible */
		while ((from = G_EdictsGetNextLivingActor(from))) {
			if (from->team == ent->team)
				continue;

			if (G_IsCivilian(from))
				continue;

			if (G_IsVisibleForTeam(from, ent->team)) {
				const invList_t *invlist = RIGHT(from);
				const fireDef_t *fd = NULL;
				if (invlist && invlist->item.item) {
					fd = FIRESH_FiredefForWeapon(&invlist->item);
				} else {
					invlist = LEFT(from);
					if (invlist && invlist->item.item)
						fd = FIRESH_FiredefForWeapon(&invlist->item);
				}
				/* search the (visible) inventory (by just checking the weapon in the hands of the enemy */
				if (fd != NULL && fd->range * fd->range >= VectorDistSqr(ent->origin, from->origin)) {
					const int damageRand = fd->damage[0] + (fd->damage[1] * crand());
					const int damage = std::max(0, damageRand);
					if (damage >= ent->HP / 3) {
						const int hidingTeam = AI_GetHidingTeam(ent);
						/* now check whether this enemy is visible for this alien */
						if (G_Vis(hidingTeam, ent, from, VT_NOFRUSTUM))
							return true;
					}
				}
			}
		}
		return false;
	}
	return true;
}

/**
 * @brief Returns useable item from the given inventory list. That means that
 * the 'weapon' has ammunition left or must not be reloaded.
 * @param ic The inventory to search a useable weapon in.
 * @return Ready to fire weapon.
 */
static inline const item_t* AI_GetItemFromInventory (const invList_t *ic)
{
	if (ic != NULL) {
		const item_t *item = &ic->item;
		if (item->ammo && item->item->weapon && (!item->item->reload || item->ammoLeft > 0))
			return item;
	}
	return NULL;
}

/**
 * Returns the item of the currently chosen shoot type of the ai actor.
 * @param shootType The current selected shoot type
 * @param ent The ai actor
 * @return The item that was selected for the given shoot type. This might be @c NULL if
 * no item was found.
 */
const item_t *AI_GetItemForShootType (shoot_types_t shootType, const edict_t *ent)
{
	/* optimization: reaction fire is automatic */
	if (IS_SHOT_REACTION(shootType))
		return NULL;

	/* check that the current selected shoot type also has a valid item in its
	 * corresponding hand slot of the inventory. */
	if (IS_SHOT_RIGHT(shootType)) {
		const invList_t *ic = RIGHT(ent);
		return AI_GetItemFromInventory(ic);
	} else if (IS_SHOT_LEFT(shootType)) {
		const invList_t *ic = LEFT(ent);
		return AI_GetItemFromInventory(ic);
	} else if (IS_SHOT_HEADGEAR(shootType)) {
		return NULL;
	}

	return NULL;
}

/**
 * @brief Returns the value for the vis check whenever an ai actor tries to hide. For aliens this
 * is the inverse team - see the vis check code for the inverse team rules to see how this works.
 * For civilians we have to specify the alien team and can't use the inverse team rules. This is
 * needed because the inverse team rules aren't working for the civilian team - see @c TEAM_CIVILIAN
 * @return A negative team number means "every other team" as the one from the given ent. See the vis
 * check functions for the inverse team rules for more information.
 */
int AI_GetHidingTeam (const edict_t *ent)
{
	if (G_IsCivilian(ent))
		return TEAM_ALIEN;
	return -ent->team;
}

/**
 * @brief Tries to search a hiding spot
 * @param[out] ent The actor edict. The position of the actor is updated here to perform visibility checks
 * @param[in] from The grid position the actor is (theoretically) standing at and searching a hiding location from
 * @param[in,out] tuLeft The amount of left TUs to find a hiding spot. The TUs needed to walk to the grid position
 * is subtracted. May not be @c NULL.
 * @param[in] team The team from which actor tries to hide
 * @return @c true if hiding is possible, @c false otherwise
 */
bool AI_FindHidingLocation (int team, edict_t *ent, const pos3_t from, int *tuLeft)
{
	byte minX, maxX, minY, maxY;
	const byte crouchingState = G_IsCrouched(ent) ? 1 : 0;
	const int distance = std::min(*tuLeft, HIDE_DIST * 2);

	/* We need a local table to calculate the hiding steps */
	if (!hidePathingTable)
		hidePathingTable = (pathing_t *) G_TagMalloc(sizeof(*hidePathingTable), TAG_LEVEL);
	/* search hiding spot */
	G_MoveCalcLocal(hidePathingTable, 0, ent, from, crouchingState, distance);
	ent->pos[2] = from[2];
	minX = std::max(from[0] - HIDE_DIST, 0);
	minY = std::max(from[1] - HIDE_DIST, 0);
	maxX = std::min(from[0] + HIDE_DIST, PATHFINDING_WIDTH - 1);
	maxY = std::min(from[1] + HIDE_DIST, PATHFINDING_WIDTH - 1);

	for (ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++) {
		for (ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++) {
			/* Don't have TUs  to walk there */
			const pos_t delta = G_ActorMoveLength(ent, hidePathingTable, ent->pos, false);
			if (delta > *tuLeft || delta == ROUTING_NOT_REACHABLE)
				continue;

			/* If enemies see this position, it doesn't qualify as hiding spot */
			G_EdictCalcOrigin(ent);
			if (G_IsVisibleForTeam(ent, team))
				continue;

			*tuLeft -= delta;
			return true; /** @todo it could be a good idea to return the best spot found, not the the first found */
		}
	}

	return false;
}

/**
 * @brief Tries to search a spot where actor will be more closer to the target and
 * behind the target from enemy
 * @param[in] ent The actor edict.
 * @param[in] from The grid position the actor is (theoretically) standing at and
 * searching the nearest location from
 * @param[in] target Tries to find the nearest position to this location
 * @param[in] tu The available TUs of the actor
 */
bool AI_FindHerdLocation (edict_t *ent, const pos3_t from, const vec3_t target, int tu)
{
	byte minX, maxX, minY, maxY;
	const byte crouchingState = G_IsCrouched(ent) ? 1 : 0;
	const int distance = std::min(tu, HERD_DIST * 2);
	vec_t length;
	vec_t bestlength = 0.0f;
	pos3_t bestpos;
	edict_t* next = NULL;
	edict_t* enemy = NULL;
	vec3_t vfriend, venemy;

	if (!herdPathingTable)
		herdPathingTable = (pathing_t *) G_TagMalloc(sizeof(*herdPathingTable), TAG_LEVEL);
	/* find the nearest enemy actor to the target*/
	while ((next = G_EdictsGetNextLivingActorOfTeam(next, AI_GetHidingTeam(ent)))) {
		length = VectorDistSqr(target, next->origin);
		if (!bestlength || length < bestlength) {
			enemy = next;
			bestlength = length;
		}
	}
	assert(enemy);

	/* calculate move table */
	G_MoveCalcLocal(herdPathingTable, 0, ent, from, crouchingState, distance);
	ent->pos[2] = from[2];
	minX = std::max(from[0] - HERD_DIST, 0);
	minY = std::max(from[1] - HERD_DIST, 0);
	maxX = std::min(from[0] + HERD_DIST, PATHFINDING_WIDTH - 1);
	maxY = std::min(from[1] + HERD_DIST, PATHFINDING_WIDTH - 1);

	/* search the location */
	VectorCopy(from, bestpos);
	bestlength = VectorDistSqr(target, ent->origin);
	for (ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++) {
		for (ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++) {
			/* time */
			const pos_t delta = G_ActorMoveLength(ent, herdPathingTable, ent->pos, false);
			if (delta > tu || delta == ROUTING_NOT_REACHABLE)
				continue;

			G_EdictCalcOrigin(ent);
			length = VectorDistSqr(ent->origin, target);
			if (length < bestlength) {
				/* check this position to locate behind target from enemy */
				VectorSubtract(target, ent->origin, vfriend);
				VectorNormalizeFast(vfriend);
				VectorSubtract(enemy->origin, ent->origin, venemy);
				VectorNormalizeFast(venemy);
				if (DotProduct(vfriend, venemy) > 0.5) {
					bestlength = length;
					VectorCopy(ent->pos, bestpos);
				}
			}
		}
	}

	if (!VectorCompare(from, bestpos)) {
		VectorCopy(bestpos, ent->pos);
		return true;
	}

	return false;
}

/**
 * @todo This feature causes the 'aliens shoot at walls'-bug.
 * I considered adding a visibility check, but that wouldn't prevent aliens
 * from shooting at the breakable parts of their own ship.
 * So I disabled it for now. Duke, 23.10.09
 */
static edict_t *AI_SearchDestroyableObject (const edict_t *ent, const fireDef_t *fd)
{
#if 0
	/* search best none human target */
	edict_t *check = NULL;
	float dist;

	while ((check = G_EdictsGetNextInUse(check))) {
		if (G_IsBreakable(check)) {
			float vis;

			if (!AI_FighterCheckShoot(ent, check, fd, &dist))
				continue;

			/* check whether target is visible enough */
			vis = G_ActorVis(ent->origin, check, true);
			if (vis < ACTOR_VIS_0)
				continue;

			/* take the first best breakable or door and try to shoot it */
			return check;
		}
	}
#endif
	return NULL;
}

/**
 * @todo timed firedefs that bounce around should not be thrown/shoot about the whole distance
 */
static void AI_SearchBestTarget (aiAction_t *aia, const edict_t *ent, edict_t *check, const item_t *item, shoot_types_t shootType, int tu, float *maxDmg, int *bestTime, const fireDef_t *fdArray)
{
	float vis = ACTOR_VIS_0;
	bool visChecked = false;	/* only check visibily once for an actor */
	fireDefIndex_t fdIdx;
	float dist;

	for (fdIdx = 0; fdIdx < item->ammo->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
		const fireDef_t *fd = &fdArray[fdIdx];
		const float acc = GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill) *
				G_ActorGetInjuryPenalty(ent, MODIFIER_ACCURACY);
		const float nspread = SPREAD_NORM((fd->spread[0] + fd->spread[1]) * 0.5 + acc);
		const int time = G_GetActorTimeForFiredef(ent, fd, false);
		/* how many shoots can this actor do */
		const int shots = tu / time;
		if (shots) {
			float dmg;
			if (!AI_FighterCheckShoot(ent, check, fd, &dist))
				continue;

			/* check how good the target is visible and if we have a shot */
			if (!visChecked) {	/* only do this once per actor ! */
				vis = G_ActorVis(ent->origin, ent, check, true);
				visChecked = true;

				if (vis != ACTOR_VIS_0) {
					/* check weapon can hit */
					vec3_t dir, origin;

					VectorSubtract(check->origin, ent->origin, dir);
					G_GetShotOrigin(ent, fd, dir, origin);

					/* gun-to-target line free? */
					if (G_TestLine(origin, check->origin))
						continue;
				}
			}

			if (vis == ACTOR_VIS_0)
				continue;

			/* calculate expected damage */
			dmg = vis * (fd->damage[0] + fd->spldmg[0]) * fd->shots * shots;
			if (nspread && dist > nspread)
				dmg *= nspread / dist;

			/* take into account armour */
			if (CONTAINER(check, gi.csi->idArmour)) {
				const objDef_t *ad = CONTAINER(check, gi.csi->idArmour)->item.item;
				dmg *= 1.0 - ad->protection[fd->dmgweight] * 0.01;
			}

			if (dmg > check->HP && G_IsReaction(check))
				/* reaction shooters eradication bonus */
				dmg = check->HP + SCORE_KILL + SCORE_REACTION_ERADICATION;
			else if (dmg > check->HP)
				/* standard kill bonus */
				dmg = check->HP + SCORE_KILL;

			/* ammo is limited and shooting gives away your position */
			if ((dmg < 25.0 && vis < 0.2) /* too hard to hit */
				|| (dmg < 10.0 && vis < 0.6) /* uber-armour */
				|| dmg < 0.1) /* at point blank hit even with a stick */
				continue;

			/* civilian malus */
			if (G_IsCivilian(check) && !G_IsInsane(ent))
				dmg *= SCORE_CIV_FACTOR;

			/* Stunned malus */
			if (G_IsStunned(check))
				dmg *= SCORE_DISABLED_FACTOR;

			/* add random effects */
			if (dmg > 0)
				dmg += SCORE_RANDOM * frand();

			/* check if most damage can be done here */
			if (dmg > *maxDmg) {
				*maxDmg = dmg;
				*bestTime = time * shots;
				aia->shootType = shootType;
				aia->shots = shots;
				aia->target = check;
				aia->fd = fd;
				if (G_IsStunned(check))
					aia->z_align = GROUND_DELTA;
				else
					aia->z_align = 0;
			}

			if (!aia->target) {
				aia->target = AI_SearchDestroyableObject(ent, fd);
				if (aia->target) {
					/* don't take vis into account, don't multiply with amount of shots
					 * others (human victims) should be preferred, that's why we don't
					 * want a too high value here */
					*maxDmg = (fd->damage[0] + fd->spldmg[0]);
					*bestTime = time * shots;
					aia->shootType = shootType;
					aia->shots = shots;
					aia->fd = fd;
				}
			}
		}
	}
}

static inline bool AI_IsValidTarget (const edict_t *ent, const edict_t *target)
{
	if (ent == target)
		return false;

	if (G_IsInsane(ent))
		return true;

	if (target->team == ent->team)
		return false;

	/* don't shoot civs in multiplayer */
	if (sv_maxclients->integer > 1 || (g_aihumans->integer && !G_IsAI(ent)))
		return !G_IsCivilian(target);

	return true;
}

/**
 * @sa AI_ActorThink
 * @todo fill z_align values
 * @todo optimize this
 */
static float AI_FighterCalcActionScore (edict_t * ent, pos3_t to, aiAction_t * aia)
{
	edict_t *check = NULL;
	int tu;
	pos_t move;
	shoot_types_t shootType;
	float minDist;
	float bestActionScore, maxDmg;
	int bestTime = -1;

	move = G_ActorMoveLength(ent, level.pathingMap, to, true);
	tu = G_ActorUsableTUs(ent) - move;

	/* test for time */
	if (tu < 0 || move == ROUTING_NOT_REACHABLE)
		return AI_ACTION_NOTHING_FOUND;

	bestActionScore = 0.0;
	OBJZERO(*aia);

	/* set basic parameters */
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	G_EdictSetOrigin(ent, to);

	maxDmg = 0.0;
	/* search best target */
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (G_EdictPosIsSameAs(check, to) || !AI_IsValidTarget(ent, check))
			continue;

		/* shooting */
		maxDmg = 0.0;
		for (shootType = ST_RIGHT; shootType < ST_NUM_SHOOT_TYPES; shootType++) {
			const item_t *item;
			const fireDef_t *fdArray;

			item = AI_GetItemForShootType(shootType, ent);
			if (!item)
				continue;

			fdArray = FIRESH_FiredefForWeapon(item);
			if (fdArray == NULL)
				continue;

			AI_SearchBestTarget(aia, ent, check, item, shootType, tu, &maxDmg, &bestTime, fdArray);
		}
	}
	/* add damage to bestActionScore */
	if (aia->target) {
		bestActionScore += maxDmg;
		assert(bestTime > 0);
		tu -= bestTime;
	}

	if (!G_IsRaged(ent)) {
		const int hidingTeam = AI_GetHidingTeam(ent);
		/* hide */
		if (!AI_HideNeeded(ent) || !(G_TestVis(hidingTeam, ent, VT_PERISH | VT_NOFRUSTUM) & VIS_YES)) {
			/* is a hiding spot */
			bestActionScore += SCORE_HIDE + (aia->target ? SCORE_CLOSE_IN : 0);
		} else if (aia->target && tu >= TU_MOVE_STRAIGHT) {
			/* reward short walking to shooting spot, when seen by enemies; */
			/** @todo do this decently, only penalizing the visible part of walk
			 * and penalizing much more for reaction shooters around;
			 * now it may remove some tactical options from aliens,
			 * e.g. they may now choose only the closer doors;
			 * however it's still better than going three times around soldier
			 * and only then firing at him */
			bestActionScore += std::max(SCORE_CLOSE_IN - move, 0);

			if (!AI_FindHidingLocation(hidingTeam, ent, to, &tu)) {
				/* nothing found */
				G_EdictSetOrigin(ent, to);
				/** @todo Try to crouch if no hiding spot was found - randomized */
			} else {
				/* found a hiding spot */
				VectorCopy(ent->pos, aia->stop);
				bestActionScore += SCORE_HIDE;
				/** @todo also add bonus for fleeing from reaction fire
				 * and a huge malus if more than 1 move under reaction */
			}
		}
	}

	/* reward closing in */
	minDist = CLOSE_IN_DIST;
	check = NULL;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (check->team != ent->team) {
			const float dist = VectorDist(ent->origin, check->origin);
			minDist = std::min(dist, minDist);
		}
	}
	bestActionScore += SCORE_CLOSE_IN * (1.0 - minDist / CLOSE_IN_DIST);

	/* penalize herding */
	check = NULL;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (check->team == ent->team) {
			const float dist = VectorDist(ent->origin, check->origin);
			if (dist < HERD_THRESHOLD)
				bestActionScore -= SCORE_HERDING_PENALTY;
		}
	}

	return bestActionScore;
}

/**
 * @brief Calculates possible actions for a civilian.
 * @param[in] ent Pointer to an edict being civilian.
 * @param[in] to The grid position to walk to.
 * @param[in] aia Pointer to aiAction containing informations about possible action.
 * @sa AI_ActorThink
 * @note Even civilians can use weapons if the teamdef allows this
 */
static float AI_CivilianCalcActionScore (edict_t * ent, pos3_t to, aiAction_t * aia)
{
	edict_t *check = NULL;
	int tu;
	pos_t move;
	float minDist, minDistCivilian, minDistFighter;
	float bestActionScore;
	float reactionTrap = 0.0;
	float delta = 0.0;

	/* set basic parameters */
	bestActionScore = 0.0;
	OBJZERO(*aia);
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	G_EdictSetOrigin(ent, to);

	move = G_ActorMoveLength(ent, level.pathingMap, to, true);
	tu = G_ActorUsableTUs(ent) - move;

	/* test for time */
	if (tu < 0 || move == ROUTING_NOT_REACHABLE)
		return AI_ACTION_NOTHING_FOUND;

	/* check whether this civilian can use weapons */
	if (ent->chr.teamDef) {
		const teamDef_t* teamDef = ent->chr.teamDef;
		if (!G_IsPaniced(ent) && teamDef->weapons)
			return AI_FighterCalcActionScore(ent, to, aia);
	} else
		gi.DPrintf("AI_CivilianCalcActionScore: Error - civilian team with no teamdef\n");

	/* run away */
	minDist = minDistCivilian = minDistFighter = RUN_AWAY_DIST * UNIT_SIZE;

	while ((check = G_EdictsGetNextLivingActor(check))) {
		float dist;
		if (ent == check)
			continue;
		dist = VectorDist(ent->origin, check->origin);
		/* if we are trying to walk to a position that is occupied by another actor already we just return */
		if (!dist)
			return AI_ACTION_NOTHING_FOUND;
		switch (check->team) {
		case TEAM_ALIEN:
			if (dist < minDist)
				minDist = dist;
			break;
		case TEAM_CIVILIAN:
			if (dist < minDistCivilian)
				minDistCivilian = dist;
			break;
		case TEAM_PHALANX:
			if (dist < minDistFighter)
				minDistFighter = dist;
			break;
		}
	}

	minDist /= UNIT_SIZE;
	minDistCivilian /= UNIT_SIZE;
	minDistFighter /= UNIT_SIZE;

	if (minDist < 8.0) {
		/* very near an alien: run away fast */
		delta = 4.0 * minDist;
	} else if (minDist < 16.0) {
		/* near an alien: run away */
		delta = 24.0 + minDist;
	} else if (minDist < 24.0) {
		/* near an alien: run away slower */
		delta = 40.0 + (minDist - 16) / 4;
	} else {
		delta = 42.0;
	}
	/* near a civilian: join him (1/3) */
	if (minDistCivilian < 10.0)
		delta += (10.0 - minDistCivilian) / 3.0;
	/* near a fighter: join him (1/5) */
	if (minDistFighter < 15.0)
		delta += (15.0 - minDistFighter) / 5.0;
	/* don't go close to a fighter to let him move */
	if (minDistFighter < 2.0)
		delta /= 10.0;

	/* try to hide */
	check = NULL;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (ent == check)
			continue;
		if (!(G_IsAlien(check) || G_IsInsane(ent)))
			continue;

		if (!G_IsVisibleForTeam(check, ent->team))
			continue;

		if (G_ActorVis(check->origin, check, ent, true) > 0.25)
			reactionTrap += SCORE_NONHIDING_PLACE_PENALTY;
	}
	delta -= reactionTrap;
	bestActionScore += delta;

	/* add laziness */
	if (ent->TU)
		bestActionScore += SCORE_CIV_LAZINESS * tu / ent->TU;
	/* add random effects */
	bestActionScore += SCORE_CIV_RANDOM * frand();

	return bestActionScore;
}

/**
 * @brief Calculates possible actions for a panicking unit.
 * @param[in] ent Pointer to an edict which is panicking.
 * @param[in] to The grid position to walk to.
 * @param[in] aia Pointer to aiAction containing informations about possible action.
 * @sa AI_ActorThink
 * @note Panicking units will run away from everyone other than their own team (e.g. aliens will run away even from civilians)
 */
static float AI_PanicCalcActionScore (edict_t * ent, pos3_t to, aiAction_t * aia)
{
	edict_t *check = NULL;
	int tu;
	pos_t move;
	float minDistFriendly, minDistOthers;
	float bestActionScore = 0.0;

	/* set basic parameters */
	OBJZERO(*aia);
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	G_EdictSetOrigin(ent, to);

	move = G_ActorMoveLength(ent, level.pathingMap, to, true);
	tu = G_ActorUsableTUs(ent) - move;

	/* test for time */
	if (tu < 0 || move == ROUTING_NOT_REACHABLE)
		return AI_ACTION_NOTHING_FOUND;

	/* run away */
	minDistFriendly = minDistOthers = RUN_AWAY_DIST * UNIT_SIZE;

	while ((check = G_EdictsGetNextLivingActor(check))) {
		float dist;
		if (ent == check)
			continue;
		dist = VectorDist(ent->origin, check->origin);
		/* if we are trying to walk to a position that is occupied by another actor already we just return */
		if (!dist)
			return AI_ACTION_NOTHING_FOUND;
		if (check->team == ent->team) {
			if (dist < minDistFriendly)
				minDistFriendly = dist;
		} else {
			if (dist < minDistOthers)
				minDistOthers = dist;
		}
	}

	minDistFriendly /= UNIT_SIZE;
	minDistOthers /= UNIT_SIZE;

	bestActionScore += SCORE_PANIC_RUN_TO_FRIENDS / minDistFriendly;
	bestActionScore -= SCORE_PANIC_FLEE_FROM_STRANGERS / minDistOthers;

	/* try to hide */
	check = NULL;
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (ent == check)
			continue;
		if (G_IsInsane(ent))
			continue;

		if (!G_IsVisibleForTeam(check, ent->team))
			continue;

		if (G_ActorVis(check->origin, check, ent, true) > 0.25)
			bestActionScore -= SCORE_NONHIDING_PLACE_PENALTY;
	}

	/* add random effects */
	bestActionScore += SCORE_PANIC_RANDOM * frand();

	return bestActionScore;
}

/**
 * @brief Searches the map for mission edicts and try to get there
 * @sa AI_PrepBestAction
 * @note The routing table is still valid, so we can still use
 * gi.MoveLength for the given edict here
 */
static int AI_CheckForMissionTargets (const player_t* player, edict_t *ent, aiAction_t *aia)
{
	int bestActionScore = AI_ACTION_NOTHING_FOUND;

	/* reset any previous given action set */
	OBJZERO(*aia);

	if (ent->team == TEAM_CIVILIAN) {
		edict_t *checkPoint = NULL;
		int length;
		int i = 0;
		/* find waypoints in a closer distance - if civilians are not close enough, let them walk
		 * around until they came close */
		for (checkPoint = ai_waypointList; checkPoint != NULL; checkPoint = checkPoint->groupChain) {
			if (checkPoint->inuse)
				continue;

			/* the lower the count value - the nearer the final target */
			if (checkPoint->count < ent->count) {
				if (VectorDist(ent->origin, checkPoint->origin) <= WAYPOINT_CIV_DIST) {
					const pos_t move = G_ActorMoveLength(ent, level.pathingMap, checkPoint->pos, true);
					i++;
					if (move == ROUTING_NOT_REACHABLE)
						continue;

					/* test for time and distance */
					length = G_ActorUsableTUs(ent) - move;
					bestActionScore = SCORE_MISSION_TARGET + length;

					ent->count = checkPoint->count;
					VectorCopy(checkPoint->pos, aia->to);
				}
			}
		}
		/* reset the count value for this civilian to restart the search */
		if (!i)
			ent->count = 100;
	} else if (ent->team == TEAM_ALIEN) {
		/* search for a mission edict */
		edict_t *mission = NULL;
		while ((mission = G_EdictsGetNextInUse(mission))) {
			if (mission->type == ET_MISSION) {
				VectorCopy(mission->pos, aia->to);
				aia->target = mission;
				if (player->pers.team == mission->team) {
					bestActionScore = SCORE_MISSION_TARGET;
					break;
				} else {
					/* try to prevent the phalanx from reaching their mission target */
					bestActionScore = SCORE_MISSION_OPPONENT_TARGET;
				}
			}
		}
	}

	return bestActionScore;
}

#define AI_MAX_DIST	30
/**
 * @brief Attempts to find the best action for an alien. Moves the alien
 * into the starting position for that action and returns the action.
 * @param[in] player The AI player
 * @param[in] ent The AI actor
 */
static aiAction_t AI_PrepBestAction (const player_t *player, edict_t * ent)
{
	aiAction_t aia, bestAia;
	pos3_t oldPos, to;
	vec3_t oldOrigin;
	int xl, yl, xh, yh;
	int dist;
	float bestActionScore, best;
	/* The actor will be forced to stand before doing the move, so calculations her might be a little off */
	const byte crouchingState = G_IsCrouched(ent) ? 1 : 0;

	/* calculate move table */
	G_MoveCalc(0, ent, ent->pos, crouchingState, G_ActorUsableTUs(ent));
	Com_DPrintf(DEBUG_ENGINE, "AI_PrepBestAction: Called MoveMark.\n");
	gi.MoveStore(level.pathingMap);

	/* set borders */
	dist = (G_ActorUsableTUs(ent) + 1) / 2;
	xl = std::max((int) ent->pos[0] - dist, 0);
	yl = std::max((int) ent->pos[1] - dist, 0);
	xh = std::min((int) ent->pos[0] + dist, PATHFINDING_WIDTH);
	yh = std::min((int) ent->pos[1] + dist, PATHFINDING_WIDTH);

	/* search best action */
	best = AI_ACTION_NOTHING_FOUND;
	VectorCopy(ent->pos, oldPos);
	VectorCopy(ent->origin, oldOrigin);

	/* evaluate moving to every possible location in the search area,
	 * including combat considerations */
	for (to[2] = 0; to[2] < PATHFINDING_HEIGHT; to[2]++)
		for (to[1] = yl; to[1] < yh; to[1]++)
			for (to[0] = xl; to[0] < xh; to[0]++) {
				const pos_t move = G_ActorMoveLength(ent, level.pathingMap, to, true);
				if (move != ROUTING_NOT_REACHABLE && move <= G_ActorUsableTUs(ent)) {
					if (G_IsCivilian(ent))
						bestActionScore = AI_CivilianCalcActionScore(ent, to, &aia);
					else if (G_IsPaniced(ent))
						bestActionScore = AI_PanicCalcActionScore(ent, to, &aia);
					else
						bestActionScore = AI_FighterCalcActionScore(ent, to, &aia);

					if (bestActionScore > best) {
						bestAia = aia;
						best = bestActionScore;
					}
				}
			}

	VectorCopy(oldPos, ent->pos);
	VectorCopy(oldOrigin, ent->origin);

	bestActionScore = AI_CheckForMissionTargets(player, ent, &aia);
	if (bestActionScore > best) {
		bestAia = aia;
		best = bestActionScore;
	}

	/* nothing found to do */
	if (best == AI_ACTION_NOTHING_FOUND) {
		bestAia.target = NULL;
		return bestAia;
	}

	/* check if the actor is in crouched state and try to stand up before doing the move */
	if (G_IsCrouched(ent))
		G_ClientStateChange(player, ent, STATE_CROUCHED, true);

	/* do the move */
	G_ClientMove(player, 0, ent, bestAia.to);

	/* test for possible death during move. reset bestAia due to dead status */
	if (G_IsDead(ent))
		OBJZERO(bestAia);

	return bestAia;
}

edict_t *ai_waypointList;

void G_AddToWayPointList (edict_t *ent)
{
	int i = 1;

	if (!ai_waypointList)
		ai_waypointList = ent;
	else {
		edict_t *e = ai_waypointList;
		while (e->groupChain) {
			e = e->groupChain;
			i++;
		}
		i++;
		e->groupChain = ent;
	}
}

/**
 * @brief This function will turn the AI actor into the direction that is needed to walk
 * to the given location
 * @param[in] ent The actor to turn
 * @param[in] pos The position to set the direction for
 */
void AI_TurnIntoDirection (edict_t *ent, const pos3_t pos)
{
	int dvec;
	const byte crouchingState = G_IsCrouched(ent) ? 1 : 0;

	G_MoveCalc(ent->team, ent, pos, crouchingState, G_ActorUsableTUs(ent));

	dvec = gi.MoveNext(level.pathingMap, pos, crouchingState);
	if (dvec != ROUTING_UNREACHABLE) {
		const byte dir = getDVdir(dvec);
		/* Only attempt to turn if the direction is not a vertical only action */
		if (dir < CORE_DIRECTIONS || dir >= FLYING_DIRECTIONS)
			G_ActorDoTurn(ent, dir & (CORE_DIRECTIONS - 1));
	}
}

/**
 * @brief if a weapon can be reloaded we attempt to do so if TUs permit, otherwise drop it
 */
static void AI_TryToReloadWeapon (edict_t *ent, containerIndex_t containerID)
{
	if (G_ClientCanReload(ent, containerID)) {
		G_ActorReload(ent, INVDEF(containerID));
	} else {
		G_ActorInvMove(ent, INVDEF(containerID), CONTAINER(ent, containerID), INVDEF(gi.csi->idFloor), NONE, NONE, true);
	}
}

/**
 * @brief The think function for the ai controlled aliens or paniced humans
 * @param[in] player The AI or human player object
 * @param[in] ent The ai controlled edict or the human actor edict
 * @sa AI_FighterCalcActionScore
 * @sa AI_CivilianCalcActionScore
 * @sa G_ClientMove
 * @sa G_ClientShoot
 */
void AI_ActorThink (player_t * player, edict_t * ent)
{
	aiAction_t bestAia;

	/* if a weapon can be reloaded we attempt to do so if TUs permit, otherwise drop it */
	if (!G_IsPaniced(ent)) {
		if (RIGHT(ent) && RIGHT(ent)->item.item->reload && RIGHT(ent)->item.ammoLeft == 0)
			AI_TryToReloadWeapon(ent, gi.csi->idRight);
		if (LEFT(ent) && LEFT(ent)->item.item->reload && LEFT(ent)->item.ammoLeft == 0)
			AI_TryToReloadWeapon(ent, gi.csi->idLeft);
	}

	/* if both hands are empty, attempt to get a weapon out of backpack or the
	 * floor (if TUs permit) */
	if (!LEFT(ent) && !RIGHT(ent))
		G_ClientGetWeaponFromInventory(ent);

	bestAia = AI_PrepBestAction(player, ent);

	/* shoot and hide */
	if (bestAia.target) {
		const fireDefIndex_t fdIdx = bestAia.fd ? bestAia.fd->fdIdx : 0;
		/* shoot until no shots are left or target died */
		while (bestAia.shots) {
			G_ClientShoot(player, ent, bestAia.target->pos, bestAia.shootType, fdIdx, NULL, true, bestAia.z_align);
			bestAia.shots--;
			/* died by our own shot? */
			if (G_IsDead(ent))
				return;
			/* check for target's death */
			if (G_IsDead(bestAia.target)) {
				/* search another target now */
				bestAia = AI_PrepBestAction(player, ent);
				/* no other target found - so no need to hide */
				if (!bestAia.target)
					return;
			}
		}
		ent->hiding = true;

		/* now hide - for this we use the team of the alien actor because a phalanx soldier
		 * might become visible during the hide movement */
		G_ClientMove(player, ent->team, ent, bestAia.stop);
		/* no shots left, but possible targets left - maybe they shoot back
		 * or maybe they are still close after hiding */

		/* decide whether the actor wants to crouch */
		if (AI_CheckCrouch(ent))
			G_ClientStateChange(player, ent, STATE_CROUCHED, true);

		/* actor is still alive - try to turn into the appropriate direction to see the target
		 * actor once he sees the ai, too */
		AI_TurnIntoDirection(ent, bestAia.target->pos);

		/** @todo If possible targets that can shoot back (check their inventory for weapons, not for ammo)
		 * are close, go into reaction fire mode, too */
		/* G_ClientStateChange(player, ent->number, STATE_REACTION_ONCE, true); */

		ent->hiding = false;
	}
}

static void AI_PlayerRun (player_t *player)
{
	if (level.activeTeam == player->pers.team) {
		/* find next actor to handle */
		edict_t *ent = player->pers.last;

		while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, player->pers.team))) {
			const int beforeTUs = ent->TU;
			if (beforeTUs > 0) {
				if (g_ailua->integer)
					AIL_ActorThink(player, ent);
				else
					AI_ActorThink(player, ent);
				player->pers.last = ent;

				if (beforeTUs > ent->TU)
					return;
			}
		}

		/* nothing left to do, request endround */
		G_ClientEndRound(player);
		player->pers.last = NULL;
	}
}

/**
 * @brief Every server frame one single actor is handled - always in the same order
 * @sa G_RunFrame
 */
void AI_Run (void)
{
	player_t *player;

	/* don't run this too often to prevent overflows */
	if (level.framenum % 10)
		return;

	/* set players to ai players and cycle over all of them */
	player = NULL;
	while ((player = G_PlayerGetNextActiveAI(player))) {
		AI_PlayerRun(player);
	}

	if (g_aihumans->integer) {
		player = NULL;
		while ((player = G_PlayerGetNextActiveHuman(player))) {
			AI_PlayerRun(player);
		}
	}
}

/**
 * @brief Initializes the actor's stats like morals, strength and so on.
 * @param ent Actor to set the stats for.
 */
static void AI_SetStats (edict_t * ent)
{
	CHRSH_CharGenAbilitySkills(&ent->chr, sv_maxclients->integer >= 2);

	ent->HP = ent->chr.HP;
	ent->morale = ent->chr.morale;
	ent->STUN = 0;

	/* hurt aliens in ufo crash missions (5%: almost dead, 10%: wounded, 15%: stunned)  */
	if (level.hurtAliens && CHRSH_IsTeamDefAlien(ent->chr.teamDef)) {
		const float random = frand();
		int damage = 0, stun = 0;
		if (random <= 0.05f) {
			damage = ent->HP * 0.95f;
		} else if (random <= 0.15f) {
			stun = ent->HP * 0.3f;
			damage = ent->HP * 0.5f;
		} else if (random <= 0.3f) {
			stun = ent->HP * 0.75f;
		}
		ent->HP -= damage;
		if (!CHRSH_IsTeamDefRobot(ent->chr.teamDef))
			ent->STUN = stun;

		for (int i = 0; i < ent->chr.teamDef->bodyTemplate->numBodyParts(); ++i)
			ent->chr.wounds.treatmentLevel[ent->chr.teamDef->bodyTemplate->getRandomBodyPart()] += damage / BODYPART_MAXTYPE;
	}

	G_ActorGiveTimeUnits(ent);
}


/**
 * @brief Sets an actor's character values.
 * @param ent Actor to set the model for.
 * @param[in] team Team to which actor belongs.
 */
static void AI_SetCharacterValues (edict_t * ent, int team)
{
	/* Set model. */
	const char *teamDefinition;
	if (team != TEAM_CIVILIAN) {
		if (gi.csi->numAlienTeams) {
			const int alienTeam = rand() % gi.csi->numAlienTeams;
			const teamDef_t *td = gi.csi->alienTeams[alienTeam];
			assert(td);
			teamDefinition = td->id;
		} else
			teamDefinition = gi.Cvar_String("ai_alien");
	} else if (team == TEAM_CIVILIAN) {
		teamDefinition = gi.Cvar_String("ai_civilian");
	}
	gi.GetCharacterValues(teamDefinition, &ent->chr);
	if (!ent->chr.teamDef)
		gi.Error("Could not set teamDef for character: '%s'", teamDefinition);
}


/**
 * @brief Sets the actor's equipment.
 * @param ent Actor to give equipment to.
 * @param[in] ed Equipment definition for the new actor.
 */
static void AI_SetEquipment (edict_t * ent, const equipDef_t * ed)
{
	/* Pack equipment. */
	if (ent->chr.teamDef->robot && ent->chr.teamDef->onlyWeapon) {
		const objDef_t *weapon = ent->chr.teamDef->onlyWeapon;
		if (weapon->numAmmos > 0)
			game.i.EquipActorRobot(&game.i, &ent->chr.i, weapon);
		else if (weapon->fireTwoHanded)
			game.i.EquipActorMelee(&game.i, &ent->chr.i, ent->chr.teamDef);
		else
			gi.DPrintf("AI_InitPlayer: weapon %s has no ammo assigned and must not be fired two handed\n", weapon->id);
	} else if (ent->chr.teamDef->weapons) {
		game.i.EquipActor(&game.i, &ent->chr.i, ed, ent->chr.teamDef);
	} else {
		gi.DPrintf("AI_InitPlayer: actor with no equipment\n");
	}
}


/**
 * @brief Initializes the actor.
 * @param[in] player Player to which this actor belongs.
 * @param[in,out] ent Pointer to edict_t representing actor.
 * @param[in] ed Equipment definition for the new actor. Might be @c NULL.
 */
static void AI_InitPlayer (const player_t * player, edict_t * ent, const equipDef_t * ed)
{
	const int team = player->pers.team;

	/* Set the model and chose alien race. */
	AI_SetCharacterValues(ent, team);

	/* Calculate stats. */
	AI_SetStats(ent);

	/* Give equipment. */
	if (ed != NULL)
		AI_SetEquipment(ent, ed);

	/* after equipping the actor we can also get the model indices */
	ent->body = gi.ModelIndex(CHRSH_CharGetBody(&ent->chr));
	ent->head = gi.ModelIndex(CHRSH_CharGetHead(&ent->chr));

	/* no need to call G_SendStats for the AI - reaction fire is serverside only for the AI */
	if (frand() < 0.75f) {
		G_ClientStateChange(player, ent, STATE_REACTION, false);
	}

	/* initialize the LUA AI now */
	if (team == TEAM_CIVILIAN)
		AIL_InitActor(ent, "civilian", "default");
	else if (team == TEAM_ALIEN)
		AIL_InitActor(ent, "alien", "default");
	else
		gi.DPrintf("AI_InitPlayer: unknown team AI\n");
}

static const equipDef_t* G_GetEquipmentForAISpawn (int team)
{
	const equipDef_t *ed;
	/* prepare equipment */
	if (team != TEAM_CIVILIAN) {
		const char *equipID = gi.Cvar_String("ai_equipment");
		ed = G_GetEquipDefByID(equipID);
		if (ed == NULL)
			ed = &gi.csi->eds[0];
	} else {
		ed = NULL;
	}
	return ed;
}

static edict_t* G_SpawnAIPlayer (const player_t * player, const equipDef_t *ed)
{
	edict_t *ent = G_ClientGetFreeSpawnPointForActorSize(player, ACTOR_SIZE_NORMAL);
	if (!ent) {
		gi.DPrintf("Not enough spawn points for team %i\n", player->pers.team);
		return NULL;
	}

	/* initialize the new actor */
	AI_InitPlayer(player, ent, ed);

	G_TouchTriggers(ent);

	gi.DPrintf("Spawned ai player for team %i with entnum %i (%s)\n", ent->team, ent->number, ent->chr.name);
	G_CheckVis(ent, VIS_PERISH | VIS_NEW);
	G_CheckVisTeamAll(ent->team, 0, ent);

	return ent;
}

/**
 * @brief Spawn civilians and aliens
 * @param[in] player
 * @param[in] numSpawn
 * @sa AI_CreatePlayer
 */
static void G_SpawnAIPlayers (const player_t * player, int numSpawn)
{
	int i;
	const equipDef_t *ed = G_GetEquipmentForAISpawn(player->pers.team);

	for (i = 0; i < numSpawn; i++) {
		if (G_SpawnAIPlayer(player, ed) == NULL)
			break;
	}

	/* show visible actors */
	G_VisFlagsClear(player->pers.team);
	G_CheckVis(NULL, false);
}

/**
 * @brief If the cvar g_endlessaliens is set we will endlessly respawn aliens
 * @note This can be used for rescue or collect missions where it is enough to do something,
 * and then leave the map (rescue zone)
 */
void AI_CheckRespawn (int team)
{
	if (!g_endlessaliens->integer)
		return;

	if (team != TEAM_ALIEN)
		return;

	const int spawned = level.initialAlienActorsSpawned;
	const int alive = level.num_alive[team];
	int diff = spawned - alive;
	const equipDef_t *ed = G_GetEquipmentForAISpawn(team);

	while (diff > 0) {
		const player_t *player = G_GetPlayerForTeam(team);
		edict_t *ent = G_SpawnAIPlayer(player, ed);
		if (ent == NULL)
			break;

		const unsigned int playerMask = G_VisToPM(ent->visflags);
		G_AppearPerishEvent(playerMask, true, ent, NULL);
		G_EventActorAdd(~playerMask, ent);

		diff--;
	}
}

/**
 * @brief Spawn civilians and aliens
 * @param[in] team
 * @sa G_SpawnAIPlayer
 * @return player_t pointer
 * @note see cvars ai_numaliens, ai_numcivilians, ai_numactors
 */
player_t *AI_CreatePlayer (int team)
{
	player_t *p;

	if (!sv_ai->integer) {
		gi.DPrintf("AI deactivated - set sv_ai cvar to 1 to activate it\n");
		return NULL;
	}

	/* set players to ai players and cycle over all of them */
	p = NULL;
	while ((p = G_PlayerGetNextAI(p))) {
		if (!p->inuse) {
			OBJZERO(*p);
			p->inuse = true;
			p->num = p - game.players;
			p->pers.ai = true;
			G_SetTeamForPlayer(p, team);
			if (p->pers.team == TEAM_CIVILIAN) {
				G_SpawnAIPlayers(p, ai_numcivilians->integer);
			} else {
				if (sv_maxclients->integer == 1)
					G_SpawnAIPlayers(p, ai_numaliens->integer);
				else
					G_SpawnAIPlayers(p, ai_numactors->integer);

				level.initialAlienActorsSpawned = level.num_spawned[p->pers.team];
			}

			gi.DPrintf("Created AI player (team %i)\n", p->pers.team);
			return p;
		}
	}

	/* nothing free */
	return NULL;
}
