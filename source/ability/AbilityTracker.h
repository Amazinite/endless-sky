/* AbilityState.h
Copyright (c) 2026 by Amazinite

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

#include "Ability.h"

#include <string>
#include <vector>

class ResourceLevels;
class Ship;
class Sprite;



// Tracks the state of an ability on a ship.
// AbilityTracker is to Ability as Armament/Hardpoint is to Weapon.
class AbilityTracker {
public:
	explicit AbilityTracker(const Ability &ability);
	// Try to stack the given ability into this tracker.
	// Returns true if it was accepted.
	bool AddAbility(const Ability &ability);

	Ability::State State() const;
	Ability::Type Type() const;
	const std::vector<const Ability *> &Abilities() const;

	const Sprite *Icon() const;
	std::string StateString(const Ship &ship) const;

	bool CanToggle(const Ship &ship) const;
	void Toggle(Ship &ship);
	void Step(Ship &ship);


private:
	ResourceLevels Cost() const;


private:
	// The abilities that this state is tracking.
	// If the ability is not of a stackable type, then this will only contain one ability.
	std::vector<const Ability *> abilities;

	Ability::Activation activation;
	Ability::SpendsCost spendsCost;
	const Sprite *icon;

	// Whether this ability is currently active.
	Ability::State state = Ability::State::INACTIVE;
	// The remaining duration of the current state.
	int duration = 0;
	// The cooldown of this ability before it can be used again.
	int cooldown = 0;
};
