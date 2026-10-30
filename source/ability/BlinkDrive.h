/* Ability.h
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

class DataNode;



// A short-ranged teleportation device.
class BlinkDrive : public Ability {
public:
	enum BlinkType {
		// Teleport a set distance forward.
		FORWARD,
		// Teleport a set distance backward.
		BACKWARD,
		// Teleport a set distance in the direction of the next movement input.
		OMNIDIRECTIONAL,
		// Teleport to the next mouse click, limited within a given distance.
		MOUSE,
	};


public:
	BlinkDrive(const DataNode &node);

	virtual bool LoadType(const DataNode &node) override;

	// Whether this ability can be toggled by the given ship.
	virtual bool CanToggle(const Ship &ship, State state) const override;
	// Given the current state that an ability is in, return the next state that it should enter.
	virtual State NextState(const Ship &ship, State state) const override;
	// Whether this ability should be deactivated if it is currently activating or active.
	virtual bool ShouldDeactivate(const Ship &ship, State state) const override;


protected:
	// Called while the ability is in the corresponding state.
	virtual void DoActivating(Ship &ship) const override;
	virtual void DoActive(Ship &ship) const override;
	virtual void DoDeactivating(Ship &ship) const override;


private:
	BlinkType blinkType = FORWARD;
	double range = 100.;
};
