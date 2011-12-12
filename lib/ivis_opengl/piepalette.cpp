/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2011  Warzone 2100 Project

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
#include "lib/framework/frame.h"
#include "lib/framework/file.h"
#include "lib/ivis_opengl/piestate.h"
#include "lib/ivis_opengl/piepalette.h"
#include "screen.h"

PIELIGHT psPalette[WZCOL_MAX];

void pal_Init(void)
{
	char *pFileData, *ptr;
	UDWORD fileSize;
	int i, lenLeft;

	// Read these from file so that mod-makers can change them
	loadFile("palette.txt", &pFileData, &fileSize);

	ptr = pFileData;
	lenLeft = fileSize;
	for (i = 0; i < WZCOL_MAX; i++)
	{
		unsigned int r, g, b, a;
		int len;

		sscanf(ptr, "%x, %x, %x, %x %*[^\n]\n%n", &r, &g, &b, &a, &len);
		psPalette[i].byte.r = r;
		psPalette[i].byte.g = g;
		psPalette[i].byte.b = b;
		psPalette[i].byte.a = a;
		ptr += len;
		lenLeft -= len;
		ASSERT(lenLeft >= 0, "Buffer overrun reading palette data");
	}
	free(pFileData);
}

void pal_ShutDown(void)
{
	// placeholder
}

	PIELIGHT *tcolours[16] = {
		&WZCOL_TEAM1,
		&WZCOL_TEAM2,
		&WZCOL_TEAM3,
		&WZCOL_TEAM4,
		&WZCOL_TEAM5,
		&WZCOL_TEAM6,
		&WZCOL_TEAM7,
		&WZCOL_TEAM8,
		&WZCOL_TEAM9,
		&WZCOL_TEAM10,
		&WZCOL_TEAM11,
		&WZCOL_TEAM12,
		&WZCOL_TEAM13,
		&WZCOL_TEAM14,
		&WZCOL_TEAM15,
		&WZCOL_TEAM16,
	};
void pal_tweakColour(int team, int n, int delta)
{
	if (delta == 0)
		return;

	int val = tcolours[team]->vector[n];
	val = clip(val + delta, 0, 255);
	tcolours[team]->vector[n] = val;

	debug(LOG_WARNING, "WZCOL_TEAM%d = %02x,%02x,%02x,%02x", team + 1, (uint8_t)tcolours[team]->vector[0], (uint8_t)tcolours[team]->vector[1], (uint8_t)tcolours[team]->vector[2], (uint8_t)tcolours[team]->vector[3]);
}

void pal_saveTweakedColour()
{
	FILE *f = fopen("newPalette.txt", "wb");
	for (unsigned team = 0; team < 16; ++team)
	{
		fprintf(f, "%02x,%02x,%02x,%02x\t// team %d - suffusion of yellow\n", (uint8_t)tcolours[team]->vector[0], (uint8_t)tcolours[team]->vector[1], (uint8_t)tcolours[team]->vector[2], (uint8_t)tcolours[team]->vector[3], team + 1);
	}
	fclose(f);
}

PIELIGHT pal_GetTeamColour(int team)
{
	PIELIGHT tcolour;

	// set correct team colour based on team
	switch (team)
	{
		case 0: 
			tcolour = WZCOL_TEAM1; //green
			break;
		case 1:
			tcolour = WZCOL_TEAM2; //orange
			break;
		case 2:
			tcolour = WZCOL_TEAM3; //gray
			break;
		case 3:
			tcolour = WZCOL_TEAM4; //black
			break;
		case 4:
			tcolour = WZCOL_TEAM5; //red
			break;
		case 5:
			tcolour = WZCOL_TEAM6; //blue
			break;
		case 6:
			tcolour = WZCOL_TEAM7; //purple
			break;
		case 7:
			tcolour = WZCOL_TEAM8; //teal
			break;
		case  8: return WZCOL_TEAM9;
		case  9: return WZCOL_TEAM10;
		case 10: return WZCOL_TEAM11;
		case 11: return WZCOL_TEAM12;
		case 12: return WZCOL_TEAM13;
		case 13: return WZCOL_TEAM14;
		case 14: return WZCOL_TEAM15;
		case 15: return WZCOL_TEAM16;
		default:
			STATIC_ASSERT(MAX_PLAYERS <= 16);
			ASSERT(false, "Attempting to get colour for non-existing team %u", (unsigned int)team);
			tcolour = WZCOL_WHITE; //default is white
			break;
	}

	return tcolour;
}
