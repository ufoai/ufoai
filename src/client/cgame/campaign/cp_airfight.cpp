/**
 * @file
 * @brief Airfight related stuff.
 * @todo Somehow i need to know which alien race was in the ufo we shoot down
 * I need this info for spawning the crash site @sa CP_CreateBattleParameters
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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_geoscape.h"
#include "cp_ufo.h"
#include "cp_missions.h"
#include "save/save_airfight.h"
#include "../../sound/s_main.h"

/**
 * @brief Remove a projectile from ccs.projectiles
 * @param[in] projectile The projectile to remove
 * @sa AIRFIGHT_AddProjectile
 */
static void AIRFIGHT_RemoveProjectile (aircraftProjectile_t* projectile)
{
	const ptrdiff_t num = (ptrdiff_t)(projectile - ccs.projectiles);
	REMOVE_ELEM_ADJUST_IDX(ccs.projectiles, num, ccs.numProjectiles);
}

/**
 * @brief Add a projectile in ccs.projectiles
 * @param[in] attackingBase the attacking base in ccs.bases[]. nullptr is the attacker is an aircraft or a samsite.
 * @param[in] attackingInstallation the attacking samsite in ccs.installations[]. nullptr is the attacker is an aircraft or a base.
 * @param[in] attacker Pointer to the attacking aircraft
 * @param[in] target Pointer to the target aircraft
 * @param[in] weaponSlot Pointer to the weapon slot that fires the projectile.
 * @note we already checked in AIRFIGHT_ChooseWeapon that the weapon has still ammo
 * @sa AIRFIGHT_RemoveProjectile
 * @sa AII_ReloadWeapon for the aircraft item reload code
 */
static bool AIRFIGHT_AddProjectile (const base_t* attackingBase, const installation_t* attackingInstallation, aircraft_t* attacker, aircraft_t* target, aircraftSlot_t* weaponSlot)
{
	aircraftProjectile_t* projectile;

	if (ccs.numProjectiles >= MAX_PROJECTILESONGEOSCAPE) {
		Com_DPrintf(DEBUG_CLIENT, "Too many projectiles on map\n");
		return false;
	}

	projectile = &ccs.projectiles[ccs.numProjectiles];

	if (!weaponSlot->ammo) {
		Com_Printf("AIRFIGHT_AddProjectile: Error - no ammo assigned\n");
		return false;
	}

	assert(weaponSlot->item);

	projectile->aircraftItem = weaponSlot->ammo;
	if (attackingBase) {
		projectile->attackingAircraft = nullptr;
		VectorSet(projectile->pos[0], attackingBase->pos[0], attackingBase->pos[1], 0);
		VectorSet(projectile->attackerPos, attackingBase->pos[0], attackingBase->pos[1], 0);
	} else if (attackingInstallation) {
		projectile->attackingAircraft = nullptr;
		VectorSet(projectile->pos[0], attackingInstallation->pos[0], attackingInstallation->pos[1], 0);
		VectorSet(projectile->attackerPos, attackingInstallation->pos[0], attackingInstallation->pos[1], 0);
	} else {
		assert(attacker);
		projectile->attackingAircraft = attacker;
		VectorSet(projectile->pos[0], attacker->pos[0], attacker->pos[1], 0);
		/* attacker may move, use attackingAircraft->pos */
		VectorSet(projectile->attackerPos, 0, 0, 0);
	}

	projectile->numProjectiles++;

	assert(target);
	projectile->aimedAircraft = target;
	VectorSet(projectile->idleTarget, 0, 0, 0);

	projectile->time = 0;
	projectile->angle = 0.0f;

	projectile->bullets = weaponSlot->item->craftitem.bullets;
	projectile->beam = weaponSlot->item->craftitem.beam;
	projectile->rocket = !projectile->bullets && !projectile->beam;

	weaponSlot->ammoLeft--;
	if (weaponSlot->ammoLeft <= 0)
		AII_ReloadWeapon(weaponSlot);

	ccs.numProjectiles++;

	const char* sound;
	if (projectile->bullets) {
		sound = "geoscape/combat-gun";
	} else if (projectile->beam) {
		sound = "geoscape/combat-airlaser";
	} else if (projectile->rocket) {
		sound = "geoscape/combat-rocket";
	} else {
		sound = nullptr;
	}

	if (sound != nullptr)
		cgi->S_StartLocalSample(sound, 1.0f);

	return true;
}

#ifdef DEBUG
/**
 * @brief List all projectiles on map to console.
 * @note called with debug_listprojectile
 */
static void AIRFIGHT_ProjectileList_f (void)
{
	for (int i = 0; i < ccs.numProjectiles; i++) {
		Com_Printf("%i. (idx: %i)\n", i, ccs.projectiles[i].idx);
		Com_Printf("... type '%s'\n", ccs.projectiles[i].aircraftItem->id);
		if (ccs.projectiles[i].attackingAircraft)
			Com_Printf("... shooting aircraft '%s'\n", ccs.projectiles[i].attackingAircraft->id);
		else
			Com_Printf("... base is shooting, or shooting aircraft is destroyed\n");
		if (ccs.projectiles[i].aimedAircraft)
			Com_Printf("... aiming aircraft '%s'\n", ccs.projectiles[i].aimedAircraft->id);
		else
			Com_Printf("... aiming idle target at (%.02f, %.02f)\n",
				ccs.projectiles[i].idleTarget[0], ccs.projectiles[i].idleTarget[1]);
	}
}
#endif

/**
 * @brief Change destination of projectile to an idle point of the map, close to its former target.
 * @param[in] projectile The projectile to update
 */
static void AIRFIGHT_MissTarget (aircraftProjectile_t* projectile)
{
	vec3_t newTarget;
	float offset;

	assert(projectile);

	if (projectile->aimedAircraft) {
		VectorCopy(projectile->aimedAircraft->pos, newTarget);
		projectile->aimedAircraft = nullptr;
	} else {
		VectorCopy(projectile->idleTarget, newTarget);
	}

	/* get the distance between the projectile and target */
	const float distance = GetDistanceOnGlobe(projectile->pos[0], newTarget);

	/* Work out how much the projectile should miss the target by.  We dont want it too close
	 * or too far from the original target.
	 * * 1/3 distance between target and projectile * random (range -0.5 to 0.5)
	 * * Then make sure the value is at least greater than 0.1 or less than -0.1 so that
	 *   the projectile doesn't land too close to the target. */
	offset = (distance / 3) * (frand() - 0.5f);

	if (abs(offset) < 0.1f)
		offset = 0.1f;

	newTarget[0] = newTarget[0] + offset;
	newTarget[1] = newTarget[1] + offset;

	VectorCopy(newTarget, projectile->idleTarget);
}

/**
 * @brief Check if the selected weapon can shoot.
 * @param[in] slot Pointer to the weapon slot to shoot with.
 * @param[in] distance distance between the weapon and the target.
 * @return 0 AIRFIGHT_WEAPON_CAN_SHOOT if the weapon can shoot,
 * -1 AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT if it can't shoot atm,
 * -2 AIRFIGHT_WEAPON_CAN_NEVER_SHOOT if it will never be able to shoot.
 */
int AIRFIGHT_CheckWeapon (const aircraftSlot_t* slot, float distance)
{
	assert(slot);

	/* check if there is a functional weapon in this slot */
	if (!slot->item || slot->installationTime != 0)
		return AIRFIGHT_WEAPON_CAN_NEVER_SHOOT;

	/* check if there is still ammo in this weapon */
	if (!slot->ammo || (slot->ammoLeft <= 0))
		return AIRFIGHT_WEAPON_CAN_NEVER_SHOOT;

	/* check if the target is within range of this weapon */
	if (distance > slot->ammo->craftitem.stats[AIR_STATS_WRANGE])
		return AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT;

	/* check if weapon is reloaded */
	if (slot->delayNextShot > 0)
		return AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT;

	return AIRFIGHT_WEAPON_CAN_SHOOT;
}

/**
 * @brief Choose the weapon an attacking aircraft will use to fire on a target.
 * @param[in] slot Pointer to the first weapon slot of attacking base or aircraft.
 * @param[in] maxSlot maximum number of weapon slots in attacking base or aircraft.
 * @param[in] pos position of attacking base or aircraft.
 * @param[in] targetPos Pointer to the aimed aircraft.
 * @return indice of the slot to use (in array weapons[]),
 * -1 AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT no weapon to use atm,
 * -2 AIRFIGHT_WEAPON_CAN_NEVER_SHOOT if no weapon to use at all.
 * @sa AIRFIGHT_CheckWeapon
 */
int AIRFIGHT_ChooseWeapon (const aircraftSlot_t* slot, int maxSlot, const vec2_t pos, const vec2_t targetPos)
{
	int slotIdx = AIRFIGHT_WEAPON_CAN_NEVER_SHOOT;
	float distance0 = 99999.9f;
	const float distance = GetDistanceOnGlobe(pos, targetPos);

	/* We choose the usable weapon with the smallest range */
	for (int i = 0; i < maxSlot; i++) {
		const int weaponStatus = AIRFIGHT_CheckWeapon(slot + i, distance);

		/* set slotIdx to AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT if needed */
		/* this will only happen if weapon_state is AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT
		 * and no weapon has been found that can shoot. */
		if (weaponStatus > slotIdx)
			slotIdx = AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT;

		/* select this weapon if this is the one with the shortest range */
		if (weaponStatus >= AIRFIGHT_WEAPON_CAN_SHOOT && distance < distance0) {
			slotIdx = i;
			distance0 = distance;
		}
	}
	return slotIdx;
}

/**
 * @brief Calculate the probability to hit the enemy.
 * @param[in] shooter Pointer to the attacking aircraft (may be nullptr if a base fires the projectile).
 * @param[in] target Pointer to the aimed aircraft (may be nullptr if a target is a base).
 * @param[in] slot Slot containing the weapon firing.
 * @return Probability to hit the target (0 when you don't have a chance, 1 (or more) when you're sure to hit).
 * @note that modifiers due to electronics, weapons, and shield are already taken into account in AII_UpdateAircraftStats
 * @sa AII_UpdateAircraftStats
 * @sa AIRFIGHT_ExecuteActions
 * @sa AIRFIGHT_ChooseWeapon
 * @pre slotIdx must have a weapon installed, with ammo available (see AIRFIGHT_ChooseWeapon)
 * @todo This probability should also depend on the pilot skills, when they will be implemented.
 */
static float AIRFIGHT_ProbabilityToHit (const aircraft_t* shooter, const aircraft_t* target, const aircraftSlot_t* slot)
{
	float probability = 0.0f;

	if (!slot->item) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no weapon assigned to attacking aircraft\n");
		return probability;
	}

	if (!slot->ammo) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no ammo in weapon of attacking aircraft\n");
		return probability;
	}

	/* Take Base probability from the ammo of the attacking aircraft */
	probability = slot->ammo->craftitem.stats[AIR_STATS_ACCURACY];
	Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Base probability: %f\n", probability);

	/* Modify this probability by items of the attacking aircraft (stats is in percent) */
	if (shooter)
		probability *= shooter->stats[AIR_STATS_ACCURACY] / 100.0f;

	Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Probability after accounting items of attacker: %f\n", probability);

	/* Modify this probability by items of the aimed aircraft (stats is in percent) */
	if (target)
		probability /= target->stats[AIR_STATS_ECM] / 100.0f;

	Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Probability after accounting ECM of target: %f\n", probability);

	/* If shooter is a PHALANX craft, check the targeting skills of the pilot */
	if (shooter && shooter->type != AIRCRAFT_UFO) {
		if (shooter->pilot) {
			/**
			 * Targeting skill increases hit chance for shooter
			 * With this equation, max increase (0.29) is reached at skill level 70. Any higher skill rating actually
			 * reduces the bonus, so a skill cap should be placed at 70 when skill increase is implemented for pilots.
			 */
			probability += ( ( ( 1.4f - ( shooter->pilot->chr.score.skills[SKILL_TARGETING] / 100.0f ) ) * ( shooter->pilot->chr.score.skills[SKILL_TARGETING] / 100.0f ) ) - 0.2f );

			Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Probability after accounting targeting skill of shooter: %f\n",
					probability);
		}
	}

	/* If target is a PHALANX craft, check the evading skills of the pilot */
	if (target && target->type != AIRCRAFT_UFO) {
		if (target->pilot) {
			/**
			 * Evasion skill decreases hit chance for shooter
			 * With this equation, max decrease (0.29) is reached at skill level 70. Any higher skill rating actually
			 * reduces the bonus, so a skill cap should be placed at 70 when skill increase is implemented for pilots.
			 */
			probability -= ( ( ( 1.4f - ( target->pilot->chr.score.skills[SKILL_EVADING] / 100.0f ) ) * ( target->pilot->chr.score.skills[SKILL_EVADING] / 100.0f ) ) - 0.2f );

			Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Probability after accounting evasion skill of target: %f\n",
					probability);
		}
	}

	/* Probability should not exceed 0.95 so there is always a chance to miss */
	probability = std::min(probability, 0.95f);

	Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Probability to hit: %f\n", probability);

	return probability;
}

/**
 * @brief Decide what an attacking aircraft can do.
 * @param[in] campaign The campaign data structure
 * @param[in] shooter The aircraft we attack with.
 * @param[in] target The ufo we are going to attack.
 * @todo Implement me and display an attack popup.
 */
void AIRFIGHT_ExecuteActions (const campaign_t* campaign, aircraft_t* shooter, aircraft_t* target)
{
	/* some asserts */
	assert(shooter);
	assert(target);

	/* Check if the attacking aircraft can shoot */
	const int slotIdx = AIRFIGHT_ChooseWeapon(shooter->weapons, shooter->maxWeapons, shooter->pos, target->pos);

	/* if weapon found that can shoot */
	if (slotIdx >= AIRFIGHT_WEAPON_CAN_SHOOT) {
		aircraftSlot_t* weaponSlot = &shooter->weapons[slotIdx];
		const objDef_t* ammo = weaponSlot->ammo;

		/* shoot */
		if (AIRFIGHT_AddProjectile(nullptr, nullptr, shooter, target, weaponSlot)) {
			/* will we miss the target ? */
			const float probability = frand();
			Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ExecuteActions: %s - Random probability to hit: %f\n", shooter->name, probability);
			weaponSlot->delayNextShot = ammo->craftitem.weaponDelay;

			const float calculatedProbability = AIRFIGHT_ProbabilityToHit(shooter, target, shooter->weapons + slotIdx);
			Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ExecuteActions: %s - Calculated probability to hit: %f\n", shooter->name, calculatedProbability);

			if (probability > calculatedProbability)
				AIRFIGHT_MissTarget(&ccs.projectiles[ccs.numProjectiles - 1]);

			if (shooter->type != AIRCRAFT_UFO) {
				/* Maybe UFO is going to shoot back ? */
				UFO_CheckShootBack(campaign, target, shooter);
			} else {
				/* an undetected UFO within radar range and firing should become detected */
				if (!shooter->detected && RADAR_CheckRadarSensored(shooter->pos)) {
					/* stop time and notify */
					MSO_CheckAddNewMessage(NT_UFO_ATTACKING, _("Notice"), va(_("A UFO is shooting at %s"), target->name));
					RADAR_AddDetectedUFOToEveryRadar(shooter);
					UFO_DetectNewUFO(shooter);
				}
			}
		}
	} else if (slotIdx == AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT) {
		/* no ammo to fire atm (too far or reloading), pursue target */
		if (shooter->type == AIRCRAFT_UFO) {
			/** @todo This should be calculated only when target destination changes, or when aircraft speed changes.
			 *  @sa AIR_GetDestination */
			UFO_SendPursuingAircraft(shooter, target);
		} else
			AIR_SendAircraftPursuingUFO(shooter, target);
	} else {
		/* no ammo left, or no weapon, proceed with mission */
		if (shooter->type == AIRCRAFT_UFO) {
			shooter->aircraftTarget = nullptr;		/* reset target */
			CP_UFOProceedMission(campaign, shooter);
		} else {
			MS_AddNewMessage(_("Notice"), _("Our aircraft has no more ammo left - returning to home base now."));
			AIR_AircraftReturnToBase(shooter);
		}
	}
}

/**
 * @brief Set all projectile aiming a given aircraft to an idle destination.
 * @param[in] aircraft Pointer to the aimed aircraft.
 * @note This function is called when @c aircraft is destroyed.
 * @sa AIRFIGHT_ActionsAfterAirfight
 */
void AIRFIGHT_RemoveProjectileAimingAircraft (const aircraft_t* aircraft)
{
	aircraftProjectile_t* projectile;
	int idx = 0;

	if (!aircraft)
		return;

	for (projectile = ccs.projectiles; idx < ccs.numProjectiles; projectile++, idx++) {
		if (projectile->aimedAircraft != aircraft)
			continue;

		AIRFIGHT_MissTarget(projectile);
		if (projectile->attackingAircraft && projectile->attackingAircraft->homebase) {
			assert(!AIR_IsUFO(projectile->attackingAircraft));
			AIR_AircraftReturnToBase(projectile->attackingAircraft);
		}
	}
}

/**
 * @brief Set all projectile attackingAircraft pointers to nullptr
 * @param[in] aircraft Pointer to the destroyed aircraft.
 * @note This function is called when @c aircraft is destroyed.
 */
static void AIRFIGHT_UpdateProjectileForDestroyedAircraft (const aircraft_t* aircraft)
{
	aircraftProjectile_t* projectile;
	int idx;

	for (idx = 0, projectile = ccs.projectiles; idx < ccs.numProjectiles; projectile++, idx++) {
		const aircraft_t* attacker = projectile->attackingAircraft;

		if (attacker == aircraft)
			projectile->attackingAircraft = nullptr;
	}
}

/**
 * @brief Actions to execute when a fight is done.
 * @param[in] campaign The campaign data structure
 * @param[in] shooter Pointer to the aircraft that fired the projectile.
 * @param[in] aircraft Pointer to the aircraft which was destroyed (alien or phalanx).
 * @param[in] phalanxWon true if PHALANX won, false if UFO won.
 * @note Some of these mission values are redone (and not reloaded) in CP_Load
 * @note shooter may be nullptr
 * @sa UFO_DestroyAllUFOsOnGeoscape_f
 * @sa CP_Load
 * @sa CP_SpawnCrashSiteMission
 */
void AIRFIGHT_ActionsAfterAirfight (const campaign_t* campaign, aircraft_t* shooter, aircraft_t* aircraft, bool phalanxWon)
{
	if (phalanxWon) {
		const byte* color;

		assert(aircraft);

		/* change destination of other projectiles aiming aircraft */
		AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);
		/* now update the projectile for the destroyed aircraft, too */
		AIRFIGHT_UpdateProjectileForDestroyedAircraft(aircraft);

		/* don't remove ufo from global array: the mission is not over yet
		 * UFO are removed from game only at the end of the mission
		 * (in case we need to know what item to collect e.g.) */

		/* get the color value of the map at the crash position */
		color = GEO_GetColor(aircraft->pos, MAPTYPE_TERRAIN, nullptr);
		/* if this color value is not the value for water ...
		 * and we hit the probability to spawn a crashsite mission */
		if (!MapIsWater(color)) {
			CP_SpawnCrashSiteMission(aircraft);
		} else {
			Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ActionsAfterAirfight: zone: %s (%i:%i:%i)\n", cgi->csi->terrainDefs.getTerrainName(color), color[0], color[1], color[2]);
			MS_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost to sea."));
			CP_MissionIsOverByUFO(aircraft);
		}

		/* skill increase (for aircraft only, base defences skip) */
		if (shooter) {
			/* Increase targeting skill of pilot who destroyed UFO. Never more than 70, see AIRFIGHT_ProbabilityToHit() */
			shooter->pilot->chr.score.skills[SKILL_TARGETING] += 1;
			shooter->pilot->chr.score.skills[SKILL_TARGETING] = std::min(shooter->pilot->chr.score.skills[SKILL_TARGETING], 70);

			/* Increase evasion skill of pilot who destroyed UFO if the aircraft it attacked can carry weapons.
			 * Never more than 70, see AIRFIGHT_ProbabilityToHit() */
			if (aircraft->maxWeapons > 0) {
				shooter->pilot->chr.score.skills[SKILL_EVADING] += 1;
				shooter->pilot->chr.score.skills[SKILL_EVADING] = std::min(shooter->pilot->chr.score.skills[SKILL_EVADING], 70);
			}
		}
	} else {
		/* change destination of other projectiles aiming aircraft */
		AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);

		/* and now update the projectile pointers (there still might be some in the air
		 * of the current destroyed aircraft) - this is needed not send the aircraft
		 * back to base as soon as the projectiles will hit their target */
		AIRFIGHT_UpdateProjectileForDestroyedAircraft(aircraft);

		/* notify UFOs that a phalanx aircraft has been destroyed */
		UFO_NotifyPhalanxAircraftRemoved(aircraft);

		if (!MapIsWater(GEO_GetColor(aircraft->pos, MAPTYPE_TERRAIN, nullptr))) {
			CP_SpawnRescueMission(aircraft, shooter);
		} else {
			/* Destroy the aircraft and everything onboard - the aircraft pointer
			 * is no longer valid after this point */
			bool pilotSurvived = false;
			if (AIR_PilotSurvivedCrash(aircraft))
				pilotSurvived = true;

			AIR_DestroyAircraft(aircraft, pilotSurvived);

			if (pilotSurvived)
				MS_AddNewMessage(_("Interception"), _("Pilot ejected from craft"), MSG_STANDARD);
			else
				MS_AddNewMessage(_("Interception"), _("Pilot killed in action"), MSG_STANDARD);
		}

		/* Make UFO proceed with its mission, if it has not been already destroyed */
		if (shooter)
			CP_UFOProceedMission(campaign, shooter);

		MS_AddNewMessage(_("Interception"), _("A PHALANX craft has been destroyed"), MSG_DEATH);
	}
}

/**
 * @brief Check if some projectiles on geoscape reached their destination.
 * @note Destination is not necessarily an aircraft, in case the projectile missed its initial target.
 * @param[in] projectile Pointer to the projectile
 * @param[in] movement distance that the projectile will do up to next draw of geoscape
 * @sa AIRFIGHT_CampaignRunProjectiles
 */
static bool AIRFIGHT_ProjectileReachedTarget (const aircraftProjectile_t* projectile, float movement)
{
	float distance;

	if (!projectile->aimedAircraft)
		/* the target is idle, its position is in idleTarget*/
		distance = GetDistanceOnGlobe(projectile->idleTarget, projectile->pos[0]);
	else {
		/* the target is moving, pointer to the other aircraft is aimedAircraft */
		distance = GetDistanceOnGlobe(projectile->aimedAircraft->pos, projectile->pos[0]);
	}

	/* projectile reaches its target */
	if (distance < movement)
		return true;

	assert(projectile->aircraftItem);

	/* check if the projectile went farther than it's range */
	distance = (float) projectile->time * projectile->aircraftItem->craftitem.weaponSpeed / (float)SECONDS_PER_HOUR;
	if (distance > projectile->aircraftItem->craftitem.stats[AIR_STATS_WRANGE])
		return true;

	return false;
}

/**
 * @brief Calculates the damage value for the airfight
 * @param[in] od The ammo object definition of the craft item
 * @param[in] target The aircraft the ammo hits
 * @return the damage the hit causes
 * @sa AII_UpdateAircraftStats
 * @note ECM is handled in AIRFIGHT_ProbabilityToHit
 */
static int AIRFIGHT_GetDamage (const objDef_t* od, const aircraft_t* target)
{
	int damage;

	assert(od);

	/* already destroyed - do nothing */
	if (target->damage <= 0)
		return 0;

	/* base damage is given by the ammo */
	damage = od->craftitem.weaponDamage;

	/* reduce damages with shield target */
	damage -= target->stats[AIR_STATS_SHIELD];

	return damage;
}

/**
 * @brief Solve the result of one projectile hitting an aircraft.
 * @param[in] campaign The campaign data structure
 * @param[in] projectile Pointer to the projectile.
 * @note the target loose (base damage - shield of target) hit points
 */
static void AIRFIGHT_ProjectileHits (const campaign_t* campaign, aircraftProjectile_t* projectile)
{
	aircraft_t* target;

	assert(projectile);
	target = projectile->aimedAircraft;
	assert(target);

	/* if the aircraft is not on geoscape anymore, do nothing (returned to base) */
	if (AIR_IsAircraftInBase(target))
		return;

	const int damage = AIRFIGHT_GetDamage(projectile->aircraftItem, target);

	/* apply resulting damages - but only if damage > 0 - because the target might
	 * already be destroyed, and we don't want to execute the actions after airfight
	 * for every projectile */
	if (damage > 0) {
		assert(target->damage > 0);
		target->damage -= damage;
		if (target->damage <= 0) {
			/* Target is destroyed */
			AIRFIGHT_ActionsAfterAirfight(campaign, projectile->attackingAircraft, target, target->type == AIRCRAFT_UFO);
			cgi->S_StartLocalSample("geoscape/combat-explosion", 1.0f);
		} else {
			if (projectile->rocket)
				cgi->S_StartLocalSample("geoscape/combat-rocket-exp", 1.0f);
		}
	}
}

/**
 * @brief Get the next point in the object path based on movement converting the positions from
 * polar coordinates to vector for the calculation and back again to be returned.
 * @param[in] movement The distance that the object needs to move.
 * @param[in] originalPoint The point from which the object is moving.
 * @param[in] orthogonalVector The orthogonal vector.
 * @param[out] finalPoint The next point from the original point + movement in "angle" direction.
 */
static void AIRFIGHT_GetNextPointInPathFromVector (const float* movement, const vec2_t originalPoint, const vec3_t orthogonalVector, vec2_t finalPoint)
{
	vec3_t startPoint, finalVectorPoint;

	PolarToVec(originalPoint, startPoint);
	RotatePointAroundVector(finalVectorPoint, orthogonalVector, startPoint, *movement);
	VecToPolar(finalVectorPoint, finalPoint);
}

/**
 * @brief Get the next point in the object path based on movement.
 * @param[in] movement The distance that the object needs to move.
 * @param[in] originalPoint The point from which the object is moving.
 * @param[in] targetPoint The final point to which the object is moving.
 * @param[out] angle The direction that the object moving in.
 * @param[out] finalPoint The next point from the original point + movement in "angle" direction.
 * @param[out] orthogonalVector The orthogonal vector.
 */
static void AIRFIGHT_GetNextPointInPath (const float* movement, const vec2_t originalPoint, const vec2_t targetPoint, float* angle, vec2_t finalPoint, vec3_t orthogonalVector)
{
	*angle = GEO_AngleOfPath(originalPoint, targetPoint, nullptr, orthogonalVector);
	AIRFIGHT_GetNextPointInPathFromVector(movement, originalPoint, orthogonalVector, finalPoint);
}

/**
 * @brief Update values of projectiles.
 * @param[in] campaign The campaign data structure
 * @param[in] dt Time elapsed since last call of this function.
 */
void AIRFIGHT_CampaignRunProjectiles (const campaign_t* campaign, int dt)
{
	int idx;

	/* ccs.numProjectiles is changed in AIRFIGHT_RemoveProjectile */
	for (idx = ccs.numProjectiles - 1; idx >= 0; idx--) {
		aircraftProjectile_t* projectile = &ccs.projectiles[idx];
		const float movement = (float) dt * projectile->aircraftItem->craftitem.weaponSpeed / (float)SECONDS_PER_HOUR;
		projectile->time += dt;
		projectile->hasMoved = true;
		projectile->numInterpolationPoints = 0;

		/* Check if the projectile reached its destination (aircraft or idle point) */
		if (AIRFIGHT_ProjectileReachedTarget(projectile, movement)) {
			/* check if it got the ennemy */
			if (projectile->aimedAircraft)
				AIRFIGHT_ProjectileHits(campaign, projectile);

			/* remove the missile from ccs.projectiles[] */
			AIRFIGHT_RemoveProjectile(projectile);
		} else {
			float angle;
			vec3_t ortogonalVector, finalPoint, projectedPoint;

			/* missile is moving towards its target */
			if (projectile->aimedAircraft) {
				AIRFIGHT_GetNextPointInPath(&movement, projectile->pos[0], projectile->aimedAircraft->pos, &angle, finalPoint, ortogonalVector);
				AIRFIGHT_GetNextPointInPath(&movement, finalPoint, projectile->aimedAircraft->pos, &angle, projectedPoint, ortogonalVector);
			} else {
				AIRFIGHT_GetNextPointInPath(&movement, projectile->pos[0], projectile->idleTarget, &angle, finalPoint, ortogonalVector);
				AIRFIGHT_GetNextPointInPath(&movement, finalPoint, projectile->idleTarget, &angle, projectedPoint, ortogonalVector);
			}

			/* update angle of the projectile */
			projectile->angle = angle;
			VectorCopy(finalPoint, projectile->pos[0]);
			VectorCopy(projectedPoint, projectile->projectedPos[0]);
		}
	}
}

/**
 * @brief Check if one type of battery (missile or laser) can shoot now.
 * @param[in] base Pointer to the firing base.
 * @param[in] weapons The base weapon to check and fire.
 * @param[in] maxWeapons The number of weapons in that base.
 */
static void AIRFIGHT_BaseShoot (const base_t* base, baseWeapon_t* weapons, int maxWeapons)
{
	for (int i = 0; i < maxWeapons; i++) {
		aircraft_t* target = weapons[i].target;
		aircraftSlot_t* slot = &(weapons[i].slot);
		/* if no target, can't shoot */
		if (!target)
			continue;

		/* If the weapon is not ready in base, can't shoot. */
		if (slot->installationTime > 0)
			continue;

		/* if weapon is reloading, can't shoot */
		if (slot->delayNextShot > 0)
			continue;

		/* check that the ufo is still visible */
		if (!UFO_IsUFOSeenOnGeoscape(target)) {
			weapons[i].target = nullptr;
			continue;
		}

		/* Check if we can still fire on this target. */
		const float distance = GetDistanceOnGlobe(base->pos, target->pos);
		const int test = AIRFIGHT_CheckWeapon(slot, distance);
		/* weapon unable to shoot, reset target */
		if (test == AIRFIGHT_WEAPON_CAN_NEVER_SHOOT) {
			weapons[i].target = nullptr;
			continue;
		}
		/* we can't shoot with this weapon atm, wait to see if UFO comes closer */
		else if (test == AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT)
			continue;
		/* target is too far, wait to see if UFO comes closer */
		else if (distance > slot->ammo->craftitem.stats[AIR_STATS_WRANGE])
			continue;

		/* shoot */
		if (AIRFIGHT_AddProjectile(base, nullptr, nullptr, target, slot)) {
			slot->delayNextShot = slot->ammo->craftitem.weaponDelay;
			/* will we miss the target ? */
			if (frand() > AIRFIGHT_ProbabilityToHit(nullptr, target, slot))
				AIRFIGHT_MissTarget(&ccs.projectiles[ccs.numProjectiles - 1]);
		}
	}
}

/**
 * @brief Check if one type of battery (missile or laser) can shoot now.
 * @param[in] installation Pointer to the firing intallation.
 * @param[in] weapons The installation weapons to check and fire.
 * @param[in] maxWeapons The number of weapons in that installation.
 */
static void AIRFIGHT_InstallationShoot (const installation_t* installation, baseWeapon_t* weapons, int maxWeapons)
{
	for (int i = 0; i < maxWeapons; i++) {
		aircraft_t* target = weapons[i].target;
		aircraftSlot_t* slot = &(weapons[i].slot);
		/* if no target, can't shoot */
		if (!target)
			continue;

		/* If the weapon is not ready in base, can't shoot. */
		if (slot->installationTime > 0)
			continue;

		/* if weapon is reloading, can't shoot */
		if (slot->delayNextShot > 0)
			continue;

		/* check that the ufo is still visible */
		if (!UFO_IsUFOSeenOnGeoscape(target)) {
			weapons[i].target = nullptr;
			continue;
		}

		/* Check if we can still fire on this target. */
		const float distance = GetDistanceOnGlobe(installation->pos, target->pos);
		const int test = AIRFIGHT_CheckWeapon(slot, distance);
		/* weapon unable to shoot, reset target */
		if (test == AIRFIGHT_WEAPON_CAN_NEVER_SHOOT) {
			weapons[i].target = nullptr;
			continue;
		}
		/* we can't shoot with this weapon atm, wait to see if UFO comes closer */
		else if (test == AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT)
			continue;
		/* target is too far, wait to see if UFO comes closer */
		else if (distance > slot->ammo->craftitem.stats[AIR_STATS_WRANGE])
			continue;

		/* shoot */
		if (AIRFIGHT_AddProjectile(nullptr, installation, nullptr, target, slot)) {
			slot->delayNextShot = slot->ammo->craftitem.weaponDelay;
			/* will we miss the target ? */
			if (frand() > AIRFIGHT_ProbabilityToHit(nullptr, target, slot))
				AIRFIGHT_MissTarget(&ccs.projectiles[ccs.numProjectiles - 1]);
		}
	}
}

/**
 * @brief Run base defences.
 * @param[in] dt Time elapsed since last call of this function.
 */
void AIRFIGHT_CampaignRunBaseDefence (int dt)
{
	base_t* base;

	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		int idx;

		if (B_IsUnderAttack(base))
			continue;

		for (idx = 0; idx < base->numBatteries; idx++) {
			baseWeapon_t* battery = &base->batteries[idx];
			aircraftSlot_t* slot = &battery->slot;
			if (slot->delayNextShot > 0)
				slot->delayNextShot -= dt;
			if (slot->ammoLeft <= 0)
				AII_ReloadWeapon(slot);
		}

		for (idx = 0; idx < base->numLasers; idx++) {
			baseWeapon_t* battery = &base->lasers[idx];
			aircraftSlot_t* slot = &battery->slot;
			if (slot->delayNextShot > 0)
				slot->delayNextShot -= dt;
			if (slot->ammoLeft <= 0)
				AII_ReloadWeapon(slot);
		}

		if (AII_BaseCanShoot(base)) {
			if (B_GetBuildingStatus(base, B_DEFENCE_MISSILE))
				AIRFIGHT_BaseShoot(base, base->batteries, base->numBatteries);
			if (B_GetBuildingStatus(base, B_DEFENCE_LASER))
				AIRFIGHT_BaseShoot(base, base->lasers, base->numLasers);
		}
	}

	INS_Foreach(installation) {
		if (installation->installationStatus != INSTALLATION_WORKING)
			continue;

		if (installation->installationTemplate->maxBatteries <= 0)
			continue;

		for (int idx = 0; idx < installation->installationTemplate->maxBatteries; idx++) {
			baseWeapon_t* battery = &installation->batteries[idx];
			aircraftSlot_t* slot = &battery->slot;
			if (slot->delayNextShot > 0)
				slot->delayNextShot -= dt;
			if (slot->ammoLeft <= 0)
				AII_ReloadWeapon(slot);
		}

		if (AII_InstallationCanShoot(installation)) {
			AIRFIGHT_InstallationShoot(installation, installation->batteries, installation->installationTemplate->maxBatteries);
		}
	}
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] parent XML Node structure, where we write the information to
 */
bool AIRFIGHT_SaveXML (xmlNode_t* parent)
{
	for (int i = 0; i < ccs.numProjectiles; i++) {
		aircraftProjectile_t* projectile = &ccs.projectiles[i];
		xmlNode_t* node = cgi->XML_AddNode(parent, SAVE_AIRFIGHT_PROJECTILE);

		cgi->XML_AddString(node, SAVE_AIRFIGHT_ITEMID, projectile->aircraftItem->id);
		for (int j = 0; j < projectile->numProjectiles; j++)
			cgi->XML_AddPos2(node, SAVE_AIRFIGHT_POS, projectile->pos[j]);
		cgi->XML_AddPos3(node, SAVE_AIRFIGHT_IDLETARGET, projectile->idleTarget);

		cgi->XML_AddInt(node, SAVE_AIRFIGHT_TIME, projectile->time);
		cgi->XML_AddFloat(node, SAVE_AIRFIGHT_ANGLE, projectile->angle);
		cgi->XML_AddBoolValue(node, SAVE_AIRFIGHT_BULLET, projectile->bullets);
		cgi->XML_AddBoolValue(node, SAVE_AIRFIGHT_BEAM, projectile->beam);

		if (projectile->attackingAircraft) {
			xmlNode_t* attacking =  cgi->XML_AddNode(node, SAVE_AIRFIGHT_ATTACKINGAIRCRAFT);

			cgi->XML_AddBoolValue(attacking, SAVE_AIRFIGHT_ISUFO, projectile->attackingAircraft->type == AIRCRAFT_UFO);
			if (projectile->attackingAircraft->type == AIRCRAFT_UFO)
				cgi->XML_AddInt(attacking, SAVE_AIRFIGHT_AIRCRAFTIDX, UFO_GetGeoscapeIDX(projectile->attackingAircraft));
			else
				cgi->XML_AddInt(attacking, SAVE_AIRFIGHT_AIRCRAFTIDX, projectile->attackingAircraft->idx);
		}
		cgi->XML_AddPos3(node, SAVE_AIRFIGHT_ATTACKERPOS, projectile->attackerPos);

		if (projectile->aimedAircraft) {
			xmlNode_t* aimed =  cgi->XML_AddNode(node, SAVE_AIRFIGHT_AIMEDAIRCRAFT);

			cgi->XML_AddBoolValue(aimed, SAVE_AIRFIGHT_ISUFO, projectile->aimedAircraft->type == AIRCRAFT_UFO);
			if (projectile->aimedAircraft->type == AIRCRAFT_UFO)
				cgi->XML_AddInt(aimed, SAVE_AIRFIGHT_AIRCRAFTIDX, UFO_GetGeoscapeIDX(projectile->aimedAircraft));
			else
				cgi->XML_AddInt(aimed, SAVE_AIRFIGHT_AIRCRAFTIDX, projectile->aimedAircraft->idx);
		}
	}

	return true;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] parent XML Node structure, where we get the information from
 */
bool AIRFIGHT_LoadXML (xmlNode_t* parent)
{
	int i;
	xmlNode_t* node;

	for (i = 0, node = cgi->XML_GetNode(parent, SAVE_AIRFIGHT_PROJECTILE); i < MAX_PROJECTILESONGEOSCAPE && node;
			node = cgi->XML_GetNextNode(node, parent, SAVE_AIRFIGHT_PROJECTILE), i++) {
		technology_t* tech = RS_GetTechByProvided(cgi->XML_GetString(node, SAVE_AIRFIGHT_ITEMID));
		int j;
		xmlNode_t* positions;
		xmlNode_t* attackingAircraft;
		xmlNode_t* aimedAircraft;
		aircraftProjectile_t* projectile = &ccs.projectiles[i];

		if (!tech) {
			Com_Printf("AIR_Load: Could not get technology of projectile %i\n", i);
			return false;
		}

		projectile->aircraftItem = INVSH_GetItemByID(tech->provides);

		for (j = 0, positions = cgi->XML_GetPos2(node, SAVE_AIRFIGHT_POS, projectile->pos[0]); j < MAX_MULTIPLE_PROJECTILES && positions;
			j++, positions = cgi->XML_GetNextPos2(positions, node, SAVE_AIRFIGHT_POS, projectile->pos[j]))
			;
		projectile->numProjectiles = j;
		cgi->XML_GetPos3(node, SAVE_AIRFIGHT_IDLETARGET, projectile->idleTarget);

		projectile->time = cgi->XML_GetInt(node, SAVE_AIRFIGHT_TIME, 0);
		projectile->angle = cgi->XML_GetFloat(node, SAVE_AIRFIGHT_ANGLE, 0.0);
		projectile->bullets = cgi->XML_GetBool(node, SAVE_AIRFIGHT_BULLET, false);
		projectile->beam = cgi->XML_GetBool(node, SAVE_AIRFIGHT_BEAM, false);

		if ((attackingAircraft = cgi->XML_GetNode(node, SAVE_AIRFIGHT_ATTACKINGAIRCRAFT))) {
			if (cgi->XML_GetBool(attackingAircraft, SAVE_AIRFIGHT_ISUFO, false))
				/** @todo 0 as default might be incorrect */
				projectile->attackingAircraft = UFO_GetByIDX(cgi->XML_GetInt(attackingAircraft, SAVE_AIRFIGHT_AIRCRAFTIDX, 0));
			else
				projectile->attackingAircraft = AIR_AircraftGetFromIDX(cgi->XML_GetInt(attackingAircraft, SAVE_AIRFIGHT_AIRCRAFTIDX, AIRCRAFT_INVALID));
		} else {
			projectile->attackingAircraft = nullptr;
		}
		cgi->XML_GetPos3(node, SAVE_AIRFIGHT_ATTACKERPOS, projectile->attackerPos);

		if ((aimedAircraft = cgi->XML_GetNode(node, SAVE_AIRFIGHT_AIMEDAIRCRAFT))) {
			if (cgi->XML_GetBool(aimedAircraft, SAVE_AIRFIGHT_ISUFO, false))
				/** @todo 0 as default might be incorrect */
				projectile->aimedAircraft = UFO_GetByIDX(cgi->XML_GetInt(aimedAircraft, SAVE_AIRFIGHT_AIRCRAFTIDX, 0));
			else
				projectile->aimedAircraft = AIR_AircraftGetFromIDX(cgi->XML_GetInt(aimedAircraft, SAVE_AIRFIGHT_AIRCRAFTIDX, AIRCRAFT_INVALID));
		} else {
			projectile->aimedAircraft = nullptr;
		}
	}
	ccs.numProjectiles = i;

	return true;
}

/**
 * @sa UI_InitStartup
 */
void AIRFIGHT_InitStartup (void)
{
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_listprojectile", AIRFIGHT_ProjectileList_f, "Print Projectiles information to game console");
#endif
}
