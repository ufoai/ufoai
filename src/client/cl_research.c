/*

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

/*======================
cl_research.c

* Handles everything related to the research-tree.
* Provides information if items/buildings/etc.. can be researched/used/displayed etc...
* Implements the research-system (research new items/etc...)

See
	base/ufos/research.ufo
and
	base/ufos/menu_research.ufo
for the underlying content.

TODO: comment on used globasl variables.
======================*/

#include "client.h"

void RS_GetFirstRequired( technology_t* tech,  stringlist_t *required);
qboolean RS_TechIsResearchable( technology_t* t );

technology_t technologies[MAX_TECHNOLOGIES];	// A global listof _all_ technologies (see cl_research.h)
technology_t technologies_skeleton[MAX_TECHNOLOGIES];	// A local listof _all_ technologies  that are used to initialise the global list on new/loaded game.
int numTechnologies;						// The global number of entries in the global list AND the local list above.  (see cl_research.h)

technology_t *researchList[MAX_RESEARCHLIST];	// A (local) list of displayed technology-entries (the research list in the base)
int researchListLength;						// The number of entries in the above list.
int researchListPos;						// The currently selected entry in the above list.


/*======================
RS_MarkOneCollected

Marks one tech as 'collected'  if an item it 'provides' (= id) has been collected.

IN
	id:	unique id of a provided item (can be item/building/craft/etc..)
======================*/
void RS_MarkOneCollected ( char *id )
{
	int i;
	technology_t *t = NULL;


	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !Q_strncmp( t->provides, id, MAX_VAR ) ) {	// provided item found
			t->statusCollected++;
			return;
		}
	}
	Com_Printf( "RS_MarkOneCollected: \"%s\" not found. No tech provides this.\n", id );
}

/*======================
RS_MarkCollected

Marks all techs if an item they 'provide' have been collected.
Should be run after items have been collected/looted from the battlefield (cl_campaign.c -> "CL_CollectItems") and after techtree/inventory init (for all).
======================*/
void RS_MarkCollected ( void )
{
	int i;

	for ( i=0; i < MAX_OBJDEFS; i++ ) {
		if ( ccs.eCampaign.num[i] ) {
			Com_DPrintf("RS_MarkCollected: %s marked as collected.\n", csi.ods[i].kurz );
			RS_MarkOneCollected( csi.ods[i].kurz );
		}
	}
}

/*======================
RS_MarkOneResearchable

Marks one tech as researchedable.

IN
	id:	unique id of a technology_t
======================*/
void RS_MarkOneResearchable ( technology_t* t )
{
	Com_DPrintf("RS_MarkOneResearchable: \"%s\" marked as researchable.\n", t->id );
	t->statusResearchable = qtrue;
}

/*======================
RS_MarkResearchable

Marks all the techs that can be researched.
Should be called when a new item is researched (RS_MarkResearched) and after the tree-initialisation (RS_InitTree)
======================*/
void RS_MarkResearchable( void )
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t firstrequired;
	stringlist_t *required = NULL;
	byte required_are_researched;


	// set all entries to initial value
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		t->statusResearchable = qfalse;
	}

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !t->statusResearchable ) {	// Redundant, since we set them all to false, but you never know.
			if ( t->statusResearch != RS_FINISH) {
				Com_DPrintf("RS_MarkResearchable: handling \"%s\".\n", t->id );
				firstrequired.numEntries = 0;
				RS_GetFirstRequired( t,  &firstrequired );

				// If the tech has an collected item, mark the first-required techs as researchable //TODO doesn't work yet?
				if ( t->statusCollected ) {
					for ( j=0; j < firstrequired.numEntries; j++ ) {
						Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:firstrequired of collected.\n", firstrequired.list[j] );
						RS_MarkOneResearchable( firstrequired.techPtr[j] );
					}
				}

				// if needed/required techs are all researched, mark this as researchable.
				required = &t->requires;
				required_are_researched = qtrue;
				for ( j=0; j < required->numEntries; j++ ) {
					if ( ( !RS_TechIsResearched(required->list[j] ) )
					  || ( !Q_strncmp( required->list[j], "nothing", 7 ) ) ) {
						required_are_researched = qfalse;
						break;
					}
				}

				if ( required_are_researched											// all required items are researched
				&& ( ( t->needsCollected && t->statusCollected ) || !t->needsCollected ) ){	// AND ( all needed collected OR no collected needed )
					Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:required.\n", t->id );

					t->statusResearchable = qtrue;
				}


				// If the tech is an initial one,  mark it as as researchable.
				for ( j=0; j < required->numEntries; j++ ) {
					if ( !Q_strncmp( required->list[j], "initial", 7 ) ) {
						Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable - reason:isinitial.\n", t->id );
						t->statusResearchable = qtrue;
						break;
					}
				}
			}
		}
	}
	Com_DPrintf( "RS_MarkResearchable: Done.\n");
}


/*======================
Com_AddObjectTechs
======================*/
void RS_AddObjectTechs( void )
{
	objDef_t	*od;
	int		i;

	// add weapon link to ammo
	for ( i = 0, od = csi.ods; i < csi.numODs; i++, od++ )
	{
		od->tech = RS_GetTechByProvided( od->kurz );
#ifdef DEBUG
		if ( ! od->tech )
			Sys_Error("Com_AddObjectTechs: Could not find a valid tech for item %s\n", od->kurz );
#endif /* DEBUG */
	}
}

/*======================
RS_CopyFromSkeleton

Copy the research-tree skeleton parsed on game-start to the global list.
The skeleton has all the informations about already researched items etc..
======================*/
void RS_CopyFromSkeleton( void )
{
	int i;
	technology_t *tech = NULL;

	for ( i = 0; i < numTechnologies; i++ ) {
		tech = &technologies[i];
		memcpy( tech, &technologies_skeleton[i], sizeof( technology_t ) );
	}

	// link in the tech pointers
	RS_AddObjectTechs();
}

/*======================
RS_InitTree

Gets all needed names/file-paths/etc... for each technology entry.
Should be executed after the parsing of _all_ the ufo files and e.g. the research tree/inventory/etc... are initialised.

TODO: add a function to reset ALL research-stati to RS_NONE; -> to be called after start of a new game.
======================*/
void RS_InitTree( void )
{
	int i, j, k;
	technology_t *t = NULL;
	objDef_t *item = NULL;
	objDef_t *item_ammo = NULL;
	building_t	*building = NULL;
	aircraft_t	*ac = NULL;
	byte	found;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];

		t->overalltime = t->time;	// set the overall reseach time (now fixed) to the one given in the ufo-file.

		// Search in correct data/.ufo
		switch ( t->type )
		{
		case RS_TECH:
			if ( !*t->name )
				Com_Printf( "RS_InitTree: \"%s\" A 'type tech' item needs to have a 'name\txxx' defined.", t->id );
			break;
		case RS_WEAPON:
		case RS_ARMOR:
			found = qfalse;
			for ( j = 0; j < csi.numODs; j++ ) {
				item = &csi.ods[j];
				if ( !Q_strncmp( t->provides, item->kurz, MAX_VAR ) ) { // This item has been 'provided',
					found = qtrue;
					if ( !*t->name )
						Com_sprintf( t->name, MAX_VAR, item->name );
					if ( !*t->mdl_top )
						Com_sprintf( t->mdl_top, MAX_VAR, item->model );
					if ( !*t->image_top )
						Com_sprintf( t->image_top, MAX_VAR, item->image );
					if ( !*t->mdl_bottom ) {
						if  ( t->type == RS_WEAPON) {
							// find ammo
							for ( k = 0; k < csi.numODs; k++ ) {
								item_ammo = &csi.ods[k];
								if ( j == item_ammo->link ) {
									Com_DPrintf("RS_InitTree: Ammo \"%s\" for \"%s\" found.\n", item_ammo->name,  item->name);
									Com_sprintf( t->mdl_bottom, MAX_VAR, item_ammo->model );
								}
							}
						}
					}
					break;	// Should return to CASE RS_xxx.
				}

			}
			//no id found in csi.ods
			if ( !found ) {
				Com_sprintf( t->name, MAX_VAR, t->id );
				Com_Printf("RS_InitTree: \"%s\" - Linked weapon or armor (provided=\"%s\") not found. Tech-id used as name.\n", t->id, t->provides );
			}
			break;
		case RS_BUILDING:
			found = qfalse;
			for ( j = 0; j < numBuildings; j++ ) {
				building = &bmBuildings[0][j];
				if ( !Q_strncmp( t->provides, building->name, MAX_VAR ) ) { // This building has been 'provided',
					found = qtrue;
					if ( !*t->name )
						Com_sprintf( t->name, MAX_VAR, building->title );
					if ( !*t->image_top )
						Com_sprintf( t->image_top, MAX_VAR, building->image );

					break;	// Should return to CASE RS_xxx.
				}
			}
			if ( !found ){
				Com_sprintf( t->name, MAX_VAR, t->id );
				Com_Printf( "RS_InitTree: \"%s\" - Linked building (provided=\"%s\") not found. Tech-id used as name.\n", t->id, t->provides );
			}
			break;
		case RS_CRAFT:
			found = qfalse;
			for ( j = 0; j < numAircraft; j++ ) {
				ac = &aircraft[j];
				if ( !Q_strncmp( t->provides, ac->title, MAX_VAR ) ) { // This aircraft has been 'provided',
					found = qtrue;
					if ( !*t->name )
						Com_sprintf( t->name, MAX_VAR, ac->name );
					if ( !*t->mdl_top ) {			// DEBUG testing
						Com_sprintf( t->mdl_top, MAX_VAR, ac->model );
						Com_Printf( "RS_InitTree: aircraft model \"%s\" \n",ac->model );
					}
					break;	// Should return to CASE RS_xxx.
				}
			}
			if ( !found )
				Com_Printf( "RS_InitTree: \"%s\" - Linked aircraft or craft-upgrade (provided=\"%s\") not found.\n", t->id, t->provides );
			break;
		case RS_ALIEN:
			/* does nothing right now */
			break;
		} // switch
	}
	RS_MarkCollected();
	RS_MarkResearchable();
	Com_DPrintf( "RS_InitTree: Technology tree initialised. %i entries found.\n", i );
}

/*======================
RS_GetName

Return "name" if present, otherwise enter the correct .ufo file and get it from the definition there.

IN
	id:	unique id of a technology_t

OUT
	name:	Full name of this technology_t (technology_t->name).
			Defaults to id if nothing is found.
======================*/
void RS_GetName( char *id, char *name )
{
	technology_t *tech = NULL;

	tech = RS_GetTechByID( id );
	if ( ! tech ) {
		Com_sprintf( name, MAX_VAR, id );	// set the name to the id.
		return;
	}

	if ( *tech->name ) {
		Com_sprintf( name, MAX_VAR, _(tech->name) );
		return;
	} else {
		// FIXME: Do we need to translate the id?
		Com_sprintf( name, MAX_VAR, _(id) );	// set the name to the id.
		return;
	}
}

/*======================
RS_ResearchDisplayInfo

Displays the informations of the current selected technology in the description-area.
See menu_research.ufo for the layout/called functions.
======================*/
void RS_ResearchDisplayInfo ( void  )
{
	int i;
	technology_t *tech = NULL;
	base_t *base = NULL;
	char dependencies[MAX_VAR];
	char tempstring[MAX_VAR];
	stringlist_t req_temp;

	// we are not in base view
	if ( ! baseCurrent )
		return;

	if ( researchListLength <= 0 )
		return;
	
	tech = researchList[researchListPos];

	// display total number of free labs in current base
	Cvar_Set( "mn_research_labs", va( _("Free labs in this base: %i"), MN_GetUnusedLabs( baseCurrent->id ) ) );
	Cvar_Set( "mn_research_scis", va( _("Available scientists in this base: %i"), MN_EmployeesInBase2 ( baseCurrent->id, EMPL_SCIENTIST, qtrue ) ) );

	Cvar_Set( "mn_research_selbase", _("Not researched in any base.") );
	// display the base this tech is researched in
	if (tech->lab) {
		if ( tech->lab->base != baseCurrent->id ) {
			base = &bmBases[tech->lab->base];
			Cvar_Set( "mn_research_selbase", va( _("Researched in %s"), base->title ) );
		} else {
			Cvar_Set( "mn_research_selbase", _("Researched in this base.") );
		}
	}

	Cvar_Set( "mn_research_selname", tech->name );
	if ( tech->overalltime ) {
		if ( tech->time > tech->overalltime ) {
			Com_Printf("RS_ResearchDisplayInfo: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", tech->id, tech->time, tech->overalltime );
			tech->time = tech->overalltime;	// just in case the values got messed up
		}
		Cvar_Set( "mn_research_seltime", va( _("Progress: %.1f\%"), 100-(tech->time*100/tech->overalltime) ) );
	} else {
		Cvar_Set( "mn_research_seltime", _("Progress: Not available.") );
	}

	switch ( tech->statusResearch )
	{
	case RS_RUNNING:
		Cvar_Set( "mn_research_selstatus", _("Status: Under research") );
		break;
	case RS_PAUSED:
		Cvar_Set( "mn_research_selstatus", _("Status: Research paused") );
		break;
	case RS_FINISH:
		Cvar_Set( "mn_research_selstatus", _("Status: Research finished") );
		break;
	case RS_NONE:
		Cvar_Set( "mn_research_selstatus", _("Status: Unknown technology") );
		break;
	default:
		break;
	}

	req_temp.numEntries = 0;
	RS_GetFirstRequired( tech, &req_temp );
	Com_sprintf( dependencies, MAX_VAR, _("Dependencies: "));
	if ( req_temp.numEntries > 0 ) {
		for ( i = 0; i < req_temp.numEntries; i++ ) {
			RS_GetName( req_temp.list[i], tempstring ); //name_temp gets initialised in getname
			Q_strcat( dependencies, MAX_VAR, tempstring );

			if ( i < req_temp.numEntries-1 )
				Q_strcat( dependencies, MAX_VAR, ", " );
		}
	} else {
		*dependencies = '\0';
	}
	Cvar_Set( "mn_research_seldep", dependencies );


}

/*======================
CL_ResearchSelectCmd

Changes the active research-list entry to the currently selected.
See menu_research.ufo for the layout/called functions.
======================*/
void CL_ResearchSelectCmd( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: research_select <num>\n" );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num >= researchListLength )
	{
		menuText[TEXT_STANDARD] = NULL;
		return;
	}

	// call researchselect function from menu_research.ufo
	researchListPos = num ;
	Cbuf_AddText( va( "researchselect%i\n", researchListPos ) );
	//RS_ResearchDisplayInfo();
	RS_UpdateData();
}

/*======================
RS_AssignScientist + RS_AssignScientist2

Assigns scientist to the selected research-project.
======================*/
void RS_AssignScientist2( int num )
{
	technology_t *tech = NULL;
	building_t *building = NULL;

	if ( num >= researchListLength )
	{
		menuText[TEXT_STANDARD] = NULL;
		return;
	}

	tech = researchList[num];

	if ( tech->statusResearchable ) {
		// check if there is a free lab available
		if ( ! tech->lab ) {
			building = MN_GetUnusedLab( baseCurrent->id ); // get a free lab from the currently active base
			if ( building ) {
				// assign the lab to the tech:
				tech->lab = building;
			} else {
				MN_Popup( _("Notice"), _("There is no free lab available.\\You need to build one or free another\\in order to assign scientists to research this technology.\n") );
				return;
			}
		}
		// assign a scientists to the lab
		if ( MN_AssignEmployee ( tech->lab, EMPL_SCIENTIST ) ) {
			tech->statusResearch = RS_RUNNING;
		} else {
			Com_Printf( "Can't add scientist from the lab.\n" );
		}

		RS_ResearchDisplayInfo();
		RS_UpdateData();
	}
}

void RS_AssignScientist( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: mn_rs_add <num_in_list>\n" );
		return;
	}
	num = atoi( Cmd_Argv( 1 ) );
	RS_AssignScientist2( num );
}



/*======================
RS_RemoveScientist + RS_RemoveScientist2

Remove scientist from the selected research-project.
======================*/
void RS_RemoveScientist2( int num )
{
	technology_t *tech = NULL;
	employees_t *employees_in_building = NULL;

	if ( num >= researchListLength )
	{
		menuText[TEXT_STANDARD] = NULL;
		return;
	}

	tech = researchList[num];
	// TODO: remove scientists from research-item
	Com_DPrintf( "RS_RemoveScientist: %s\n", tech->id );
	if ( tech->lab ) {
		Com_DPrintf( "RS_RemoveScientist: %s\n", tech->lab->name );
		if ( MN_RemoveEmployee( tech->lab ) ) {
			Com_DPrintf( "RS_RemoveScientist: Removal done.\n" );
			employees_in_building = &tech->lab->assigned_employees;
			if ( employees_in_building->numEmployees == 0) {
				tech->lab = NULL;
				tech->statusResearch = RS_PAUSED;
			}
		} else {
			Com_DPrintf( "Can't remove scientist from the lab.\n" );
		}
	} else {
		Com_DPrintf( "This tech is not researched in any lab.\n" );
	}

	RS_ResearchDisplayInfo();
	RS_UpdateData();
}

void RS_RemoveScientist( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: mn_rs_remove <num_in_list>\n" );
		return;
	}
	num = atoi( Cmd_Argv( 1 ) );
	RS_RemoveScientist2(num);
}

/*======================
RS_ResearchStart

Starts the research of the selected research-list entry.

TODO: Check if laboratory is available
======================*/
void RS_ResearchStart ( void )
{
	technology_t *tech = NULL;
	employees_t *employees_in_building = NULL;

	// we are not in base view
	if ( ! baseCurrent )
		return;

#if 0
	// check if laboratory is available
	if ( ! baseCurrent->hasLab )
		return;
#endif

	// get the currently selected research-item
	tech = researchList[researchListPos];

	if ( tech->statusResearchable ) {
		switch ( tech->statusResearch )
		{
		case RS_RUNNING:
			MN_Popup( _("Notice"), _("This item is already under research by your scientists.") );
			break;
		case RS_PAUSED:
			MN_Popup( _("Notice"), _("The research on this item continues.") );
			tech->statusResearch = RS_RUNNING;
			break;
		case RS_FINISH:
			MN_Popup( _("Notice"), _("The research on this item is complete.") );
			break;
		case RS_NONE:
			if ( ! tech->lab ) {
				RS_AssignScientist2( researchListPos ); // add scientists to tech
			} else {
				employees_in_building = &tech->lab->assigned_employees;
				if ( employees_in_building->numEmployees < 1  ) {
					RS_AssignScientist2( researchListPos ); // add scientists to tech
				}
			}
			tech->statusResearch = RS_RUNNING;
			break;
		default:
			break;
		}
	} else {
		MN_Popup( _("Notice"), _("The research on this item is not yet possible.\nYou need to research the technologies it's based on first.") );
	}
	RS_UpdateData();
}

/*======================
RS_ResearchStop

Pauses the research of the selected research-list entry.

TODO: Check if laboratory is available

======================*/
void RS_ResearchStop ( void )
{
	technology_t *tech = NULL;

	// we are not in base view
	if ( ! baseCurrent )
		return;

	// get the currently selected research-item
	tech = researchList[researchListPos];

	// TODO: remove lab from technology and scientists from lab

	switch ( tech->statusResearch )
	{
	case RS_RUNNING:
		tech->statusResearch = RS_PAUSED;
		break;
	case RS_PAUSED:
		MN_Popup( _("Notice"), _("The research on this item continues.") );
		tech->statusResearch = RS_RUNNING;
		break;
	case RS_FINISH:
		MN_Popup( _("Notice"), _("The research on this item is complete.") );
		break;
	case RS_NONE:
		Com_Printf( "Can't pause research. Research not started.\n" );
		break;
	default:
		break;
	}
	RS_UpdateData();
}

/*======================
RS_UpdateData

Loops trough the research-list and updates the displayed text+color of each research-item according to it's status.
See menu_research.ufo for the layout/called functions.
======================*/
void RS_UpdateData ( void )
{
	char	name[MAX_VAR];
	int	i, j;
	technology_t *tech = NULL;
	employees_t *employees_in_building = NULL;
	char tempstring[MAX_VAR];

	*name = '\0'; // init temp-name

	// make everything the same-color.
	Cbuf_AddText("research_clear\n");

	for ( i=0, j=0; i < numTechnologies; i++ ) {
		tech = &technologies[i];
		Com_sprintf( name, MAX_VAR, tech->name );
		if ( tech->statusCollected && !tech->statusResearchable && (tech->statusResearch != RS_FINISH ) ) { // an unresearched collected item that cannot yet be researched
			Q_strcat(name, MAX_VAR, _(" [not yet researchable]") );
			Cbuf_AddText( va( "researchunresearchable%i\n", j ) );	// Color the item 'unresearchable'
			Cvar_Set( va("mn_researchitem%i", j),  name );		// Display the concated text in the correct list-entry.
			researchList[j] = &technologies[i];					// Assign the current tech in the global list to the correct entry in the displayed list.
			j++;											// counting the numbers of display-list entries.
		}
		else
		if ( ( tech->statusResearch != RS_FINISH ) && ( tech->statusResearchable ) ) {
			//TODO:
			Cvar_Set( va( "mn_researchassigned%i", j ), "as.");
			Cvar_Set( va( "mn_researchavailable%i", j ), "av.");
			Cvar_Set( va( "mn_researchmax%i", j ), "mx.");

			if ( tech->lab ) {
				/* display the assigned/free/max numbers of scientists for this tech */
				employees_in_building = &tech->lab->assigned_employees;
				Com_sprintf( tempstring, MAX_VAR, _("%i max.\n"), employees_in_building->maxEmployees );
				Cvar_Set( va( "mn_researchmax%i",j ), tempstring );		// max number of employees in this base
				Com_sprintf( tempstring, MAX_VAR, "%i\n", employees_in_building->numEmployees );
				Cvar_Set( va( "mn_researchassigned%i",j ), tempstring );	// assigned employees to the technology
				Com_sprintf( tempstring, MAX_VAR, "%i\n", MN_EmployeesInBase2 ( tech->lab->base, EMPL_SCIENTIST, qtrue ) );
				Cvar_Set( va( "mn_researchavailable%i",j ), tempstring );	// max. available scis in the base the tech is reseearched
			}

			switch ( tech->statusResearch ) // Set the text of the research items and mark them if they are currently researched.
			{
			case RS_RUNNING:
				Q_strcat(name, MAX_VAR, _(" [under research]") );
				Cbuf_AddText( va( "researchrunning%i\n", j ) );	// color the item 'research running'
				break;
			case RS_FINISH:
				// DEBUG: normaly these will not be shown at all. see "if" above
				Q_strcat(name, MAX_VAR, _(" [finished]") );
				break;
			case RS_PAUSED:
				Q_strcat(name, MAX_VAR, _(" [paused]") );
				Cbuf_AddText( va( "researchpaused%i\n", j ) );	// color the item 'research paused'
				break;
			case RS_NONE:
				Q_strcat(name, MAX_VAR, _(" [unknown]") );
				// The color is defined in menu research.ufo by  "confunc research_clear". See also above.
				break;
			default:
				break;
			}

			Cvar_Set( va("mn_researchitem%i", j),  _(name) );	// Display the concated text in the correct list-entry.
			researchList[j] = &technologies[i];				// Assign the current tech in the global list to the correct entry in the displayed list.
			j++;										// counting the numbers of display-list entries.
		}
	}

	researchListLength = j;

	// Set rest of the list-entries to have no text at all.
	for ( ; j < MAX_RESEARCHDISPLAY; j++ ) {
		Cvar_Set( va( "mn_researchitem%i", j ), "" );
		Cvar_Set( va( "mn_researchassigned%i", j ), "");
		Cvar_Set( va( "mn_researchavailable%i", j ), "");
		Cvar_Set( va( "mn_researchmax%i", j ), "");
	}

	// Select last selected item if possible or the very first one if not.
	if ( researchListLength ) {
		Com_DPrintf("RS_UpdateData: Pos%i Len%i\n", researchListPos, researchListLength );
		if ( (researchListPos < researchListLength) &&  ( researchListLength < MAX_RESEARCHDISPLAY ) ) {
			Cbuf_AddText( va("researchselect%i\n",researchListPos ) );
		}else {
			Cbuf_AddText( "researchselect0\n" );
		}
	} else {
		// No display list abailable (zero items) - > Reset description.
		Cvar_Set( "mn_researchitemname", "" );
		Cvar_Set( "mn_researchitem", "" );
		Cvar_Set( "mn_researchweapon", "" );
		Cvar_Set( "mn_researchammo", "" );
		menuText[TEXT_STANDARD] = NULL;
	}
	RS_ResearchDisplayInfo();	// Update the description field/area.
}

/*======================
CL_ResearchType

TODO: document this
======================*/
void CL_ResearchType ( void )
{
	// get the list
	RS_UpdateData();

	// nothing to research here
	if ( ! researchListLength || !ccs.numBases )
		Cbuf_ExecuteText( EXEC_NOW, "mn_pop" );
	else if ( baseCurrent && ! baseCurrent->hasLab )
		MN_Popup( _("Notice"), _("Build a laboratory first") );
}

/*======================
RS_DependsOn

Checks if the research item id1 depends on (requires) id2

IN
	id1:	Unique id of a technology_t that may or may not depend on id2.
	id2:	Unique id of a technology_t

OUT
	boolean	RS_DependsOn
======================*/
byte RS_DependsOn(char *id1, char *id2)
{
	int i;
	technology_t *tech = NULL;
	stringlist_t	required;

	tech = RS_GetTechByID( id1 );
	if ( ! tech )
		return qfalse;

	/* research item found */
	required = tech->requires;
	for ( i=0; i < required.numEntries; i++ ) {
		if ( !Q_strncmp(required.list[i], id2, MAX_VAR ) )	// current item (=id1) depends on id2
			return qtrue;
	}
	return qfalse;
}

/*======================
RS_MarkResearched

Mark technologies as researched. This includes techs that depends in "id" and have time=0

IN
	id:	Unique id of a technology_t.
======================*/
void RS_MarkResearched( char *id )
{
	int i;
	technology_t *t = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !Q_strncmp( id, t->id, MAX_VAR ) ) {
			t->statusResearch = RS_FINISH;
			Com_Printf("Research of \"%s\" finished.\n", t->id );
		}
		else if ( RS_DependsOn( t->id, id ) && (t->time <= 0) ) {
			t->statusResearch = RS_FINISH;
			Com_Printf("Depending tech \"%s\" has been researched as well.\n",  t->id );
		}
	}
	RS_MarkResearchable();
}

/*======================
CL_CheckResearchStatus

TODO: document this
======================*/
void CL_CheckResearchStatus ( void )
{
	int i, newResearch = 0;
	technology_t *tech = NULL;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;

	if ( ! researchListLength )
		return;

	for ( i=0; i < numTechnologies; i++ ) {
		tech = &technologies[i];
		if ( tech->statusResearch == RS_RUNNING )
		{
			if ( tech->time <= 0 ) {
				if ( ! newResearch )
					Com_sprintf( messageBuffer, MAX_MESSAGE_TEXT, _("Research of %s finished\n"), tech->name );
				else
					Com_sprintf( messageBuffer, MAX_MESSAGE_TEXT, _("%i researches finished\n"), newResearch+1 );
				MN_AddNewMessage( _("Research finished"), messageBuffer, qfalse, MSG_RESEARCH, tech );

				MN_ClearBuilding( tech->lab );
				tech->lab = NULL;
				RS_MarkResearched( tech->id );
				researchListPos = 0;
				newResearch++;
				tech->time = 0;
			} else {
				if ( tech->lab ) {

					building = tech->lab;
					Com_DPrintf("building found %s\n", building->name );
					employees_in_building = &building->assigned_employees;
					if ( employees_in_building->numEmployees > 0 ) {
						Com_DPrintf("employees are there\n");
						/* OLD
						tech->time--;		// reduce one time-unit
						*/
						Com_DPrintf("timebefore %.2f\n", tech->time );
						Com_DPrintf("timedelta %.2f\n", employees_in_building->numEmployees * 0.8 );
						tech->time -= employees_in_building->numEmployees * 0.8; // just for testing, better formular needed
						Com_DPrintf("timeafter %.2f\n", tech->time );
						//TODO include employee-skill in calculation
						if ( tech->time < 0 )
							tech->time = 0; // Will be a good thing (think of percentage-calculation) once non-integer values are used.
					}
				}
			}
		}
	}

	if ( newResearch ) {
		CL_GameTimeStop();
		CL_ResearchType();
	}
}

/*======================
RS_TechnologyList_f

List all parsed technologies and their attributes in commandline/console.

Command to call this: techlist
======================*/
void RS_TechnologyList_f ( void )
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t *req = NULL;
	stringlist_t req_temp;

	for ( i = 0; i < numTechnologies; i++ )
	{
		t = &technologies[i];
		req = &t->requires;
		Com_Printf("Tech: %s\n", t->id );
		Com_Printf("... time      -> %.2f\n", t->time );
		Com_Printf("... name      -> %s\n", t->name );
		Com_Printf("... requires  ->");
		for ( j = 0; j < req->numEntries; j++ )
			Com_Printf( " %s",req->list[j] );
		Com_Printf("\n");
		Com_Printf( "... provides  -> %s", t->provides );
		Com_Printf("\n");

		Com_Printf( "... type      -> ");
		switch ( t->type )
		{
		case RS_TECH:	Com_Printf("tech"); break;
		case RS_WEAPON:	Com_Printf("weapon"); break;
		case RS_CRAFT:	Com_Printf("craft"); break;
		case RS_ARMOR:	Com_Printf("armor"); break;
		case RS_BUILDING:	Com_Printf("building"); break;
		default:	break;
		}
		Com_Printf("\n");

		Com_Printf( "... research  -> ");
		switch ( t->type )
		{
		case RS_NONE:	Com_Printf("unknown tech"); break;
		case RS_RUNNING:	Com_Printf("running"); break;
		case RS_PAUSED:	Com_Printf("paused"); break;
		case RS_FINISH:	Com_Printf("done"); break;
		default:	break;
		}
		Com_Printf("\n");

		Com_Printf("... Res.able  -> %i\n", t->statusResearchable );
		Com_Printf("... Collected -> %i\n", t->statusCollected );


		Com_Printf("... req_first ->");
		req_temp.numEntries = 0;
		RS_GetFirstRequired( t, &req_temp );
		for ( j = 0; j < req_temp.numEntries; j++ )
			Com_Printf( " %s", req_temp.list[j] );

		Com_Printf("\n");
	}
}

/*======================
MN_ResearchInit

Command to call this: research_init

Should be called whenever the research menu
gets active
======================*/
void MN_ResearchInit( void )
{
	CL_ResearchType();
}

#ifdef DEBUG
/*======================
RS_DebugResearchAll

call this function if you already hold a tech pointer
======================*/
void RS_DebugResearchAll ( void )
{
	int i;
	for ( i=0; i < numTechnologies; i++ )
	{
		Com_Printf("...mark %s as researched\n", technologies[i].id );
		technologies[i].statusResearchable = qtrue;
		technologies[i].statusResearch = RS_FINISH;
		technologies[i].needsCollected = qfalse;
	}
}
#endif

/*======================
MN_ResetResearch

This is more or less the initial
Bind some of the functions in htis file to console-commands that you can call ingame.
Called from MN_ResetMenus resp. CL_InitLocal
======================*/
void MN_ResetResearch( void )
{
	researchListLength = 0;
	// add commands and cvars
	Cmd_AddCommand( "research_init", MN_ResearchInit );
	Cmd_AddCommand( "research_select", CL_ResearchSelectCmd );
	Cmd_AddCommand( "research_type", CL_ResearchType );
	Cmd_AddCommand( "mn_start_research", RS_ResearchStart );
	Cmd_AddCommand( "mn_stop_research", RS_ResearchStop );
	Cmd_AddCommand( "mn_rs_add", RS_AssignScientist );
	Cmd_AddCommand( "mn_rs_remove", RS_RemoveScientist );
	Cmd_AddCommand( "research_update", RS_UpdateData );
	Cmd_AddCommand( "techlist", RS_TechnologyList_f );	// DEBUGGING ONLY
	Cmd_AddCommand( "invlist", Com_InventoryList_f );
#ifdef DEBUG
	Cmd_AddCommand( "research_all", RS_DebugResearchAll );
#endif
}

/*======================
The valid definition names in the research.ufo file.
NOTE: the TECHFS macro assignes the values from scriptfile
to the appropriate values in the corresponding struct
======================*/
value_t valid_tech_vars[] =
{
	{ "name",		V_TRANSLATION2_STRING,	TECHFS( name ) },			//name of technology
	{ "description",	V_TRANSLATION2_STRING,	TECHFS( description ) },
	{ "provides",		V_STRING,	TECHFS( provides ) },		//what does this research provide
	{ "needscollected",	V_BOOL,		TECHFS( needsCollected ) },	// to be able to research this tech zou need all "required" and at least one collected "provides" item.
	{ "time",			V_FLOAT,		TECHFS( time ) },			//how long will this research last
	{ "image_top",		V_STRING,	TECHFS( image_top ) },
	{ "image_bottom",	V_STRING,	TECHFS( image_bottom ) },
	{ "mdl_top",		V_STRING,	TECHFS( mdl_top ) },
	{ "mdl_bottom",	V_STRING,	TECHFS( mdl_bottom ) },
	{ NULL,	0, 0 }
};

/*======================
RS_ParseTechnologies

Parses one "tech" entry in the research.ufo file and writes it into the next free entry in technologies (technology_t).

IN
	id:	Unique id of a technology_t. This is parsed from "tech xxx" -> id=xxx
	text:	TODO document this ... it appears to be the whole following text that is part of the "tech" item definition in research.ufo.
======================*/
void RS_ParseTechnologies ( char* id, char** text )
{
	value_t *var = NULL;
	technology_t *tech = NULL;
	char	*errhead = "RS_ParseTechnologies: unexptected end of file.";
	char	*token = NULL;
	char	*misp = NULL;
	char	temp_text[MAX_VAR];
	stringlist_t *required = NULL;

	// get body
	token = COM_Parse( text );
	if ( !*text || *token != '{' ) {
		Com_Printf("RS_ParseTechnologies: \"%s\" technology def without body ignored.\n", id );
		return;
	}
	if ( numTechnologies >= MAX_TECHNOLOGIES ) {
		Com_Printf("RS_ParseTechnologies: too many technology entries. limit is %i.\n", MAX_TECHNOLOGIES );
		return;
	}

	// New technology (next free entry in global tech-list)
	tech = &technologies_skeleton[numTechnologies++];
	required = &tech->requires;
	memset( tech, 0, sizeof( technology_t ) );

	//set standard values
	Com_sprintf( tech->id, MAX_VAR, id );
	Com_sprintf( tech->description, MAX_VAR, _("No description available.") );
	*tech->provides = '\0';
	*tech->image_top = '\0';
	*tech->image_bottom = '\0';
	*tech->mdl_top = '\0';
	*tech->mdl_bottom = '\0';
	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;
	tech->statusCollected  = 0;
	tech->time = 0;
	tech->overalltime = 0;

	do {
		// get the name type
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		// get values
		if (  !Q_strncmp( token, "type", 4 ) ) {
			/* what type of tech this is */
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;
			if ( !Q_strncmp( token, "tech", 4 ) )
				tech->type = RS_TECH; // redundant, but oh well.
			else if ( !Q_strncmp( token, "weapon", 6 ) )
				tech->type = RS_WEAPON;
			else if ( !Q_strncmp( token, "armor", 5 ) )
				tech->type = RS_ARMOR;
			else if ( !Q_strncmp( token, "craft", 5 ) )
				tech->type = RS_CRAFT;
			else if ( !Q_strncmp( token, "building", 8 ) )
				tech->type = RS_BUILDING;
			else if ( !Q_strncmp( token, "alien", 5 ) )
				tech->type = RS_ALIEN;
			else Com_Printf("RS_ParseTechnologies: \"%s\" unknown techtype: \"%s\" - ignored.\n", id, token );
		}
		else
		if ( !Q_strncmp( token, "requires", 8 ) )
		{
			/* what other techs this one requires */
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;
			Q_strncpyz( temp_text, token, MAX_VAR );
			misp = temp_text;

			required->numEntries = 0;
			do {
				token = COM_Parse( &misp );
				if ( !misp ) break;
				if ( required->numEntries < MAX_TECHLINKS )
					Q_strncpyz( required->list[required->numEntries++], token, MAX_VAR);
				else
					Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", id, MAX_TECHLINKS );

			}
			while ( misp && required->numEntries < MAX_TECHLINKS );
			continue;
		}
		else
		if ( !Q_strncmp( token, "researched", 10 ) )
		{
			/* tech alreadyy researched? */
			token = COM_EParse( text, errhead, id );
			if ( !Q_strncmp( token, "true", 4 ) || *token == '1' )
				tech->statusResearch = RS_FINISH;
		}
		else
		if ( !Q_strncmp( token, "up_chapter", 10 ) ) {
			/* ufopedia chapter */
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;

			if ( *token ) {
				// find chapter
				int i;
				for ( i = 0; i < numChapters; i++ ) {
					if ( !Q_strncmp( token, upChapters[i].id, MAX_VAR ) ) {
						// add entry to chapter
						tech->up_chapter = &upChapters[i];
						if ( !upChapters[i].first ) {
							upChapters[i].first = tech;
							upChapters[i].last = tech;
						} else {
							technology_t *old;
							upChapters[i].last = tech;
							old = upChapters[i].first;
							while ( old->next ) old = old->next;
							old->next = tech;
							tech->prev = old;
						}
						break;
					}
					if ( i == numChapters )
						Com_Printf("RS_ParseTechnologies: \"%s\" - chapter \"%s\" not found.\n", id, token );
				}
			}
		}
		else
		for ( var = valid_tech_vars; var->string; var++ )
			if ( !Q_strncmp( token, var->string, sizeof(var->string) ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, id );
				if ( !*text ) return;

				if ( var->ofs && var->type != V_NULL )
					Com_ParseValue( tech, token, var->type, var->ofs );
				else
					// NOTE: do we need a buffer here? for saving or something like that?
					Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed\n" );
				break;
			}
		//TODO: debug this thing, it crashes the game at startup.
		//if ( !var->string ) //TODO: escape "type weapon/tech/etc.." here
		//	Com_Printf( "RS_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n", token, id );

	} while ( *text );
}

/*======================
RS_GetRequired

Returns the list of required (by id) items.

IN
	id:	Unique id of a technology_t.

OUT
	required: a list of strings with the unique ids of items/buildings/etc..

TODO: out of order ... seems to produce garbage
======================*/

void RS_GetRequired( char *id, stringlist_t *required)
{
	technology_t *tech = NULL;

	tech = RS_GetTechByID( id );
	if ( ! tech )
		return;

	/* research item found */
	required = &tech->requires;	// is linking a good idea?
}

/*======================
RS_IsResearched_

call this function if you already hold a tech pointer
======================*/
qboolean RS_IsResearched_ ( technology_t* t )
{
	if ( t && t->statusResearch == RS_FINISH )
		return qtrue;
	return qfalse;
}

/*======================
RS_ItemIsResearched

Checks if the item (as listed in "provides") has been researched

IN
	id_provided:	Unique id of an item/building/etc.. that is provided by a technology_t

OUT
	boolean	RS_ItemIsResearched
======================*/
qboolean RS_ItemIsResearched(char *id_provided )
{
	int i;
	technology_t *tech = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		tech = &technologies[i];
		if ( !Q_strncmp( id_provided, tech->provides, MAX_VAR ) )	// provided item found
			return RS_IsResearched_( tech );
	}
	return qtrue;	// no research needed
}

/*======================
RS_Collected_

call this function if you already hold a tech pointer
======================*/
qboolean RS_Collected_ ( technology_t* t )
{
	return ( t && t->statusCollected );
}

/*======================
RS_ItemCollected

Returns the number of collected ("provided") items.

IN
	id_provided:	Unique id of an item/building/etc.. that is provided by a technology_t

OUT
	int	RS_ItemIsResearched	Number of the collected items.
======================*/
qboolean RS_ItemCollected(char *id_provided )
{
	int	i = 0;

	if ( ! id_provided )
		return qfalse;

	for ( ; i < numTechnologies; i++ ) {
		if ( !Q_strncmp( (char*)id_provided, technologies[i].provides, MAX_VAR ) )
			return RS_Collected_( &technologies[i] );
	}
	Com_DPrintf("RS_ItemCollected: \"%s\" <- research item that 'provides' this item not found.\n", id_provided );
	return qfalse;
}

/*======================
RS_TechIsResearched

Checks if the technology (tech-id) has been researched.

IN
	id:	Unique id of a technology_t.

OUT
	boolean	RS_TechIsResearched
======================*/
qboolean RS_TechIsResearched(char *id )
{
	technology_t *tech = NULL;

	if ( !id ) return qfalse;

	if ( !Q_strncmp( id, "initial", 7 )
	|| !Q_strncmp( id, "nothing", 7 ) )
		return qtrue;	// initial and nothing are always researched. as they are just starting "technologys" that are never used.

	tech = RS_GetTechByID( id );
	if ( ! tech )
		return qfalse;

	/* research item found */
	if ( tech->statusResearch == RS_FINISH )
		return qtrue;

	return qfalse;
}

/*======================
RS_TechIsResearchable

Checks if the technology (tech-id) is researchable.

IN
	t:	pointer to technology_t.

OUT
	boolean	RS_TechIsResearchable
======================*/
qboolean RS_TechIsResearchable( technology_t* tech )
{
	int i;
	stringlist_t* required = NULL;

	if ( ! tech )
		return qfalse;

	/* research item found */
	if ( tech->statusResearch == RS_FINISH )
		return qfalse;

	if ( ( !Q_strncmp( tech->id, "initial", 7 ) )
	  || ( !Q_strncmp( tech->id, "nothing", 7 ) ) )
		return qtrue;

	required = &tech->requires;

	for ( i=0; i < required->numEntries; i++)
		if ( !RS_TechIsResearched( required->list[i]) )	// Research of "id" not finished (RS_FINISH) at this point.
			return qfalse;

	return qtrue;

}

/*======================
RS_GetFirstRequired + RS_GetFirstRequired2

Returns the first required (yet unresearched) technologies that are needed by "id".
That means you need to research the result to be able to research (and maybe use) "id".
======================*/
void RS_GetFirstRequired2 ( technology_t* tech, char *first_id, stringlist_t *required )
{
	int i;
	stringlist_t *required_temp = NULL;
	technology_t* t = NULL;

	if ( ! tech )
		return;

	required_temp = &tech->requires;
	//Com_DPrintf( "RS_GetFirstRequired2: %s - %s - %s\n", id, first_id, required_temp->list[0]  );
	if ( !Q_strncmp( required_temp->list[0] , "initial", 7 ) || !Q_strncmp( required_temp->list[0] , "nothing", 7 ) ) {
		if ( !Q_strncmp( tech->id, first_id, MAX_VAR ) )
			return;
		if ( required->numEntries < MAX_TECHLINKS ) {
			// TODO: check if the firstrequired tech has already been added (e.g indirectly from another tech)
			required->techPtr[required->numEntries] = tech;
			Q_strncpyz( required->list[required->numEntries++], tech->id, MAX_VAR );
			Com_DPrintf("RS_GetFirstRequired2: \"%s\" - requirement 'initial' or 'nothing' found.\n", tech->id );
		}
		return;
	}
	for ( i=0; i < required_temp->numEntries; i++ ) {
		t = RS_GetTechByID( required_temp->list[i] );
		if ( RS_IsResearched_(t) ) {
			if ( required->numEntries < MAX_TECHLINKS ) {
				required->techPtr[required->numEntries] = t;
				Q_strncpyz( required->list[required->numEntries++], t->id, MAX_VAR );
				Com_DPrintf( "RS_GetFirstRequired2: \"%s\" - next item \"%s\" already researched.\n", tech->id, t->id );
			}
		} else {
			RS_GetFirstRequired2( t, first_id, required );
		}
	}
}

void RS_GetFirstRequired( technology_t* tech, stringlist_t *required )
{
	 RS_GetFirstRequired2( tech, tech->id, required);
}

/*======================
RS_GetProvided

TODO
Returns a list of .ufo items that are produceable when this item has been researched (=provided)
This list also incldues other items that "require" this one (id) and have a reseach_time of 0.
======================
void RS_GetProvided( char *id, char *provided )
{
	int i, j;
	technology_t *t = NULL;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			for ( j=0; j < MAX_TECHLINKS; j++ )
				Com_sprintf(provided[j], MAX_VAR, t->provides);
			//TODO: search for dependent items.
			for ( j=0; j < numTechnologies; j++ ) {
				if (RS_DependsOn( t->id, id ) ) {
					// TODO: append researchtree[j]->provided to *provided
				}
			}
			return;
		}
	}
	Com_Printf( "RS_GetProvided: research item \"%s\" not found.\n", id );
}*/

/*======================
RS_SaveTech
======================*/
void RS_SaveTech( sizebuf_t *sb )
{
	int i;

	MSG_WriteLong( sb, numTechnologies );
	Com_DPrintf("Saving %i technologies\n", numTechnologies );
	for ( i = 0; i < numTechnologies; i++ )
	{
		MSG_WriteByte( sb, technologies[i].statusResearch );
		MSG_WriteLong( sb, technologies[i].statusCollected );
		MSG_WriteFloat( sb, technologies[i].time);
	}
}

/*======================
RS_LoadTech
======================*/
void RS_LoadTech( sizebuf_t *sb, int version )
{
	int tmp, i;
	if ( version >= 3 )
	{
		tmp = MSG_ReadLong( sb );
		if ( tmp != numTechnologies )
			Com_Printf("There was an update and there are new technologies available which aren't in your savegame. You may encounter problems. (%i:%i)\n", tmp, numTechnologies );
		for ( i = 0; i < tmp; i++ )
		{
			technologies[i].statusResearch = MSG_ReadByte( sb );
			technologies[i].statusCollected = MSG_ReadLong( sb );
			technologies[i].time = MSG_ReadFloat( sb );
		}
	}
	RS_MarkResearchable ();
}

/*======================
RS_GetTechByID

return a pointer to the technology
identified by given id string
======================*/
technology_t* RS_GetTechByID ( const char* id )
{
	int	i = 0;

	if ( ! id )
		return NULL;

	for ( ; i < numTechnologies; i++ )
	{
		if ( !Q_strncmp( (char*)id, technologies[i].id, MAX_VAR ) )
			return &technologies[i];
	}
	Com_DPrintf("RS_GetTechByID: Could not find a technology with id \"%s\"\n", id );
	return NULL;
}

/*======================
RS_GetTechByProvided

returns a pointer to the item tech (as listed in "provides")
======================*/
technology_t* RS_GetTechByProvided( const char *id_provided )
{
	int i;
	for ( i=0; i < numTechnologies; i++ )
	{
		if ( !Q_strncmp( (char*)id_provided, technologies[i].provides, MAX_VAR ) )
			return &technologies[i];
	}
	Com_DPrintf("RS_GetTechByProvided: Could not find a technology that provides \"%s\"\n", id_provided );
	return NULL;
}
