#include "RoutingLumpLoader.h"
#include "RoutingLump.h"
#include "../../../shared/autoptr.h"
#include "ifilesystem.h"
#include "gtkutil/dialog.h"
#include "radiant_i18n.h"

#include "../../../shared/ufotypes.h"
#include "../../../shared/typedefs.h"
#include "../../../game/q_sizes.h"
#include "../../../common/qfiles.h"
#define COMPILE_MAP
typedef int mapTiles_t;
#include "../../../common/routing.h"

namespace routing
{

	/**
	 * @param[in] source Source will be set to the end of the compressed data block!
	 * @sa CompressRouting (ufo2map)
	 * @sa CMod_LoadRouting
	 */
	static int CMod_DeCompressRouting (byte ** source, byte * dataStart)
	{
		int i, c;
		byte *data_p;
		byte *src;

		data_p = dataStart;
		src = *source;

		while (*src) {
			if (*src & 0x80) {
				/* repetitions */
				c = *src++ & ~0x80;
				/* Remember that the total bytes that are the same is c + 2 */
				for (i = 0; i < c + 2; i++)
					*data_p++ = *src;
				src++;
			} else {
				/* identities */
				c = *src++;
				for (i = 0; i < c; i++)
					*data_p++ = *src++;
			}
		}

		src++;
		*source = src;

		return data_p - dataStart;
	}

	/**
	 * @brief evaluate access state for given position for given routing data
	 * @param routes routing data
	 * @param pos position to evaluate
	 * @return access state as enum value for later rendering
	 */
	static EAccessState evaluateAccessState (const routing_t routes[ACTOR_MAX_SIZE], const pos3_t pos, const int actorSize)
	{
		const int height = QuantToModel(RT_getCeiling(routes, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1))
				- RT_getFloor(routes, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1)));
		if (height >= PLAYER_STANDING_HEIGHT)
			return ACC_STAND;
		else if (height >= PLAYER_CROUCHING_HEIGHT)
			return ACC_CROUCH;
		else
			return ACC_DISABLED;
	}

	static EConnectionState evaluateConnectionState (const routing_t routes[ACTOR_MAX_SIZE], const pos3_t pos,
			const int actorSize, const EDirection direction)
	{
		byte route = 0;
		byte stepup = 0;

		switch (direction) {
		case DIR_WEST:
			route = RT_CONN_NY(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_NY(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_NORTHWEST:
			route = RT_CONN_PX_NY(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_PX_NY(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_NORTH:
			route = RT_CONN_PX(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_PX(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_NORTHEAST:
			route = RT_CONN_PX_PY(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_PX_PY(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_EAST:
			route = RT_CONN_PY(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_PY(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_SOUTHEAST:
			route = RT_CONN_NX_PY(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_NX_PY(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_SOUTH:
			route = RT_CONN_NX(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_NX(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case DIR_SOUTHWEST:
			route = RT_CONN_NX_NY(routes,actorSize,pos[0],pos[1],pos[2]);
			stepup = RT_STEPUP_NX_NY(routes,actorSize,pos[0],pos[1],pos[2]);
			break;
		case MAX_DIRECTIONS:
			break;
		}

		if (stepup == PATHFINDING_NO_STEPUP)
			return CON_DISABLE;
		else if (route >= ModelCeilingToQuant(PLAYER_STANDING_HEIGHT))
			return CON_WALKABLE;
		else if (route >= ModelCeilingToQuant(PLAYER_CROUCHING_HEIGHT))
			return CON_CROUCHABLE;
		else
			return CON_DISABLE;
	}

	/**
	 * @brief evaluate map data to set access and connectivity state to given lump entry.
	 * @param entry entry to fill values into
	 * @param routes routing data
	 * @param pos position to evaluate
	 */
	static void FillRoutingLumpEntry (RoutingLumpEntry &entry, routing_t routes[ACTOR_MAX_SIZE], pos3_t pos)
	{
		entry.setAccessState(evaluateAccessState(routes, pos, ACTOR_SIZE_NORMAL));
		for (EDirection direction = DIR_WEST; direction < MAX_DIRECTIONS; direction++) {
			entry.setConnectionState(direction, evaluateConnectionState(routes, pos, ACTOR_SIZE_NORMAL, direction));
		}
	}

	/**
	 * @param[in] l Routing lump ... (routing data lump from bsp file)
	 * @param[in] sX The x position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
	 * @param[in] sY The y position on the world plane (grid position) - values from -(PATHFINDING_WIDTH/2) up to PATHFINDING_WIDTH/2 are allowed
	 * @param[in] sZ The height level on the world plane (grid position) - values from 0 - PATHFINDING_HEIGHT are allowed
	 * @sa CM_AddMapTile
	 * @todo TEST z-level routing
	 */
	static void CMod_LoadRouting (RoutingLump& routingLump, const std::string& name, const lump_t *l,
			byte* cModelBase, int sX, int sY, int sZ)
	{
		static routing_t tempMap[ACTOR_MAX_SIZE];
		static routing_t clMap[ACTOR_MAX_SIZE];
		static MapTile curTile;
		byte *source;
		int length;
		int x, y, z, size, dir;
		int minX, minY, minZ;
		int maxX, maxY, maxZ;
		unsigned int i;
		double start, end;
		const int targetLength = sizeof(curTile.wpMins) + sizeof(curTile.wpMaxs) + sizeof(tempMap);

		start = time(NULL);

		if (!GUINT32_TO_LE(l->filelen)) {
			g_error("CMod_LoadRouting: Map has NO routing lump");
			return;
		}

		source = cModelBase + GUINT32_TO_LE(l->fileofs);

		i = CMod_DeCompressRouting(&source, (byte*) curTile.wpMins);
		length = i;
		i = CMod_DeCompressRouting(&source, (byte*) curTile.wpMaxs);
		length += i;
		i = CMod_DeCompressRouting(&source, (byte*) tempMap);
		length += i;

		if (length != targetLength) {
			g_error("CMod_LoadRouting: Map has BAD routing lump; expected %i got %i", targetLength, length);
			return;
		}

		/* endian swap possibly necessary */
		for (i = 0; i < 3; i++) {
			curTile.wpMins[i] = GUINT32_TO_LE(curTile.wpMins[i]);
			curTile.wpMaxs[i] = GUINT32_TO_LE(curTile.wpMaxs[i]);
		}

		g_debug("wpMins:(%i, %i, %i) wpMaxs:(%i, %i, %i)\n", curTile.wpMins[0], curTile.wpMins[1], curTile.wpMins[2],
				curTile.wpMaxs[0], curTile.wpMaxs[1], curTile.wpMaxs[2]);

		/* Things that need to be done:
		 * The floor, ceiling, and route data can be copied over from the map.
		 * All data must be regenerated for cells with overlapping content or where new
		 * model data is adjacent to a cell with existing model data. */

		/* Copy the routing information into our master table */
		minX = std::max(curTile.wpMins[0], 0);
		minY = std::max(curTile.wpMins[1], 0);
		minZ = std::max(curTile.wpMins[2], 0);
		maxX = std::min(curTile.wpMaxs[0], PATHFINDING_WIDTH - 1);
		maxY = std::min(curTile.wpMaxs[1], PATHFINDING_WIDTH - 1);
		maxZ = std::min(curTile.wpMaxs[2], PATHFINDING_HEIGHT - 1);

		/** @todo check whether we need this copy code */
		for (size = 0; size < ACTOR_MAX_SIZE; size++)
			/* Adjust starting x and y by size to catch large actor cell overlap. */
			for (y = minY - size; y <= maxY; y++)
				for (x = minX - size; x <= maxX; x++) {
					/* Just incase x or y start negative. */
					if (x < 0 || y < 0)
						continue;
					for (z = minZ; z <= maxZ; z++) {
						clMap[size].setFloor(x, y, z, tempMap[size].getFloor(x - sX, y - sY, z - sZ));
						clMap[size].setCeiling(x, y, z, tempMap[size].getCeiling(x - sX, y - sY, z - sZ));
						for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
							clMap[size].setConn(x, y, z, dir, tempMap[size].getConn(x - sX, y - sY, z - sZ, dir));
							clMap[size].setStepup(x, y, z, dir, tempMap[size].getStepup(x - sX, y - sY, z - sZ, dir));
						}
					}
					/* Update the reroute table */
					/*if (!reroute[size][y][x]) {
					 reroute[size][y][x] = numTiles + 1;
					 } else {
					 reroute[size][y][x] = ROUTING_NOT_REACHABLE;
					 }*/
				}

		g_message("Done copying data.\n");
		for (size = 0; size < 1; size++)
			for (x = minX; x <= maxX; x++)
				for (y = minY; y <= maxY; y++)
					for (z = minZ; z <= maxZ; z++) {
						pos3_t pos = { x, y, z };
						vec3_t vect;
						PosToVec(pos,vect);
						/**@todo add other data to constructor: accessibility + connection states */
						RoutingLumpEntry entry = RoutingLumpEntry(Vector3(vect), (z + 1));
						FillRoutingLumpEntry(entry, clMap, pos);
						/**@todo perhaps there is a better way than creating a const object for adding */
						const RoutingLumpEntry toAdd = RoutingLumpEntry(entry);
						routingLump.add(toAdd);
					}
		end = time(NULL);
		g_message("Loaded routing for tile %s in %5.1fs\n", name.c_str(), end - start);
	}

	void RoutingLumpLoader::loadRoutingLump (ArchiveFile& file)
	{
		/* load the file */
		InputStream &stream = file.getInputStream();
		const std::size_t size = file.size();
		byte *buf = (byte*) malloc(size + 1);
		dBspHeader_t *header = (dBspHeader_t *) buf;
		stream.read(buf, size);

		CMod_LoadRouting(_routingLump, file.getName(), &header->lumps[LUMP_ROUTING], (byte *) buf, 0, 0, 0);
		free(buf);
	}

	void RoutingLumpLoader::loadRouting (const std::string& bspFileName)
	{
		/**@todo try to reduce loading, store latest file + mtime and load only if that changed */
		// Open an ArchiveFile to load
		AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(bspFileName));
		if (file) {
			// Load the model and return the RenderablePtr
			_routingLump = RoutingLump();
			loadRoutingLump(*file);
		} else {
			gtkutil::errorDialog(_("No compiled version of the map found"));
		}
	}

	RoutingLumpLoader::RoutingLumpLoader ()
	{
	}

	routing::RoutingLump& RoutingLumpLoader::getRoutingLump ()
	{
		return _routingLump;
	}

	RoutingLumpLoader::~RoutingLumpLoader ()
	{
	}
}
