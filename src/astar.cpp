/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2010  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** @file
 *  A* based path finding
 *  See http://en.wikipedia.org/wiki/A*_search_algorithm for more information.
 *  How this works:
 *  * First time (in a given tick)  that some droid  wants to pathfind  to a particular
 *    destination,  the A*  algorithm from source to  destination is used.  The desired
 *    destination,  and the nearest  reachable point  to the  destination is saved in a
 *    Context.
 *  * Second time (in a given tick)  that some droid wants to  pathfind to a particular
 *    destination,  the appropriate  Context is found,  and the A* algorithm is used to
 *    find a path from the nearest reachable point to the destination  (which was saved
 *    earlier), to the source.
 *  * Subsequent times  (in a given tick) that some droid wants to pathfind to a parti-
 *    cular destination,  the path is looked up in appropriate Context.  If the path is
 *    not already known,  the A* weights are adjusted, and the previous A*  pathfinding
 *    is continued until the new source is reached.  If the new source is  not reached,
 *    the droid is  on a  different island than the previous droid,  and pathfinding is
 *    restarted from the first step.
 *  Up to 30 pathfinding maps from A* are cached, in a LRU list. The PathNode heap con-
 *  tains the  priority-heap-sorted  nodes which are to be explored.  The path back  is
 *  stored in the PathExploredTile 2D array of tiles.
 */

#ifndef WZ_TESTING
#include "lib/framework/frame.h"

#include "astar.h"
#include "map.h"
#endif

#include <list>
#include <vector>
#include <algorithm>

/// A coordinate.
struct PathCoord
{
	PathCoord() {}
	PathCoord(int16_t x_, int16_t y_) : x(x_), y(y_) {}
	bool operator ==(PathCoord const &z) const { return x == z.x && y == z.y; }
	bool operator !=(PathCoord const &z) const { return !(*this == z); }

	int16_t x, y;
};

/** The structure to store a node of the route in node table
 *
 *  @ingroup pathfinding
 */
struct PathNode
{
	bool operator <(PathNode const &z) const
	{
		// Sort decending est, fallback to ascending dist, fallback to sorting by position.
		if (est  != z.est)  return est  > z.est;
		if (dist != z.dist) return dist < z.dist;
		if (p.x  != z.p.x)  return p.x  < z.p.x;
		                    return p.y  < z.p.y;
	}

	PathCoord p;                    // Map coords.
	unsigned  dist, est;            // Distance so far and estimate to end.
};
struct PathExploredTile
{
	PathExploredTile() : iteration(0xFFFF) {}

	uint16_t iteration;
	int8_t   dx, dy;                // Offset from previous point in the route.
	unsigned dist;                  // Shortest known distance to tile.
	bool     visited;
};

struct PathBlockingType
{
	uint32_t gameTime;

	PROPULSION_TYPE propulsion;
	int owner;
	FPATH_MOVETYPE moveType;
};
/// Pathfinding blocking map
struct PathBlockingMap
{
	bool operator ==(PathBlockingType const &z) const
	{
		return type.gameTime == z.gameTime &&
		       fpathIsEquivalentBlocking(type.propulsion, type.owner, type.moveType,
		                                    z.propulsion,    z.owner,    z.moveType);
	}

	PathBlockingType type;
	std::vector<bool> map;
};

/// Game time for all blocking maps in fpathBlockingMaps.
static uint32_t fpathCurrentGameTime;

// Data structures used for pathfinding, can contain cached results.
struct PathfindContext
{
	PathfindContext() : myGameTime(0), iteration(0), blockingMap(NULL) {}
	bool isBlocked(int x, int y) const
	{
		// Not sure whether the out-of-bounds check is needed, can only happen if pathfinding is started on a blocking tile (or off the map).
		return x < 0 || y < 0 || x >= mapWidth || y >= mapWidth || blockingMap->map[x + y*mapWidth];
	}
	bool matches(PathBlockingMap const *blockingMap_, PathCoord tileS_) const
	{
		// Must check myGameTime == fpathCurrentGameTime, otherwise blockingMap be a deleted pointer which coincidentally compares equal to the valid pointer blockingMap_.
		return myGameTime == fpathCurrentGameTime && blockingMap == blockingMap_ && tileS == tileS_;
	}
	void assign(PathBlockingMap const *blockingMap_, PathCoord tileS_)
	{
		blockingMap = blockingMap_;
		tileS = tileS_;
		myGameTime = blockingMap->type.gameTime;
		nodes.clear();

		// Make the iteration not match any value of iteration in map.
		if (++iteration == 0xFFFF)
		{
			map.clear();  // There are no values of iteration guaranteed not to exist in map, so clear the map.
			iteration = 0;
		}
		map.resize(mapWidth * mapHeight);  // Allocate space for map, if needed.
	}

	PathCoord       tileS;                // Start tile for pathfinding. (May be either source or target tile.)
	uint32_t        myGameTime;

	PathCoord       nearestCoord;         // Nearest reachable tile to destination.

	/** Counter to implement lazy deletion from map.
	 *
	 *  @see fpathTableReset
	 */
	uint16_t        iteration;

	std::vector<PathNode> nodes;        ///< Edge of explored region of the map.
	std::vector<PathExploredTile> map;  ///< Map, with paths leading back to tileS.
	PathBlockingMap const *blockingMap; ///< Map of blocking tiles for the type of object which needs a path.
};

/// Last recently used list of contexts.
static std::list<PathfindContext> fpathContexts;

/// Lists of blocking maps from current tick.
static std::list<PathBlockingMap> fpathBlockingMaps;
/// Lists of blocking maps from previous tick, will be cleared next tick (since it will be no longer needed after that).
static std::list<PathBlockingMap> fpathPrevBlockingMaps;

// Convert a direction into an offset
// dir 0 => x = 0, y = -1
static const Vector2i aDirOffset[] =
{
	{ 0, 1},
	{-1, 1},
	{-1, 0},
	{-1,-1},
	{ 0,-1},
	{ 1,-1},
	{ 1, 0},
	{ 1, 1},
};

void fpathHardTableReset()
{
	fpathContexts.clear();
	fpathBlockingMaps.clear();
	fpathPrevBlockingMaps.clear();
}

/** Get the nearest entry in the open list
 */
/// Takes the current best node, and removes from the node heap.
static inline PathNode fpathTakeNode(std::vector<PathNode> &nodes)
{
	// find the node with the lowest distance
	// if equal totals, give preference to node closer to target
	PathNode ret = nodes.front();

	// remove the node from the list
	std::pop_heap(nodes.begin(), nodes.end());  // Move the best node from the front of nodes to the back of nodes, preserving the heap properties, setting the front to the next best node.
	nodes.pop_back();                           // Pop the best node (which we will be returning).

	return ret;
}

/** Estimate the distance to the target point
 */
static inline unsigned WZ_DECL_CONST fpathEstimate(PathCoord s, PathCoord f)
{
	// Cost of moving horizontal/vertical = 70, cost of moving diagonal = 99, 99/70 = 1.41428571... ≈ √2 = 1.41421356...
	unsigned xDelta = abs(s.x - f.x), yDelta = abs(s.y - f.y);
	return std::min(xDelta, yDelta)*(99 - 70) + std::max(xDelta, yDelta)*70;
}

/** Generate a new node
 */
static inline void fpathNewNode(PathfindContext &context, PathCoord dest, PathCoord pos, unsigned prevDist, PathCoord prevPos)
{
	ASSERT((unsigned)pos.x < (unsigned)mapWidth && (unsigned)pos.y < (unsigned)mapHeight, "X (%d) or Y (%d) coordinate for path finding node is out of range!", pos.x, pos.y);

	// Create the node.
	PathNode node;
	node.p = pos;
	node.dist = prevDist + fpathEstimate(prevPos, pos);
	node.est = node.dist + fpathEstimate(pos, dest);

	PathExploredTile &expl = context.map[pos.x + pos.y*mapWidth];
	if (expl.iteration == context.iteration && (expl.visited || expl.dist <= node.dist))
	{
		return;  // Already visited this tile. Do nothing.
	}

	// Remember where we have been, and remember the way back.
	expl.iteration = context.iteration;
	expl.dx = pos.x - prevPos.x;
	expl.dy = pos.y - prevPos.y;
	expl.dist = node.dist;
	expl.visited = false;

	// Add the node to the node heap.
	context.nodes.push_back(node);                               // Add the new node to nodes.
	std::push_heap(context.nodes.begin(), context.nodes.end());  // Move the new node to the right place in the heap.
}

/// Recalculates estimates to new tileF tile.
static void fpathAStarReestimate(PathfindContext &context, PathCoord tileF)
{
	for (std::vector<PathNode>::iterator node = context.nodes.begin(); node != context.nodes.end(); ++node)
	{
		node->est = node->dist + fpathEstimate(node->p, tileF);
	}

	// Changing the estimates breaks the heap ordering. Fix the heap ordering.
	std::make_heap(context.nodes.begin(), context.nodes.end());
}

/// Returns nearest explored tile to tileF.
static PathCoord fpathAStarExplore(PathfindContext &context, PathCoord tileF)
{
	PathCoord       nearestCoord(0, 0);
	unsigned        nearestDist = 0xFFFFFFFF;

	// search for a route
	while (!context.nodes.empty())
	{
		PathNode node = fpathTakeNode(context.nodes);
		if (context.map[node.p.x + node.p.y*mapWidth].visited)
		{
			continue;  // Already been here.
		}
		context.map[node.p.x + node.p.y*mapWidth].visited = true;

		// note the nearest node to the target so far
		if (node.est - node.dist < nearestDist)
		{
			nearestCoord = node.p;
			nearestDist = node.est - node.dist;
		}

		if (node.p == tileF)
		{
			// reached the target
			nearestCoord = node.p;
			break;
		}

		// loop through possible moves in 8 directions to find a valid move
		for (unsigned dir = 0; dir < ARRAY_SIZE(aDirOffset); ++dir)
		{
			/*
			   5  6  7
			     \|/
			   4 -I- 0
			     /|\
			   3  2  1
			   odd:orthogonal-adjacent tiles even:non-orthogonal-adjacent tiles
			*/
			if (dir % 2 != 0)
			{
				int x, y;

				// We cannot cut corners
				x = node.p.x + aDirOffset[(dir + 1) % 8].x;
				y = node.p.y + aDirOffset[(dir + 1) % 8].y;
				if (context.isBlocked(x, y))
				{
					continue;
				}
				x = node.p.x + aDirOffset[(dir + 7) % 8].x;
				y = node.p.y + aDirOffset[(dir + 7) % 8].y;
				if (context.isBlocked(x, y))
				{
					continue;
				}
			}

			// Try a new location
			int x = node.p.x + aDirOffset[dir].x;
			int y = node.p.y + aDirOffset[dir].y;

			// See if the node is a blocking tile
			if (context.isBlocked(x, y))
			{
				// tile is blocked, skip it
				continue;
			}

			// Now insert the point into the appropriate list, if not already visited.
			fpathNewNode(context, tileF, PathCoord(x, y), node.dist, node.p);
		}
	}

	return nearestCoord;
}

static void fpathInitContext(PathfindContext &context, PathBlockingMap const *blockingMap, PathCoord tileS, PathCoord tileRealS, PathCoord tileF)
{
	context.assign(blockingMap, tileS);

	// Add the start point to the open list
	fpathNewNode(context, tileF, tileRealS, 0, tileRealS);
	ASSERT(!context.nodes.empty(), "fpathNewNode failed to add node.");
}

SDWORD fpathAStarRoute(MOVE_CONTROL *psMove, PATHJOB *psJob)
{
	int             retval = ASR_OK;

	bool            mustReverse = true;

	const PathCoord tileOrig(map_coord(psJob->origX), map_coord(psJob->origY));
	const PathCoord tileDest(map_coord(psJob->destX), map_coord(psJob->destY));

	PathCoord endCoord;  // Either nearest coord (mustReverse = true) or orig (mustReverse = false).

	std::list<PathfindContext>::iterator contextIterator = fpathContexts.begin();
	for (contextIterator = fpathContexts.begin(); contextIterator != fpathContexts.end(); ++contextIterator)
	{
		if (!contextIterator->matches(psJob->blockingMap, tileDest))
		{
			// This context is not for the same droid type and same destination.
			continue;
		}

		// We have tried going to tileDest before.

		if (contextIterator->map[tileOrig.x + tileOrig.y*mapWidth].iteration == contextIterator->iteration
		 && contextIterator->map[tileOrig.x + tileOrig.y*mapWidth].visited)
		{
			// Already know the path from orig to dest.
			endCoord = tileOrig;
		}
		else
		{
			// Need to find the path from orig to dest, continue previous exploration.
			fpathAStarReestimate(*contextIterator, tileOrig);
			endCoord = fpathAStarExplore(*contextIterator, tileOrig);
		}

		if (endCoord != tileOrig)
		{
			// orig turned out to be on a different island than what this context was used for, so can't use this context data after all.
			continue;
		}

		mustReverse = false;  // We have the path from the nearest reachable tile to dest, to orig.
		break;  // Found the path! Don't search more contexts.
	}

	if (contextIterator == fpathContexts.end())
	{
		// We did not find an appropriate context. Make one.

		if (fpathContexts.size() < 30)
		{
			fpathContexts.push_back(PathfindContext());
		}
		--contextIterator;

		// Init a new context, overwriting the oldest one if we are caching too many.
		// We will be searching from orig to dest, since we don't know where the nearest reachable tile to dest is.
		fpathInitContext(*contextIterator, psJob->blockingMap, tileOrig, tileOrig, tileDest);
		endCoord = fpathAStarExplore(*contextIterator, tileDest);
		contextIterator->nearestCoord = endCoord;
	}

	PathfindContext &context = *contextIterator;

	// return the nearest route if no actual route was found
	if (context.nearestCoord != tileDest)
	{
		retval = ASR_NEAREST;
	}

	// Get route, in reverse order.
	static std::vector<Vector2i> path;  // Declared static to save allocations.
	path.clear();

	PathCoord newP;
	for (PathCoord p = endCoord; p != context.tileS; p = newP)
	{
		ASSERT_OR_RETURN(ASR_FAILED, tileOnMap(p.x, p.y), "Assigned XY coordinates (%d, %d) not on map!", (int)p.x, (int)p.y);
		ASSERT_OR_RETURN(ASR_FAILED, path.size() < (unsigned)mapWidth*mapHeight, "Pathfinding got in a loop.");

		Vector2i v = {world_coord(p.x) + TILE_UNITS / 2, world_coord(p.y) + TILE_UNITS / 2};
		path.push_back(v);

		PathExploredTile &tile = context.map[p.x + p.y*mapWidth];
		newP = PathCoord(p.x - tile.dx, p.y - tile.dy);
		if (p == newP)
		{
			break;  // We stopped moving, because we reached the closest reachable tile to context.tileS. Give up now.
		}
	}
	if (path.empty())
	{
		// We are probably already in the destination tile. Go to the exact coordinates.
		Vector2i v = {psJob->destX, psJob->destY};
		path.push_back(v);
	}
	else if (retval == ASR_OK)
	{
		// Found exact path, so use exact coordinates for last point, no reason to lose precision
		Vector2i v = {psJob->destX, psJob->destY};
		if (mustReverse)
		{
			path.front() = v;
		}
		else
		{
			path.back() = v;
		}
	}

	// TODO FIXME once we can change numPoints to something larger than uint16_t
	psMove->numPoints = std::min<int>(UINT16_MAX, path.size());

	// Allocate memory
	psMove->asPath = static_cast<Vector2i *>(malloc(sizeof(*psMove->asPath) * path.size()));
	ASSERT(psMove->asPath, "Out of memory");
	if (!psMove->asPath)
	{
		fpathHardTableReset();
		return ASR_FAILED;
	}

	// get the route in the correct order
	// If as I suspect this is to reverse the list, then it's my suspicion that
	// we could route from destination to source as opposed to source to
	// destination. We could then save the reversal. to risky to try now...Alex M
	//
	// The idea is impractical, because you can't guarentee that the target is
	// reachable. As I see it, this is the reason why psNearest got introduced.
	// -- Dennis L.
	//
	// If many droids are heading towards the same destination, then destination
	// to source would be faster if reusing the information in nodeArray. --Cyp
	if (mustReverse)
	{
		// Copy the list, in reverse.
		std::copy(path.rbegin(), path.rend(), psMove->asPath);

		if (!context.isBlocked(tileOrig.x, tileOrig.y))  // If blocked, searching from tileDest to tileOrig wouldn't find the tileOrig tile.
		{
			// Next time, search starting from nearest reachable tile to the destination.
			fpathInitContext(context, psJob->blockingMap, tileDest, context.nearestCoord, tileOrig);
		}
	}
	else
	{
		// Copy the list.
		std::copy(path.begin(), path.end(), psMove->asPath);
	}

	// Move context to beginning of last recently used list.
	if (contextIterator != fpathContexts.begin())  // Not sure whether or not the splice is a safe noop, if equal.
	{
		fpathContexts.splice(fpathContexts.begin(), fpathContexts, contextIterator);
	}

	psMove->DestinationX = psMove->asPath[path.size() - 1].x;
	psMove->DestinationY = psMove->asPath[path.size() - 1].y;

	return retval;
}

class VectorMap
{
public:
	struct Point
	{
		Point() {}
		Point(int32_t x_, int32_t &y_) : x(x_), y(y_) {}
		bool operator <(Point const &b) const { if (x != b.x) return x < b.x; return y < b.y; }

		int32_t x, y;
	};
	struct Edge
	{
		Edge() {}
		Edge(Point const &p1_, Point const &p2_) : p1(p1_), p2(p2_) {}

		Point p1, p2;
	};
	struct Triangle
	{
		unsigned p[3];    // The triangle's points.
		unsigned t[3];    // Neighbouring triangles.
		bool     f[3];    // True iff edge is fixed in place.
	};

	void addLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
	void triangulate();  // Call after adding all lines.

	static const unsigned pointAtInfinity = ~0;

	std::vector<Point> points;
	std::vector<Edge> edges;
	std::vector<Triangle> triangles;
};

void VectorMap::addLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	Edge edge(Point(x1, y1), Point(x2, y2));
	points.push_back(edge.p1);
	points.push_back(edge.p2);
	edges.push_back(edge);
}

void VectorMap::triangulate()
{
	// Sort points and remove duplicates.
	std::sort(points.begin(), points.end());
	points.erase(std::unique(points.begin(), points.end()), points.end());

	ASSERT_OR_RETURN(, points.size() > 2, "Trying to triangulate nothing...");

	// Seed triangulation with 2 points and 2 triangles.
	unsigned pointsUsed = 2;
	triangles.resize(2);
	triangles[0].p[0] = pointAtInfinity;
	triangles[0].p[1] = 0;
	triangles[0].p[2] = 1;
	triangles[1].p[0] = pointAtInfinity;
	triangles[1].p[1] = 1;
	triangles[1].p[2] = 0;
	for (int e = 0; e < 3; ++e)
	{
		triangles[0].t[e] = 1;
		triangles[0].f[e] = false;
		triangles[1].t[e] = 0;
		triangles[1].f[e] = false;
	}

	while (pointsUsed < points.size())
	{
		
	}
}

static FILE *dump = NULL;
static void startLines()
{
	dump = fopen("mapDump.svg", "wb");
	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><svg xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"%d\" height=\"%d\" id=\"svg2\"><defs id=\"defs4\" /><g id=\"layer1\">", mapWidth*5, mapHeight*5);
}

static void finishLines()
{
	fprintf(dump, "</g></svg>");
	fclose(dump);
	dump = NULL;
}

static void addLine(int x1, int y1, int x2, int y2)
{
	fprintf(dump, "<path d=\"M %d,%d %d,%d\" id=\"\" style=\"fill:none;stroke:#000000;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1\" />", x1*5, y1*5, x2*5, y2*5);
}
static void addSquare(int x, int y)
{
	fprintf(dump, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" id=\"\" style=\"fill:#ff0000;fill-opacity:0.07860263;fill-rule:evenodd;stroke:none\" />", x*5, y*5, 5, 5);
}

static inline bool incrementOrFlush(bool condition, uint16_t &variable, uint16_t &ret)
{
	if (condition)
	{
		ret = 0;
		++variable;
	}
	else
	{
		ret = variable;
		variable = 0;
	}
	return ret > 0;
}

static void fpathVectoriseMap(PathBlockingType type)
{
	startLines();

	uint8_t  scan[mapWidth + 1];
	uint16_t lines[mapWidth + 2][4];  // 0 = | (left), 1 = | (right), 2 = \, 3 = /

	// Clear data.
	std::fill(scan, scan + ARRAY_SIZE(scan), 0x3F);
	memset(lines, 0x00, sizeof(lines));

	// Scan map.
	for (int y = 0; y < mapHeight + 1; ++y)
	{
		// Bits 76543210 in state correspond to blocking map tiles, with (x, y) at the top-right of tile 8:
		// 012
		// 345
		// 678
		unsigned state = 0x1FF;
		uint16_t lineUp = 0;
		uint16_t lineDown = 0;
		uint16_t linesX2 = 0;
		for (int x = 0; x < mapWidth + 1; ++x)
		{
			bool blocking = x == mapWidth || y == mapHeight || fpathBaseBlockingTile(x, y, type.propulsion, type.owner, type.moveType);
			state = (state>>1 & 0xDB) | scan[x] | blocking<<8;
if (fpathBaseBlockingTile(x, y, type.propulsion, type.owner, type.moveType)) addSquare(x, y);
			scan[x] = state>>3;
			unsigned walls = 0;
			if ((state & 1<<4) == 0)  // Don't draw lines on blocking tiles, only around them.
			{
				// The string is a 256 entry lookup table, not random line noise. Your internet connection is still working.
				walls = "\000\000AA@@\001A\004\004  D\004 `@@AA\000\000\001A\004D` \004\004 `H\bPP\b\b\020P\fLM\rL\f\rM\b\b\020IH\b\020ILLM\rL\f\rM@@\001A\000\000\001AD\004``DD\005\005\000@A\001@\000A\001DD``D\004EE\b\b\020P\bH\020P\fLMML\f\rMH\bPI\b\b\020IL\fM\r\f\f\rM\002\002\003\003B\002\003CP\020\a\aP\020\aG\002B\003CB\002\003CPFGGP\006GG` KK  KK\016NOO\016\016OO``KK  KKNNOO\016\016OOBBCCB\002CCPPGG\020P\aGB\002CC\002\002\003C\020\006\a\aP\006\aG` K\vJ\n\vK\016N\017ON\016\017O  KKJ\n\vKNN\017O\016\016\017O"[state ^ ((state>>8) * 0x110)];  // Xor bit 8 into bit 4, clearing bit 8, then use the lookup table.
			}
			// Meaning of bits in walls variable:
			//
			//  *0000*
			//  24  53
			//  2 45 3
			//  2 54 3
			//  25  43
			//  *1111*  <- (x, y) points to bottom right star. (x - 1, y - 1) points to top left star.
			//
			// Pattern is: if bit is set, add 1 to the length of the wall we will need to draw. Otherwise, draw the wall, if there is one to draw.
if (y == 2)
printf("State (%3d,%3d) = 0x%03X, walls = 0x%02X\n", x, y, state, walls);
			uint16_t length;

			lines[x][3] = lines[x + 1][3];  // Diagonal, / direction.
			if (incrementOrFlush(walls & 1<<0, lineUp, length))
			{
				addLine(x - 1 - length, y - 1, x - 1, y - 1);
			}
			if (incrementOrFlush(walls & 1<<1, lineDown, length))
			{
				addLine(x - 1 - length, y, x - 1, y);
			}
			if (incrementOrFlush(walls & 1<<2, lines[x][0], length))
			{
				addLine(x - 1, y - 1 - length, x - 1, y - 1);
			}
			if (incrementOrFlush(walls & 1<<3, lines[x][1], length))
			{
				addLine(x, y - 1 - length, x, y - 1);
			}
			if (incrementOrFlush(walls & 1<<4, linesX2, length))
			{
				addLine(x - 1 - length, y - 1 - length, x - 1, y - 1);
			}
			if (incrementOrFlush(walls & 1<<5, lines[x][3], length))
			{
				addLine(x + length, y - 1 - length, x, y - 1);
			}
			std::swap(lines[x][2], linesX2);  // Diagonal, \ direction.
		}
	}

	finishLines();
}

void fpathSetBlockingMap(PATHJOB *psJob)
{
	if (fpathCurrentGameTime != gameTime)
	{
		// New tick, remove maps which are no longer needed.
		fpathCurrentGameTime = gameTime;
		fpathPrevBlockingMaps.swap(fpathBlockingMaps);
		fpathBlockingMaps.clear();
	}

	// Figure out which map we are looking for.
	PathBlockingType type;
	type.gameTime = gameTime;
	type.propulsion = psJob->propulsion;
	type.owner = psJob->owner;
	type.moveType = psJob->moveType;

	// Find the map.
	std::list<PathBlockingMap>::iterator i = std::find(fpathBlockingMaps.begin(), fpathBlockingMaps.end(), type);
	if (i == fpathBlockingMaps.end())
	{
		// Didn't find the map, so i does not point to a map.
		fpathBlockingMaps.push_back(PathBlockingMap());
		--i;

		// i now points to an empty map with no data. Fill the map.
		i->type = type;
		std::vector<bool> &map = i->map;
		map.resize(mapWidth*mapHeight);
		for (int y = 0; y < mapHeight; ++y)
			for (int x = 0; x < mapWidth; ++x)
		{
			map[x + y*mapWidth] = fpathBaseBlockingTile(x, y, type.propulsion, type.owner, type.moveType);
		}

		fpathVectoriseMap(type);
	}

	// i now points to the correct map. Make psJob->blockingMap point to it.
	psJob->blockingMap = &*i;
}
