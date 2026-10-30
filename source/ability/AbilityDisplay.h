/* AmmoDisplay.h
Copyright (c) 2022 by warp-core

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "../ClickZone.h"

#include <map>
#include <string>
#include <vector>

class AbilityTracker;
class PlayerInfo;
class Point;
class Rectangle;
class Ship;



// A class for handling the ability icons displayed in the HUD.
class AbilityDisplay {
public:
	explicit AbilityDisplay(PlayerInfo &player);
	void Reset();
	void Update(const Ship &flagship);
	void Draw(const Rectangle &abilityBox, const Point &iconDimensions) const;
	bool Click(const Point &clickPoint, bool control) const;
	bool Click(const Rectangle &clickBox) const;


private:
	PlayerInfo &player;

	std::map<const AbilityTracker *, std::string> abilities;
	mutable std::vector<ClickZone<const AbilityTracker *>> iconZones;
};
