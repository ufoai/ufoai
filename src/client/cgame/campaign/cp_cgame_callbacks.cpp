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

#include "../../cl_shared.h"
#include "../../input/cl_keys.h"
#include "../../ui/ui_dataids.h"
#include "cp_cgame_callbacks.h"
#include "cp_campaign.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "cp_team.h"
#include "cp_geoscape.h"
#include "../../battlescape/cl_hud.h"
#include "cp_mission_callbacks.h"

const cgame_import_t *cgi;

/**
 * @sa CL_ParseResults
 * @sa CP_ParseCharacterData
 * @sa CL_GameAbort_f
 */
static void GAME_CP_Results_f (void)
{
	mission_t *mission = GEO_GetSelectedMission();
	battleParam_t *bp = &ccs.battleParameters;

	if (!mission)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <won:true|false> [retry:true|false]\n", cgi->Cmd_Argv(0));
		return;
	}

	/* check for retry */
	if (cgi->Cmd_Argc() >= 3 && Com_ParseBoolean(cgi->Cmd_Argv(2))) {
		if (bp->retriable) {
			/* don't collect things and update stats --- we retry the mission */
			CP_StartSelectedMission();
			return;
		} else {
			Com_Printf("Battle cannot be retried!\n");
		}
	}

	CP_MissionEnd(ccs.curCampaign, mission, bp, Com_ParseBoolean(cgi->Cmd_Argv(1)));
}

/**
 * @brief Translate the difficulty int to a translated string
 * @param[in] difficulty The difficulty level of the game
 */
static inline const char* CP_ToDifficultyName (const int difficulty)
{
	switch (difficulty) {
	case -4:
		return _("Chicken-hearted");
	case -3:
		return _("Very Easy");
	case -2:
	case -1:
		return _("Easy");
	case 0:
		return _("Normal");
	case 1:
	case 2:
		return _("Hard");
	case 3:
		return _("Very Hard");
	case 4:
		return _("Insane");
	default:
		cgi->Com_Error(ERR_DROP, "Unknown difficulty id %i", difficulty);
	}
}

/**
 * @brief Fill the campaign list with available campaigns
 */
static void GAME_CP_GetCampaigns_f (void)
{
	int i;
	uiNode_t *campaignOption = NULL;

	for (i = 0; i < ccs.numCampaigns; i++) {
		const campaign_t *c = &ccs.campaigns[i];
		if (c->visible)
			cgi->UI_AddOption(&campaignOption, "", va("_%s", c->name), c->id);
	}

	cgi->UI_RegisterOption(OPTION_CAMPAIGN_LIST, campaignOption);
}

#define MAXCAMPAIGNTEXT 4096
static char campaignDesc[MAXCAMPAIGNTEXT];
/**
 * @brief Script function to show description of the selected a campaign
 */
static void GAME_CP_CampaignDescription_f (void)
{
	const char *racetype;
	const campaign_t *campaign;

	if (cgi->Cmd_Argc() < 2 || Q_streq(cgi->Cmd_Argv(1), "")) {
		if (ccs.numCampaigns > 0)
			campaign = &ccs.campaigns[0];
		else
			campaign = NULL;
	} else {
		campaign = CP_GetCampaign(cgi->Cmd_Argv(1));
	}

	if (!campaign) {
		Com_Printf("Invalid Campaign id: %s\n", cgi->Cmd_Argv(1));
		return;
	}

	/* Make sure that this campaign is selected */
	cgi->Cvar_Set("cp_campaign", campaign->id);

	if (campaign->team == TEAM_PHALANX)
		racetype = _("Human");
	else
		racetype = _("Aliens");

	const char *text = cgi->CL_Translate(campaign->text);
	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("%s\n\nRace: %s\nRecruits: %i %s, %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"), _(campaign->name), racetype,
		campaign->soldiers, ngettext("soldier", "soldiers", campaign->soldiers),
		campaign->scientists, ngettext("scientist", "scientists", campaign->scientists),
		campaign->workers, ngettext("worker", "workers", campaign->workers),
		campaign->pilots, ngettext("pilot", "pilots", campaign->pilots),
		campaign->credits, CP_ToDifficultyName(campaign->difficulty),
		(int)(round(campaign->minhappiness * 100.0f)), campaign->negativeCreditsUntilLost,
		text);
	cgi->UI_RegisterText(TEXT_STANDARD, campaignDesc);
}

/**
 * @brief Starts a new single-player game
 * @sa CP_CampaignInit
 * @sa CP_InitMarket
 */
static void GAME_CP_Start_f (void)
{
	campaign_t *campaign;

	campaign = CP_GetCampaign(cgi->Cmd_Argv(1));
	if (!campaign) {
		Com_Printf("Invalid Campaign id: %s\n", cgi->Cmd_Argv(1));
		return;
	}

	/* Make sure that this campaign is selected */
	cgi->Cvar_Set("cp_campaign", campaign->id);

	CP_CampaignInit(campaign, false);

	/* Intro sentences */
	cgi->Cbuf_AddText("seq_start intro\n");
}

static inline void AL_AddAlienTypeToAircraftCargo_ (void *data, const teamDef_t *teamDef, int amount, bool dead)
{
	AL_AddAlienTypeToAircraftCargo((aircraft_t *) data, teamDef, amount, dead);
}

/**
 * @brief After a mission was finished this function is called
 * @param msg The network message buffer
 * @param winner The winning team
 * @param numSpawned The amounts of all spawned actors per team
 * @param numAlive The amount of survivors per team
 * @param numKilled The amount of killed actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 * @param numStunned The amount of stunned actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 */
void GAME_CP_Results (dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], bool nextmap)
{
	int i, j;
	int ownSurvived, ownKilled, ownStunned;
	int aliensSurvived, aliensKilled, aliensStunned;
	int civiliansSurvived, civiliansKilled, civiliansStunned;
	const int currentTeam = cgi->GAME_GetCurrentTeam();
	const bool won = (winner == currentTeam);
	const bool draw = (winner == -1 || winner == 0);
	missionResults_t *results;
	aircraft_t *aircraft = GEO_GetMissionAircraft();
	battleParam_t *bp = &ccs.battleParameters;

	CP_ParseCharacterData(msg, &ccs.updateCharacters);

	ownSurvived = ownKilled = ownStunned = 0;
	aliensSurvived = aliensKilled = aliensStunned = 0;
	civiliansSurvived = civiliansKilled = civiliansStunned = 0;

	for (i = 0; i <= MAX_TEAMS; i++) {
		if (i == currentTeam)
			ownSurvived = numAlive[i];
		else if (i == TEAM_CIVILIAN)
			civiliansSurvived = numAlive[i];
		else if (i < MAX_TEAMS)
			aliensSurvived += numAlive[i];
		for (j = 0; j < MAX_TEAMS; j++)
			if (j == currentTeam) {
				ownKilled += numKilled[i][j];
				ownStunned += numStunned[i][j]++;
			} else if (j == TEAM_CIVILIAN) {
				civiliansKilled += numKilled[i][j];
				civiliansStunned += numStunned[i][j]++;
			} else {
				aliensKilled += numKilled[i][j];
				aliensStunned += numStunned[i][j]++;
			}
	}
	/* if we won, our stunned are alive */
	if (won) {
		ownSurvived += ownStunned;
		ownStunned = 0;
	} else {
		/* if we lost, they revive stunned */
		aliensStunned = 0;
	}

	/* we won, and we're not the dirty aliens */
	if (won)
		civiliansSurvived += civiliansStunned;
	else
		civiliansKilled += civiliansStunned;

	/* Collect items from the battlefield. */
	AII_CollectingItems(aircraft, won);
	if (won)
		/* Collect aliens from the battlefield. */
		cgi->CollectAliens(aircraft, AL_AddAlienTypeToAircraftCargo_);

	ccs.aliensKilled += aliensKilled;

	results = &ccs.missionResults;
	results->mission = bp->mission;

	if (nextmap) {
		assert(won || draw);
		results->aliensKilled += aliensKilled;
		results->aliensStunned += aliensStunned;
		results->aliensSurvived += aliensSurvived;
		results->ownKilled += ownKilled - numKilled[currentTeam][currentTeam] - numKilled[TEAM_CIVILIAN][currentTeam];
		results->ownStunned += ownStunned;
		results->ownKilledFriendlyFire += numKilled[currentTeam][currentTeam] + numKilled[TEAM_CIVILIAN][currentTeam];
		results->ownSurvived += ownSurvived;
		results->civiliansKilled += civiliansKilled;
		results->civiliansKilledFriendlyFire += numKilled[currentTeam][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
		results->civiliansSurvived += civiliansSurvived;
		return;
	}

	/* won mission cannot be retried */
	if (won)
		bp->retriable = false;

	if (won)
		results->state = WON;
	else if (draw)
		results->state = DRAW;
	else
		results->state = LOST;
	results->aliensKilled = aliensKilled;
	results->aliensStunned = aliensStunned;
	results->aliensSurvived = aliensSurvived;
	results->ownKilled = ownKilled - numKilled[currentTeam][currentTeam] - numKilled[TEAM_CIVILIAN][currentTeam];
	results->ownStunned = ownStunned;
	results->ownKilledFriendlyFire = numKilled[currentTeam][currentTeam] + numKilled[TEAM_CIVILIAN][currentTeam];
	results->ownSurvived = ownSurvived;
	results->civiliansKilled = civiliansKilled;
	results->civiliansKilledFriendlyFire = numKilled[currentTeam][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
	results->civiliansSurvived = civiliansSurvived;

	MIS_InitResultScreen(results);
	if (ccs.missionResultCallback) {
		ccs.missionResultCallback(results);
	}

	cgi->UI_InitStack("geoscape", "campaign_main", true, true);

	if (won)
		cgi->UI_PushWindow("won");
	else if (draw)
		cgi->UI_PushWindow("draw");
	else
		cgi->UI_PushWindow("lost");

	if (bp->retriable)
		cgi->UI_ExecuteConfunc("enable_retry");

	cgi->CL_Disconnect();
	cgi->SV_Shutdown("Mission end", false);
}

bool GAME_CP_Spawn (linkedList_t **chrList)
{
	aircraft_t *aircraft = GEO_GetMissionAircraft();
	base_t *base;

	if (!aircraft)
		return false;

	/* convert aircraft team to character list */
	LIST_Foreach(aircraft->acTeam, employee_t, employee) {
		LIST_AddPointer(chrList, (void*)&employee->chr);
	}

	base = aircraft->homebase;

	/* clean temp inventory */
	CP_CleanTempInventory(base);

	/* activate hud */
	HUD_InitUI(NULL, false);

	return true;
}

bool GAME_CP_ItemIsUseable (const objDef_t *od)
{
	const technology_t *tech = RS_GetTechForItem(od);
	return RS_IsResearched_ptr(tech);
}

/**
 * @brief Checks whether the team is known at this stage already
 * @param[in] teamDef The team definition of the alien team
 * @return @c true if known, @c false otherwise.
 */
bool GAME_CP_TeamIsKnown (const teamDef_t *teamDef)
{
	if (!CHRSH_IsTeamDefAlien(teamDef))
		return true;

	if (!ccs.teamDefTechs[teamDef->idx])
		cgi->Com_Error(ERR_DROP, "Could not find tech for teamdef '%s'", teamDef->id);

	return RS_IsResearched_ptr(ccs.teamDefTechs[teamDef->idx]);
}

/**
 * @brief Returns the currently selected character.
 * @return The selected character or @c NULL.
 */
character_t* GAME_CP_GetSelectedChr (void)
{
	employee_t *employee = E_GetEmployeeFromChrUCN( cgi->Cvar_GetInteger("mn_ucn"));
	if (employee)
		return &employee->chr;
	return NULL;
}

void GAME_CP_Drop (void)
{
	/** @todo maybe create a savegame? */
	cgi->UI_InitStack("geoscape", "campaign_main", true, true);

	SV_Shutdown("Mission end", false);
	cgi->CL_Disconnect();
}

void GAME_CP_Frame (float secondsSinceLastFrame)
{
	if (!CP_IsRunning())
		return;

	if (!CP_OnGeoscape())
		return;

	/* advance time */
	CP_CampaignRun(ccs.curCampaign, secondsSinceLastFrame);
}

void GAME_CP_DrawBaseLayout (int baseIdx, int x1, int y1, int totalMarge, int w, int h, int padding, const vec4_t bgcolor, const vec4_t color)
{
	const base_t *base = B_GetBaseByIDX(baseIdx);
	if (base == NULL)
		base = B_GetCurrentSelectedBase();
	int y = y1 + padding;
	for (int row = 0; row < BASE_SIZE; row++) {
		int x = x1 + padding;
		for (int col = 0; col < BASE_SIZE; col++) {
			if (B_IsTileBlocked(base, col, row)) {
				cgi->UI_DrawFill(x, y, w, h, bgcolor);
			} else if (B_GetBuildingAt(base, col, row) != NULL) {
				/* maybe destroyed in the meantime */
				if (base->founded)
					cgi->UI_DrawFill(x, y, w, h, color);
			}
			x += w + padding;
		}
		y += h + padding;
	}
}

void GAME_CP_DrawBaseTooltip (int baseIdx, int x, int y, int col, int row)
{
	const base_t *base = B_GetBaseByIDX(baseIdx);
	if (base == NULL)
		base = B_GetCurrentSelectedBase();
	building_t *building = base->map[row][col].building;
	if (!building)
		return;

	char const* tooltipText = _(building->name);
	if (!B_CheckBuildingDependencesStatus(building))
		tooltipText = va(_("%s\nnot operational, depends on %s"), _(building->name), _(building->dependsBuilding->name));
	const int itemToolTipWidth = 250;
	cgi->UI_DrawTooltip(tooltipText, x, y, itemToolTipWidth);
}

void GAME_CP_DrawBase (int baseIdx, int x, int y, int w, int h, int col, int row, bool hover, int overlap)
{
	const base_t *base = B_GetBaseByIDX(baseIdx);
	if (base == NULL)
		base = B_GetCurrentSelectedBase();
	if (!base) {
		cgi->UI_PopWindow(false);
		return;
	}

	bool used[MAX_BUILDINGS];
	/* reset the used flag */
	OBJZERO(used);

	int baseRow, baseCol;
	for (baseRow = 0; baseRow < BASE_SIZE; baseRow++) {
		const char *image = NULL;
		for (baseCol = 0; baseCol < BASE_SIZE; baseCol++) {
			const vec2_t pos = { x + baseCol * w, y + baseRow * (h - overlap) };
			const building_t *building;
			/* base tile */
			if (B_IsTileBlocked(base, baseCol, baseRow)) {
				building = NULL;
				image = "base/invalid";
			} else if (B_GetBuildingAt(base, baseCol, baseRow) == NULL) {
				building = NULL;
				image = "base/grid";
			} else {
				building = B_GetBuildingAt(base, baseCol, baseRow);
				assert(building);

				if (building->image)
					image = building->image;

				/* some buildings are drawn with two tiles - e.g. the hangar is no square map tile.
				 * These buildings have the needs parameter set to the second building part which has
				 * its own image set, too. We are searching for this second building part here. */
				if (B_BuildingGetUsed(used, building->idx))
					continue;
				B_BuildingSetUsed(used, building->idx);
			}

			/* draw tile */
			if (image != NULL)
				cgi->UI_DrawNormImageByName(false, pos[0], pos[1], w * (building ? building->size[0] : 1), h * (building ? building->size[1] : 1), 0, 0, 0, 0, image);
			if (building) {
				switch (building->buildingStatus) {
				case B_STATUS_UNDER_CONSTRUCTION: {
					const float remaining = B_GetConstructionTimeRemain(building);
					const float time = std::max(0.0f, remaining);
					const char* text = va(ngettext("%3.1f day left", "%3.1f days left", time), time);
					cgi->UI_DrawString("f_small", ALIGN_UL, pos[0] + 10, pos[1] + 10, text);
					break;
				}
				default:
					break;
				}
			}
		}
	}

	/* if we are building */
	if (hover && ccs.baseAction == BA_NEWBUILDING) {
		static const vec4_t white = {1.0f, 1.0f, 1.0f, 0.8f};
		const building_t *building = base->buildingCurrent;
		const vec2_t& size = building->size;
		assert(building);

		for (int i = row; i < row + size[1]; i++) {
			for (int j = col; j < col + size[0]; j++) {
				if (!B_MapIsCellFree(base, j, i))
					return;
			}
		}

		const int xCoord = x + col * w;
		const int yCoord = y + row * (h - overlap);
		const int widthRect = size[0] * w;
		const int heigthRect = size[1] * (h - overlap);
		cgi->UI_DrawRect(xCoord, yCoord, widthRect, heigthRect, white, 3, 1);
	}
}

void GAME_CP_HandleBaseClick (int baseIdx, int key, int col, int row)
{
	base_t *base = B_GetBaseByIDX(baseIdx);
	if (base == NULL)
		base = B_GetCurrentSelectedBase();
	if (base == NULL)
		return;
	const building_t *entry = base->map[row][col].building;
	if (key == K_MOUSE3) {
		if (entry) {
			assert(!B_IsTileBlocked(base, col, row));
			B_DrawBuilding(entry);
		}
	} else if (key == K_MOUSE2) {
		if (entry) {
			cgi->Cmd_ExecuteString(va("building_destroy %i %i", base->idx, entry->idx));
		}
	} else if (key == K_MOUSE1) {
		if (ccs.baseAction == BA_NEWBUILDING) {
			const building_t *building = base->buildingCurrent;

			assert(building);

			if (col + building->size[0] > BASE_SIZE)
				return;
			if (row + building->size[1] > BASE_SIZE)
				return;
			for (int y = row; y < row + building->size[1]; y++)
				for (int x = col; x < col + building->size[0]; x++)
					if (B_GetBuildingAt(base, x, y) != NULL || B_IsTileBlocked(base, x, y))
						return;
			B_SetBuildingByClick(base, building, row, col);
			cgi->S_StartLocalSample("geoscape/build-place", 1.0f);
			return;
		}

		const building_t *entry = B_GetBuildingAt(base, col, row);
		if (entry != NULL) {
			if (B_IsTileBlocked(base, col, row))
				cgi->Com_Error(ERR_DROP, "tile with building is not blocked");

			B_BuildingOpenAfterClick(entry);
			ccs.baseAction = BA_NONE;
			return;
		}
	}
}

const char* GAME_CP_GetTeamDef (void)
{
	const int team = ccs.curCampaign->team;
	return cgi->Com_ValueToStr(&team, V_TEAM, 0);
}

void GAME_CP_InitMissionBriefing (const char **title, linkedList_t **victoryConditionsMsgIDs, linkedList_t **missionBriefingMsgIDs)
{
	const battleParam_t *bp = &ccs.battleParameters;
	const mission_t *mission = bp->mission;
	const mapDef_t *md = mission->mapDef;
	if (Q_strvalid(md->victoryCondition)) {
		cgi->LIST_AddString(victoryConditionsMsgIDs, md->victoryCondition);
	}
	if (mission->crashed) {
		cgi->LIST_AddString(missionBriefingMsgIDs, "*msgid:mission_briefing_crashsite");
	}
	if (Q_strvalid(md->missionBriefing)) {
		cgi->LIST_AddString(missionBriefingMsgIDs, md->missionBriefing);
	}
	if (Q_strvalid(md->description)) {
		*title = md->description;
	}
}

/**
 * @brief Changes some actor states for a campaign game
 * @param team The team to change the states for
 */
dbuffer *GAME_CP_InitializeBattlescape (const linkedList_t *team)
{
	const int teamSize = LIST_Count(team);
	dbuffer *msg = new dbuffer(2 + teamSize * 10);

	NET_WriteByte(msg, clc_initactorstates);
	NET_WriteByte(msg, teamSize);

	LIST_Foreach(team, character_t, chr) {
		NET_WriteShort(msg, chr->ucn);
		NET_WriteShort(msg, chr->state);
		NET_WriteShort(msg, chr->RFmode.getHand());
		NET_WriteShort(msg, chr->RFmode.getFmIdx());
		NET_WriteShort(msg, chr->RFmode.weapon != NULL ? chr->RFmode.weapon->idx : NONE);
	}

	return msg;
}

equipDef_t *GAME_CP_GetEquipmentDefinition (void)
{
	return &ccs.eMission;
}

void GAME_CP_CharacterCvars (const character_t *chr)
{
	/* Display rank if the character has one. */
	if (chr->score.rank >= 0) {
		char buf[MAX_VAR];
		const rank_t *rank = CL_GetRankByIdx(chr->score.rank);
		Com_sprintf(buf, sizeof(buf), _("Rank: %s"), _(rank->name));
		cgi->Cvar_Set("mn_chrrank", buf);
		cgi->Cvar_Set("mn_chrrank_img", rank->image);
		Com_sprintf(buf, sizeof(buf), "%s ", _(rank->shortname));
		cgi->Cvar_Set("mn_chrrankprefix", buf);
	} else {
		cgi->Cvar_Set("mn_chrrank_img", "");
		cgi->Cvar_Set("mn_chrrank", "");
		cgi->Cvar_Set("mn_chrrankprefix", "");
	}

	cgi->Cvar_Set("mn_chrmis", va("%i", chr->score.assignedMissions));
	cgi->Cvar_Set("mn_chrkillalien", va("%i", chr->score.kills[KILLED_ENEMIES]));
	cgi->Cvar_Set("mn_chrkillcivilian", va("%i", chr->score.kills[KILLED_CIVILIANS]));
	cgi->Cvar_Set("mn_chrkillteam", va("%i", chr->score.kills[KILLED_TEAM]));
}

const char* GAME_CP_GetItemModel (const char *string)
{
	const aircraft_t *aircraft = AIR_GetAircraftSilent(string);
	if (aircraft) {
		assert(aircraft->tech);
		return aircraft->tech->mdl;
	} else {
		const technology_t *tech = RS_GetTechByProvided(string);
		if (tech)
			return tech->mdl;
		return NULL;
	}
}

void GAME_CP_InitStartup (void)
{
	cgi->Cmd_AddCommand("cp_results", GAME_CP_Results_f, "Parses and shows the game results");
	cgi->Cmd_AddCommand("cp_getdescription", GAME_CP_CampaignDescription_f, NULL);
	cgi->Cmd_AddCommand("cp_getcampaigns", GAME_CP_GetCampaigns_f, "Fill the campaign list with available campaigns");
	cgi->Cmd_AddCommand("cp_start", GAME_CP_Start_f, "Start the new campaign");

	CP_InitStartup();

	cp_start_employees = cgi->Cvar_Get("cp_start_employees", "1", CVAR_ARCHIVE, "Start with hired employees");
	/* cvars might have been changed by other gametypes */
	cgi->Cvar_ForceSet("sv_maxclients", "1");
	cgi->Cvar_ForceSet("sv_ai", "1");

	/* reset campaign data */
	CP_ResetCampaignData();
	CP_ParseCampaignData();
}

void GAME_CP_Shutdown (void)
{
	cgi->Cmd_RemoveCommand("cp_results");
	cgi->Cmd_RemoveCommand("cp_getdescription");
	cgi->Cmd_RemoveCommand("cp_getcampaigns");
	cgi->Cmd_RemoveCommand("cp_start");

	CP_Shutdown();

	CP_ResetCampaignData();

	cgi->SV_Shutdown("Quitting server.", false);
}
