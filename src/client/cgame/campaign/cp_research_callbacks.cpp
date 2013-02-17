/**
 * @file
 * @brief Menu related functions for research.
 *
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
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_popup.h"
#include "cp_research_callbacks.h"

typedef enum {
	RSGUI_NOTHING,
	RSGUI_RESEARCH,
	RSGUI_BASETITLE,
	RSGUI_BASEINFO,
	RSGUI_RESEARCHOUT,
	RSGUI_MISSINGITEM,
	RSGUI_MISSINGITEMTITLE,
	RSGUI_UNRESEARCHABLEITEM,
	RSGUI_UNRESEARCHABLEITEMTITLE
} guiResearchElementType_t;

typedef struct {
	const base_t *base;
	technology_t *tech;
	guiResearchElementType_t type;
} guiResearchElement_t;

/**
 * @brief A (local) list of displayed technology-entries (the research list in the base)
 */
static guiResearchElement_t researchList2[MAX_RESEARCHLIST + MAX_BASES + MAX_BASES];

/** The number of entries in the above list. */
static int researchListLength;

/** The currently selected entry in the above list. */
static int researchListPos;

/** Number of the available string (in the list) in the research menu */
static int researchListMaxDisplayItems;

/**
 * @brief Displays the informations of the current selected technology in the description-area.
 */
static void RS_UpdateInfo (const base_t* base)
{
	/* reset cvars */
	cgi->Cvar_Set("mn_research_image", "");
	cgi->Cvar_Set("mn_research_model", "");
	cgi->Cvar_Set("mn_researchitemname", "");
	cgi->Cvar_Set("mn_researchitem", "");
	cgi->UI_ResetData(TEXT_STANDARD);

	if (researchListLength <= 0 || researchListPos >= researchListLength)
		return;

	/* selection is not a research */
	/** @todo it can be an assert */
	const int type = researchList2[researchListPos].type;
	if (type != RSGUI_RESEARCH && type != RSGUI_RESEARCHOUT && type != RSGUI_UNRESEARCHABLEITEM)
		return;

	technology_t *tech = researchList2[researchListPos].tech;
	if (tech == NULL)
		return;

	char tmpbuf[128];
	/* Display laboratories limits. */
	Com_sprintf(tmpbuf, sizeof(tmpbuf), _("Laboratory space (used/all): %i/%i"),
		CAP_GetCurrent(base, CAP_LABSPACE), CAP_GetMax(base, CAP_LABSPACE));
	cgi->Cvar_Set("mn_research_labs", tmpbuf);

	/* Store laboratory limits cvars */
	cgi->Cvar_SetValue("mn_max_labspace", CAP_GetMax(base, CAP_LABSPACE));
	cgi->Cvar_SetValue("mn_current_labspace", CAP_GetCurrent(base, CAP_LABSPACE));

	/* Display scientists amounts. */
	Com_sprintf(tmpbuf, sizeof(tmpbuf), _("Scientists (available/all): %i/%i"),
		E_CountUnassigned(base, EMPL_SCIENTIST),
		E_CountHired(base, EMPL_SCIENTIST));
	cgi->Cvar_Set("mn_research_scis", tmpbuf);

	/* Store scientist limits cvars */
	cgi->Cvar_SetValue("mn_scientists_hired", E_CountHired(base, EMPL_SCIENTIST));
	cgi->Cvar_SetValue("mn_scientists_unhired", E_CountUnhired(EMPL_SCIENTIST));
	cgi->Cvar_SetValue("mn_scientists_available", E_CountUnassigned(base, EMPL_SCIENTIST));

	cgi->Cvar_Set("mn_research_selbase", _("Not researched in any base."));

	/* Display the base this tech is researched in. */
	if (tech->scientists > 0) {
		assert(tech->base);
		if (tech->base != base)
			cgi->Cvar_Set("mn_research_selbase", va(_("Researched in %s."), tech->base->name));
		else
			cgi->Cvar_Set("mn_research_selbase", _("Researched in this base."));
	}

	cgi->Cvar_Set("mn_research_selname", _(tech->name));
	if (tech->overallTime > 0.0) {
		if (tech->time > tech->overallTime) {
			Com_Printf("RS_UpdateInfo: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", tech->id, tech->time,
					tech->overallTime);
			/* just in case the values got messed up */
			tech->time = tech->overallTime;
		}
		cgi->Cvar_SetValue("mn_research_seltimebar", 100 - (tech->time * 100 / tech->overallTime));
		cgi->Cvar_Set("mn_research_seltime", va(_("Progress: %.1f%%"), 100 - (tech->time * 100 / tech->overallTime)));
	} else {
		cgi->Cvar_SetValue("mn_research_seltimebar", 0);
		cgi->Cvar_Set("mn_research_seltime", _("Progress: not available."));
	}

	switch (tech->statusResearch) {
	case RS_RUNNING:
		cgi->Cvar_Set("mn_research_selstatus", _("Status: under research"));
		cgi->Cvar_Set("mn_research_selstatus_long", _("Status: this research topic is currently being processed in laboratories"));
		break;
	case RS_PAUSED:
		cgi->Cvar_Set("mn_research_selstatus", _("Status: research paused"));
		cgi->Cvar_Set("mn_research_selstatus_long", _("Status: research topic currently paused"));
		break;
	case RS_FINISH:
		cgi->Cvar_Set("mn_research_selstatus", _("Status: research finished"));
		cgi->Cvar_Set("mn_research_selstatus_long", _("Status: research finished"));
		break;
	case RS_NONE:
		if (tech->statusCollected && !tech->statusResearchable) {
			/** @sa RS_UpdateData -> "--" */
			cgi->Cvar_Set("mn_research_selstatus", _("Status: not possible to research"));
			cgi->Cvar_Set("mn_research_selstatus_long", _("Status: the materials or background knowledge needed to research this topic are not available yet"));
		} else {
			cgi->Cvar_Set("mn_research_selstatus", _("Status: unknown technology"));
			cgi->Cvar_Set("mn_research_selstatus_long", _("Status: unknown technology"));
		}
		break;
	default:
		break;
	}

	/* Set image cvar. */
	if (tech->image)
		cgi->Cvar_Set("mn_research_image", tech->image);

	/* Set model cvar. */
	if (tech->mdl)
		cgi->Cvar_Set("mn_research_model", tech->mdl);
}

/**
 * @brief Update the research status of a row
 */
static void RS_UpdateResearchStatus (int row)
{
	guiResearchElement_t *element = &researchList2[row];
	assert(element->type == RSGUI_RESEARCH || element->type == RSGUI_RESEARCHOUT);

	switch (element->tech->statusResearch) {
	case RS_RUNNING:
		/* Color the item with 'research running'-color. */
		cgi->UI_ExecuteConfunc("research_running %i", row);
		break;
	case RS_PAUSED:
		/* Color the item with 'research paused'-color. */
		cgi->UI_ExecuteConfunc("research_paused %i", row);
		break;
	case RS_NONE:
		/* Color the item with 'research normal'-color. */
		cgi->UI_ExecuteConfunc("research_normal %i", row);
		break;
	case RS_FINISH:
	default:
		break;
	}
}

/**
 * @brief Initialize/Update all the GUI according to the current view
 * @param[in] base Pointer to the base where item list is updated
 * @param[in] update If true, only update editable content
 * @note See menu_research.ufo for the layout/called functions.
 * @todo Display free space in all labs in the current base for each item.
 */
static void RS_InitGUI (base_t* base, bool update)
{
	int i = 0;
	int available[MAX_BASES];

	assert(base);

	for (i = 0; i < MAX_BASES; i++) {
		const base_t *b = B_GetFoundedBaseByIDX(i);
		if (!b)
			continue;
		available[i] = E_CountUnassigned(b, EMPL_SCIENTIST);
	}

	for (i = 0; i < researchListMaxDisplayItems && i < researchListLength; i++) {
		guiResearchElement_t *element = &researchList2[i];

		/* only element of the current base can change */
		if (update && (element->base != base && element->type != RSGUI_RESEARCHOUT))
			continue;

		switch (element->type) {
		case RSGUI_NOTHING:
			cgi->UI_ExecuteConfunc("research_hide %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), "");
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		case RSGUI_RESEARCH:
			{
				const int value = element->tech->scientists;
				const int max = available[element->base->idx] + element->tech->scientists;
				cgi->UI_ExecuteConfunc("research_research %i", i);
				if (!update) {
					cgi->Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
				}
				cgi->UI_ExecuteConfunc("research_updateitem %i %i %i", i, value, max);
				/* How many scis are assigned to this tech. */
				cgi->Cvar_SetValue(va("mn_researchassigned%i", i), element->tech->scientists);
				if (element->tech->overallTime > 0.0) {
					float percentage;
					if (element->tech->time > element->tech->overallTime) {
						Com_Printf("RS_InitGUI: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", element->tech->id, element->tech->time,
							element->tech->overallTime);
						/* just in case the values got messed up */
						element->tech->time = element->tech->overallTime;
					}
					percentage = 100 - (element->tech->time * 100 / element->tech->overallTime);
					if (percentage > 0) {
						cgi->Cvar_Set(va("mn_rsstatus%i", i), va("%.1f%%", percentage));
					} else {
						cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
					}
				} else {
					cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
				}
				RS_UpdateResearchStatus(i);
			}
			break;
		case RSGUI_BASETITLE:
			cgi->UI_ExecuteConfunc("research_basetitle %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), element->base->name);
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		case RSGUI_BASEINFO:
			cgi->UI_ExecuteConfunc("research_baseinfo %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), _("Unassigned scientists"));
			/* How many scis are unassigned */
			cgi->Cvar_SetValue(va("mn_researchassigned%i", i), available[element->base->idx]);
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		case RSGUI_RESEARCHOUT:
			cgi->UI_ExecuteConfunc("research_outterresearch %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
			/* How many scis are assigned to this tech. */
			cgi->Cvar_SetValue(va("mn_researchassigned%i", i), element->tech->scientists);
			if (element->tech->overallTime > 0.0) {
				float percentage;
				if (element->tech->time > element->tech->overallTime) {
					Com_Printf("RS_InitGUI: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", element->tech->id, element->tech->time,
						element->tech->overallTime);
					/* just in case the values got messed up */
					element->tech->time = element->tech->overallTime;
				}
				percentage = 100 - (element->tech->time * 100 / element->tech->overallTime);
				if (percentage > 0) {
					cgi->Cvar_Set(va("mn_rsstatus%i", i), va("%.1f%%", percentage));
				} else {
					cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
				}
			} else {
				cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			}
			RS_UpdateResearchStatus(i);
			break;
		case RSGUI_MISSINGITEM:
			cgi->UI_ExecuteConfunc("research_missingitem %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		case RSGUI_MISSINGITEMTITLE:
			cgi->UI_ExecuteConfunc("research_missingitemtitle %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), _("Missing an artifact"));
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		case RSGUI_UNRESEARCHABLEITEM:
			cgi->UI_ExecuteConfunc("research_unresearchableitem %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), _(element->tech->name));
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		case RSGUI_UNRESEARCHABLEITEMTITLE:
			cgi->UI_ExecuteConfunc("research_unresearchableitemtitle %i", i);
			cgi->Cvar_Set(va("mn_researchitem%i", i), _("Unresearchable collected items"));
			cgi->Cvar_Set(va("mn_rsstatus%i", i), "");
			break;
		default:
			assert(false);
		}
	}

	/* Set rest of the list-entries to have no text at all. */
	if (!update) {
		for (; i < researchListMaxDisplayItems; i++) {
			cgi->UI_ExecuteConfunc("research_hide %i", i);
		}
	}

	/* Select last selected item if possible or the very first one if not. */
	if (researchListLength) {
		Com_DPrintf(DEBUG_CLIENT, "RS_UpdateData: Pos%i Len%i\n", researchListPos, researchListLength);
		if (researchListPos >= 0 && researchListPos < researchListLength && researchListLength < researchListMaxDisplayItems) {
			const int t = researchList2[researchListPos].type;
			/* is it a tech row */
			if (t == RSGUI_RESEARCH || t == RSGUI_RESEARCHOUT || t == RSGUI_UNRESEARCHABLEITEM) {
				cgi->UI_ExecuteConfunc("research_selected %i", researchListPos);
			}
		}
	}

	/* Update the description field/area. */
	RS_UpdateInfo(base);
}

/**
 * @brief Changes the active research-list entry to the currently selected.
 * See menu_research.ufo for the layout/called functions.
 */
static void CL_ResearchSelect_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= researchListLength) {
		cgi->UI_ResetData(TEXT_STANDARD);
		return;
	}

	const int type = researchList2[num].type;

	/* switch to another team */
	if (type == RSGUI_BASETITLE) {
		const base_t *b = researchList2[num].base;
		if (b != NULL && b != base) {
			cgi->UI_ExecuteConfunc("research_changebase %i %i", b->idx, researchListPos);
			return;
		}
	}

	if (type != RSGUI_RESEARCH && type != RSGUI_RESEARCHOUT && type != RSGUI_UNRESEARCHABLEITEM)
		return;

	/* update the selected row */
	researchListPos = num;

	/** @todo improve that, don't need to update everything */
	/* need to set previous selected tech to proper color */
	RS_InitGUI(base, true);
}

/**
 * @brief Script function to add and remove a scientist to  the technology entry in the research-list.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist_f
 * @todo use the diff as a value, not only as a +1 or -1
 */
static void RS_ChangeScientist_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <num_in_list> <diff>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	const int diff = atoi(cgi->Cmd_Argv(2));
	if (diff == 0)
		return;

	Com_DPrintf(DEBUG_CLIENT, "RS_ChangeScientist_f: num %i, diff %i\n", num, diff);
	if (diff > 0) {
		RS_AssignScientist(researchList2[num].tech, base);
	} else {
		RS_RemoveScientist(researchList2[num].tech, NULL);
	}

	/* Update display-list and display-info. */
	RS_InitGUI(base, true);
}

/**
 * @brief Assign as many scientists to the research project as possible.
 * @param[in] base The base the tech is researched in.
 * @param[in] tech The technology you want to max out.
 * @sa RS_AssignScientist
 */
static void RS_MaxOutResearch (base_t *base, technology_t* tech)
{
	if (!base || !tech)
		return;

	assert(tech->scientists >= 0);

	/* Add as many scientists as possible to this tech. */
	while (CAP_GetFreeCapacity(base, CAP_LABSPACE) > 0) {
		const employee_t *employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
		if (!employee)
			break;
		RS_AssignScientist(tech, base);
	}
}

/**
 * @brief Script function to add a scientist to the technology entry in the research-list.
 * @sa RS_AssignScientist
 * @sa RS_RemoveScientist_f
 */
static void RS_AssignScientist_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num_in_list>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	Com_DPrintf(DEBUG_CLIENT, "RS_AssignScientist_f: num %i\n", num);
	RS_AssignScientist(researchList2[num].tech, base);

	/* Update display-list and display-info. */
	RS_InitGUI(base, true);
}

/**
 * @brief Script function to remove a scientist from the technology entry in the research-list.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist
 */
static void RS_RemoveScientist_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num_in_list>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= researchListLength)
		return;

	RS_RemoveScientist(researchList2[num].tech, NULL);

	/* Update display-list and display-info. */
	RS_InitGUI(base, true);
}

/**
 * @brief Script binding for RS_UpdateData
 */
static void RS_UpdateData_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	RS_InitGUI(base, true);
}

/**
 * @brief Starts the research of the selected research-list entry.
 */
static void RS_ResearchStart_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	/* We are not in base view. */
	if (!base)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* Check if laboratory is available. */
	if (!B_GetBuildingStatus(base, B_LAB))
		return;

	/* get the currently selected research-item */
	technology_t *tech = researchList2[researchListPos].tech;

	if (!tech)
		return;

	/** @todo If there are enough items add them to the tech (i.e. block them from selling or for other research),
	 * otherwise pop an errormessage telling the player what is missing */
	if (!tech->statusResearchable) {
		Com_DPrintf(DEBUG_CLIENT, "RS_ResearchStart_f: %s was not researchable yet. re-checking\n", tech->id);
		/* If all requirements are met (includes a check for "enough-collected") mark this tech as researchable.*/
		if (RS_RequirementsMet(tech, base))
			RS_MarkOneResearchable(tech);
		RS_MarkResearchable(base);	/* Re-check all other techs in case they depend on the marked one. */
	}

	/* statusResearchable might have changed - check it again */
	if (tech->statusResearchable) {
		switch (tech->statusResearch) {
		case RS_RUNNING:
			if (tech->base == base) {
				/* Research already running in current base ... try to add max amount of scis. */
				RS_MaxOutResearch(base, tech);
			}else {
				/* Research already running in another base. */
				CP_Popup(_("Notice"), _("This item is currently under research in another base."));
			}
			break;
		case RS_PAUSED:
		case RS_NONE:
			if (tech->statusResearch == RS_PAUSED) {
				/* CP_Popup(_("Notice"), _("The research on this item continues.")); Removed because it isn't really needed.*/
				Com_Printf("RS_ResearchStart_f: The research on this item continues.\n");
			}
			/* Add as many scientists as possible to this tech. */
			RS_MaxOutResearch(base, tech);

			if (tech->scientists > 0) {
				tech->statusResearch = RS_RUNNING;
			}
			break;
		case RS_FINISH:
			/* Should never be executed. */
			CP_Popup(_("Notice"), _("The research on this item is complete."));
			break;
		default:
			break;
		}
	} else {
		CP_Popup(_("Notice"), _("The research on this item is not yet possible.\nYou need to research the technologies it's based on first."));
	}

	RS_InitGUI(base, true);
}

/**
 * @brief Removes all scientists from the selected research-list entry.
 */
static void RS_ResearchStop_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	/* we are not in base view */
	if (!base)
		return;

	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* Check if laboratory is available. */
	if (!B_GetBuildingStatus(base, B_LAB))
		return;

	/* get the currently selected research-item */
	technology_t *tech = researchList2[researchListPos].tech;

	if (!tech)
		return;

	switch (tech->statusResearch) {
	case RS_RUNNING:
		RS_StopResearch(tech);
		break;
	case RS_PAUSED:
		/** @todo remove? Popup info how much is already researched? */
		/* tech->statusResearch = RS_RUNNING; */
		break;
	case RS_FINISH:
		CP_Popup(_("Notice"), _("The research on this item is complete."));
		break;
	case RS_NONE:
		Com_Printf("Can't pause research. Research not started.\n");
		break;
	default:
		break;
	}
	RS_InitGUI(base, true);
}

/**
 * @brief Switches to the UFOpaedia entry of the currently selected research entry.
 */
static void RS_ShowPedia_f (void)
{
	if (researchListPos < 0 || researchListPos >= researchListLength)
		return;

	/* get the currently selected research-item */
	const technology_t *tech = researchList2[researchListPos].tech;
	if (!tech)
		return;

	if (tech->preDescription.numDescriptions > 0) {
		UP_OpenCopyWith(tech->id);
	} else {
		CP_Popup(_("Notice"), _("No research proposal available for this project."));
	}
}

/**
 * @brief Create a GUI view of the current research in a base
 * @param[in] base Pointer to the base where item list is updated
 * @note call when we open the GUI
 */
static void RS_InitGUIData (base_t* base)
{
	int i, j;
	int row;

	assert(base);

	RS_MarkResearchable(base);

	/* update tech of the base */
	row = 0;
	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);

		/* Don't show technologies with time == 0 - those are NOT separate research topics. */
		if (tech->time == 0)
			continue;

		/* hide finished research */
		if (tech->statusResearch == RS_FINISH)
			continue;

		/* hide tech we can't search */
		if (!tech->statusResearchable)
			continue;

		/* In this base or nowhere */
		if (tech->base != NULL && tech->base != base)
			continue;

		if (!RS_RequirementsMet(tech, base))
			continue;

		/* Assign the current tech in the global list to the correct entry in the displayed list. */
		researchList2[row].tech = tech;
		researchList2[row].base = base;
		researchList2[row].type = RSGUI_RESEARCH;
		row++;
	}
	researchList2[row].base = base;
	researchList2[row].type = RSGUI_BASEINFO;
	row++;

	/* Items collected but not yet researchable. */
	bool first = true;
	for (i = 0; i < ccs.numTechnologies; i++) {
		technology_t *tech = RS_GetTechByIDX(i);

		/* Don't show technologies with time == 0 - those are NOT separate research topics. */
		if (tech->time == 0)
			continue;

		/* hide finished research */
		if (tech->statusResearch == RS_FINISH)
			continue;

		/* Hide searchable or uncollected tech */
		if ((tech->statusResearchable && RS_RequirementsMet(tech, base)) || !tech->statusCollected)
			continue;

		/* title */
		if (first) {
			researchList2[row].type = RSGUI_NOTHING;
			row++;
			researchList2[row].base = base;
			researchList2[row].type = RSGUI_UNRESEARCHABLEITEMTITLE;
			row++;
			first = false;
		}

		/* Assign the current tech in the global list to the correct entry in the displayed list. */
		researchList2[row].tech = tech;
		researchList2[row].base = base;
		researchList2[row].type = RSGUI_UNRESEARCHABLEITEM;
		row++;
	}

	/* research from another bases */
	for (j = 0; j < MAX_BASES; j++) {
		base_t *b = B_GetFoundedBaseByIDX(j);
		if (!b || b == base)
			continue;

		/* skip bases without labs */
		if (B_GetBuildingInBaseByType(b, B_LAB, true) == NULL)
			continue;

		researchList2[row].type = RSGUI_NOTHING;
		row++;
		researchList2[row].type = RSGUI_BASETITLE;
		researchList2[row].base = b;
		row++;

		for (i = 0; i < ccs.numTechnologies; i++) {
			technology_t *tech = RS_GetTechByIDX(i);

			/* Don't show technologies with time == 0 - those are NOT separate research topics. */
			if (tech->time == 0)
				continue;

			if (tech->base != b)
				continue;

			/* hide finished research */
			if (tech->statusResearch == RS_FINISH)
				continue;

			/* hide tech we can't search */
			if (!tech->statusResearchable)
				continue;

			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList2[row].tech = tech;
			researchList2[row].base = b;
			researchList2[row].type = RSGUI_RESEARCHOUT;

			/* counting the numbers of display-list entries. */
			row++;
		}
		researchList2[row].base = b;
		researchList2[row].type = RSGUI_BASEINFO;
		row++;
	}

	/** @todo add missing items, unresearchable items */

	researchListLength = row;
}


/**
 * @brief Checks whether there are items in the research list and there is a base
 * otherwise leave the research menu again
 * @note if there is a base but no lab a popup appears
 * @sa RS_UpdateData
 * @sa cgi->UI_ResearchInit_f
 */
static void CL_ResearchType_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* Update and display the list. */
	RS_InitGUIData(base);

	/* Nothing to research here. */
	/** @todo wrong computation: researchListLength doesn't say if there are research on this base */
	if (!researchListLength) {
		cgi->UI_PopWindow(false);
		CP_Popup(_("Notice"), _("Nothing to research."));
	} else if (!B_GetBuildingStatus(base, B_LAB)) {
		cgi->UI_PopWindow(false);
		CP_Popup(_("Notice"), _("Build a laboratory first."));
	}
}
/**
 * @brief Research menu init function binding
 * @note Should be called whenever the research menu gets active
 * @sa CL_ResearchType_f
 */
static void UI_ResearchInit_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	researchListMaxDisplayItems = 22;
	if (cgi->Cmd_Argc() >= 2) {
		researchListMaxDisplayItems = atoi(cgi->Cmd_Argv(1));
	}

	CL_ResearchType_f();
	RS_InitGUI(base, false);
}

void RS_InitCallbacks (void)
{
	cgi->Cmd_AddCommand("research_init", UI_ResearchInit_f, "Research menu init function binding");
	cgi->Cmd_AddCommand("research_select", CL_ResearchSelect_f, "Update current selection with the one that has been clicked");
	cgi->Cmd_AddCommand("research_update", RS_UpdateData_f, NULL);
	cgi->Cmd_AddCommand("research_type", CL_ResearchType_f, "Switch between different research types");
	cgi->Cmd_AddCommand("mn_rs_add", RS_AssignScientist_f, "Assign one scientist to this entry");
	cgi->Cmd_AddCommand("mn_rs_change", RS_ChangeScientist_f, "Assign or remove scientist from this entry");
	cgi->Cmd_AddCommand("mn_rs_remove", RS_RemoveScientist_f, "Remove one scientist from this entry");
	cgi->Cmd_AddCommand("mn_start_research", RS_ResearchStart_f, "Start the research of the selected entry");
	cgi->Cmd_AddCommand("mn_stop_research", RS_ResearchStop_f, "Pause the research of the selected entry");
	cgi->Cmd_AddCommand("mn_show_ufopedia", RS_ShowPedia_f, "Show the entry in the UFOpaedia for the selected research topic");

	/* reset some static data - this is needed because we can start several
	 * campaign games without restarting ufo. */
	OBJZERO(researchList2);
	researchListLength = 0;
	researchListPos = 0;
}

void RS_ShutdownCallbacks (void)
{
	cgi->Cmd_RemoveCommand("research_init");
	cgi->Cmd_RemoveCommand("research_select");
	cgi->Cmd_RemoveCommand("research_update");
	cgi->Cmd_RemoveCommand("research_type");
	cgi->Cmd_RemoveCommand("mn_rs_add");
	cgi->Cmd_RemoveCommand("mn_rs_change");
	cgi->Cmd_RemoveCommand("mn_rs_remove");
	cgi->Cmd_RemoveCommand("mn_start_research");
	cgi->Cmd_RemoveCommand("mn_stop_research");
	cgi->Cmd_RemoveCommand("mn_show_ufopedia");
}
