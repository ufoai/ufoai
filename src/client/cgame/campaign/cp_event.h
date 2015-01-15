/**
 * @file
 * @brief Header for geoscape event related stuff.
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

#define MAX_EVENTMAILS 64
#define MAX_CAMPAIGNEVENTS 128

/**
 * @brief available mails for a tech - mail and mail_pre in script files
 * @sa techMail_t
 * @note Parsed via CL_ParseEventMails
 * @note You can add a mail to the message system and mail client
 * by using e.g. the mission triggers with the script command 'addeventmail \<id\>'
 */
typedef struct eventMail_s {
	char* id;			/**< script id */
	char* from;			/**< sender (_mail_from_paul_navarre, _mail_from_dr_connor) */
	char* to;			/**< recipient (_mail_to_base_commander) */
	char* cc;			/**< copy recipient (_mail_to_base_commander) */
	char* subject;		/**< mail subject line - if mail and mail_pre are available
						 * this will be filled with Proposal: (mail_pre) and Re: (mail)
						 * automatically */
	char* date;			/**< date string, if empty use the date of research */
	char* body;			/**< the body of the event mail */
	char* icon;			/**< icon in the mailclient */
	char* model;		/**< model name of the sender */
	bool read;			/**< already read the mail? */
	bool sent;			/**< already sent, don't send twice */
	bool skipMessage;	/**< don't add the mail to the message stack (won't be saved) */
} eventMail_t;

void CL_EventAddMail_f(void);
void CL_ParseEventMails(const char* name, const char** text);
eventMail_t* CL_GetEventMail(const char* id);
void CP_FreeDynamicEventMail(void);
void CL_EventAddMail(const char* eventMailId);

/**
 * @brief Defines campaign events when story related technologies should be researched
 */
typedef struct campaignEvent_s {
	char* tech;			/**< technology id that should be researched if the overall interest is reached */
	int interest;		/**< the interest value (see @c ccs.oberallInterest) */
} campaignEvent_t;

typedef struct campaignEvents_s {
	campaignEvent_t campaignEvents[MAX_CAMPAIGNEVENTS];	/**< holds all campaign events */
	int numCampaignEvents;	/**< how many events (script-id: events) parsed */
	char* id;				/**< script id */
} campaignEvents_t;

/**
 * @brief events that are triggered by the campaign
 */
typedef enum {
	NEW_DAY,
	UFO_DETECTION,
	CAPTURED_ALIENS_DIED,
	CAPTURED_ALIENS,
	ALIENBASE_DISCOVERED
} campaignTriggerEventType_t;

typedef struct {
	campaignTriggerEventType_t type;
	char* id;				/**< the script id */
	char* require;			/**< the binary expression to evaluate for this event */
	char* reactivate;		/**< the binary expression to reactivate this event */
	char* command;			/**< the command to execute if the @c require field evaluated to @c true */
	bool once;				/**< if this is @c true, the event will only be triggered once */
	bool active;			/**< if this event is inactive, and has a @c reactivate binary expression, it can get reactivated */
} campaignTriggerEvent_t;

#define MAX_CAMPAIGN_TRIGGER_EVENTS 32

void CP_CheckCampaignEvents(struct campaign_s* campaign);
void CL_ParseCampaignEvents(const char* name, const char** text);
void CP_ParseEventTrigger(const char* name, const char** text);
bool CP_TriggerEventLoadXML(xmlNode_t* p);
bool CP_TriggerEventSaveXML(xmlNode_t* p);
void CP_TriggerEvent(campaignTriggerEventType_t type, const void* userdata = nullptr);
const campaignEvents_t* CP_GetEventsByID(const char* name);
