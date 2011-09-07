/*
 * Copyright (C) 2011 gonzoj
 *
 * Please check the CREDITS file for further information.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LEVELS_H_
#define LEVELS_H_

#include <util/types.h>

typedef struct {
	word width;
	word height;
	word x;
	word y;
	char *name;
	int type;
} level_t;

#define	LEVEL(w, h, x, y, s, t) ((level_t) { w, h, x, y, s, t })

static const level_t levels[] = {
	LEVEL(0, 0, 0, 0, "Null", 0),
	LEVEL(56, 40, -1, -1, "Rogue Encampment", 2),
	LEVEL(80, 80, -1, -1, "Blood Moor", 3),
	LEVEL(80, 80, 1000, 1000, "Cold Plains", 3),
	LEVEL(80, 80, 1000, 1000, "Stony Field", 3),
	LEVEL(80, 80, -1, -1, "Dark Wood", 3),
	LEVEL(80, 80, -1, -1, "Black Marsh", 3),
	LEVEL(80, 80, -1, -1, "Tamoe Highland", 3),
	LEVEL(200, 200, 1500, 1000, "Den of Evil", 1),
	LEVEL(200, 200, 1500, 1300, "Cave Level 1", 1),
	LEVEL(200, 200, 1500, 1600, "Underground Passage Level 1", 1),
	LEVEL(200, 200, 1500, 1900, "Hole Level 1", 1),
	LEVEL(200, 200, 1500, 2200, "Pit Level 1", 1),
	LEVEL(24, 24, 1500, 2500, "Cave Level 2", 2),
	LEVEL(24, 24, 1500, 2624, "Underground Passage Level 2", 2),
	LEVEL(24, 24, 1500, 2748, "Hole Level 2", 2),
	LEVEL(24, 24, 1500, 2872, "Pit Level 2", 2),
	LEVEL(40, 48, -1, -1, "Burial Grounds", 3),
	LEVEL(200, 200, 2000, 1000, "Crypt", 1),
	LEVEL(200, 200, 2000, 1300, "Mausoleum", 1),
	LEVEL(8, 8, 2000, 1600, "Forgotten Tower", 2),
	LEVEL(200, 200, 2500, 1000, "Tower Cellar Level 1", 1),
	LEVEL(200, 200, 2500, 1300, "Tower Cellar Level 2", 1),
	LEVEL(200, 200, 2500, 1600, "Tower Cellar Level 3", 1),
	LEVEL(200, 200, 2500, 1900, "Tower Cellar Level 4", 1),
	LEVEL(30, 30, 2500, 2200, "Tower Cellar Level 5", 2),
	LEVEL(64, 18, 3000, 1000, "Monastery Gate", 2),
	LEVEL(56, 40, 0, -40, "Outer Cloister", 2),
	LEVEL(200, 200, -1, -1, "Barracks", 1),
	LEVEL(200, 200, 3500, 1000, "Jail Level 1", 1),
	LEVEL(200, 200, 3500, 1300, "Jail Level 2", 1),
	LEVEL(200, 200, 3500, 1600, "Jail Level 3", 1),
	LEVEL(18, 20, 4000, 1000, "Inner Cloister", 2),
	LEVEL(28, 34, -4, -34, "Cathedral", 2),
	LEVEL(200, 200, 4500, 1000, "Catacombs Level 1", 1),
	LEVEL(200, 200, 4500, 1300, "Catacombs Level 2", 1),
	LEVEL(200, 200, 4500, 1600, "Catacombs Level 3", 1),
	LEVEL(22, 31, 4500, 1900, "Catacombs Level 4", 2),
	LEVEL(43, 48, 5000, 1000, "Tristram", 2),
	LEVEL(80, 80, 5000, 1148, "Moo Moo Farm", 3),
	LEVEL(56, 56, 1000, 1000, "Lut Gholein", 2),
	LEVEL(80, 80, -1, -1, "Rocky Waste", 3),
	LEVEL(80, 80, -1, -1, "Dry Hills", 3),
	LEVEL(80, 80, -1, -1, "Far Oasis", 3),
	LEVEL(80, 80, -1, -1, "Lost City", 3),
	LEVEL(32, 32, -1, -1, "Valley of Snakes", 3),
	LEVEL(80, 80, 2500, 1000, "Canyon of the Magi", 3),
	LEVEL(200, 200, 1500, 1000, "Sewers Level 1", 1),
	LEVEL(200, 200, 1500, 1300, "Sewers Level 2", 1),
	LEVEL(200, 200, 1500, 1600, "Sewers Level 3", 1),
	LEVEL(16, 19, 2000, 1000, "Harem Level 1", 2),
	LEVEL(100, 100, 2000, 1119, "Harem Level 2", 1),
	LEVEL(100, 100, 2000, 1319, "Palace Cellar Level 1 ", 1),
	LEVEL(100, 100, 2000, 1519, "Palace Cellar Level 2", 1),
	LEVEL(100, 100, 2000, 1719, "Palace Cellar Level 3", 1),
	LEVEL(200, 200, 3000, 1000, "Stony Tomb Level 1", 1),
	LEVEL(200, 200, 3000, 1300, "Halls of the Dead Level 1", 1),
	LEVEL(200, 200, 3000, 1600, "Halls of the Dead Level 2", 1),
	LEVEL(200, 200, 3000, 1900, "Claw Viper Temple Level 1", 1),
	LEVEL(200, 200, 3000, 2200, "Stony Tomb Level 2", 1),
	LEVEL(200, 200, 3000, 2500, "Halls of the Dead Level 3", 1),
	LEVEL(200, 200, 3000, 2800, "Claw Viper Temple Level 2", 1),
	LEVEL(200, 200, 3500, 1000, "Maggot Lair Level 1", 1),
	LEVEL(200, 200, 3500, 1300, "Maggot Lair Level 2", 1),
	LEVEL(200, 200, 3500, 1600, "Maggot Lair Level 3", 1),
	LEVEL(200, 200, 4000, 1000, "Ancient Tunnels", 1),
	LEVEL(200, 200, 4500, 1000, "Tal Rasha's Tomb", 1),
	LEVEL(200, 200, 4500, 1300, "Tal Rasha's Tomb", 1),
	LEVEL(200, 200, 4500, 1600, "Tal Rasha's Tomb", 1),
	LEVEL(200, 200, 4500, 1900, "Tal Rasha's Tomb", 1),
	LEVEL(200, 200, 4500, 2200, "Tal Rasha's Tomb", 1),
	LEVEL(200, 200, 4500, 2500, "Tal Rasha's Tomb", 1),
	LEVEL(200, 200, 4500, 2800, "Tal Rasha's Tomb", 1),
	LEVEL(32, 47, 4500, 3100, "Duriel's Lair", 2),
	LEVEL(200, 200, 5000, 1000, "Arcane Sanctuary", 1),
	LEVEL(64, 48, 1000, 1000, "Kurast Docktown", 2),
	LEVEL(64, 192, -1, -1, "Spider Forest", 3),
	LEVEL(64, 192, -1, -1, "Great Marsh", 3),
	LEVEL(64, 192, -1, -1, "Flayer Jungle", 3),
	LEVEL(80, 64, -1, -1, "Lower Kurast", 3),
	LEVEL(80, 64, -1, -1, "Kurast Bazaar", 3),
	LEVEL(80, 64, -1, -1, "Upper Kurast", 3),
	LEVEL(48, 16, -1, -1, "Kurast Causeway", 3),
	LEVEL(64, 64, -1, -1, "Travincal", 3),
	LEVEL(200, 200, 1500, 1000, "Spider Cave", 1),
	LEVEL(200, 200, 1500, 1300, "Spider Cavern", 1),
	LEVEL(200, 200, 2000, 1000, "Swampy Pit Level 1", 1),
	LEVEL(200, 200, 2000, 1300, "Swampy Pit Level 2", 1),
	LEVEL(200, 200, 2000, 1600, "Flayer Dungeon Level 1", 1),
	LEVEL(200, 200, 2000, 1900, "Flayer Dungeon Level 2", 1),
	LEVEL(40, 40, 2000, 2200, "Swampy Pit Level 3", 2),
	LEVEL(40, 40, 2000, 2340, "Flayer Dungeon Level 3", 2),
	LEVEL(200, 200, 2500, 1000, "Sewers Level 1", 1),
	LEVEL(18, 22, 2500, 1300, "Sewers Level 2", 2),
	LEVEL(24, 24, 3000, 1000, "Ruined Temple", 2),
	LEVEL(24, 24, 3000, 1124, "Disused Fane", 2),
	LEVEL(24, 24, 3000, 1248, "Forgotten Reliquary", 2),
	LEVEL(24, 24, 3000, 1372, "Forgotten Temple", 2),
	LEVEL(24, 24, 3000, 1496, "Ruined Fane", 2),
	LEVEL(24, 24, 3000, 1620, "Disused Reliquary", 2),
	LEVEL(200, 200, 3500, 1000, "Durance of Hate Level 1", 1),
	LEVEL(200, 200, 3500, 1300, "Durance of Hate Level 2", 1),
	LEVEL(41, 29, 3500, 1600, "Durance of Hate Level 3", 2),
	LEVEL(32, 24, 1000, 1000, "The Pandemonium Fortress", 2),
	LEVEL(80, 64, -1, -1, "Outer Steppes", 3),
	LEVEL(64, 80, -1, -1, "Plains of Despair", 3),
	LEVEL(80, 64, -1, -1, "City of the Damned", 3),
	LEVEL(200, 200, -1, -1, "River of Flame", 1),
	LEVEL(120, 120, 1500, 1000, "Chaos Sanctum", 3),
	LEVEL(40, 40, 1000, 1000, "Harrogath", 2),
	LEVEL(240, 48, 760, 1000, "Bloody Foothills", 3),
	LEVEL(-1, -1, -1, -1, "Rigid Highlands", 3),
	LEVEL(-1, -1, -1, -1, "Arreat Plateau", 3),
	LEVEL(200, 200, 2000, 1000, "Crystalized Cavern Level 1", 1),
	LEVEL(64, 64, 2000, 1300, "Cellar of Pity", 1),
	LEVEL(200, 200, 2000, 1464, "Crystalized Cavern Level 2", 1),
	LEVEL(32, 32, 2000, 1764, "Echo Chamber", 1),
	LEVEL(128, 80, 2000, 1896, "Tundra Wastelands", 3),
	LEVEL(200, 200, 2000, 2076, "Glacial Caves Level 1", 1),
	LEVEL(32, 32, 2000, 2376, "Glacial Caves Level 2", 1),
	LEVEL(20, 28, 2000, 2508, "Rocky Summit", 2),
	LEVEL(22, 27, 2000, 2636, "Nihlathaks Temple", 2),
	LEVEL(80, 80, 2000, 2763, "Halls of Anguish", 1),
	LEVEL(80, 80, 2000, 2943, "Halls of Death's Calling", 1),
	LEVEL(84, 84, 2500, 1000, "Halls of Vaught", 2),
	LEVEL(200, 200, 2500, 1184, "Hell1", 1),
	LEVEL(200, 200, 2500, 1484, "Hell2", 1),
	LEVEL(200, 200, 2500, 1784, "Hell3", 1),
	LEVEL(200, 200, 2500, 2084, "The Worldstone Keep Level 1", 1),
	LEVEL(200, 200, 2500, 2384, "The Worldstone Keep Level 2", 1),
	LEVEL(200, 200, 2500, 2684, "The Worldstone Keep Level 3", 1),
	LEVEL(40, 52, 3000, 1000, "Throne of Destruction", 2),
	LEVEL(55, 55, 3000, 1152, "The Worldstone Chamber", 2)
};

#endif /* LEVELS_H_ */
