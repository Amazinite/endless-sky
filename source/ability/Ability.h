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

#include "../ship/ResourceLevels.h"

#include <map>
#include <string>

class DataNode;
class Effect;
class Ship;
class Sound;
class Sprite;



// An ability is some special action that can be activated or toggled by a ship.
class Ability {
public:
	// Implementation of ability behavior is controlled by subtypes.
	enum Type {
		// This ability only modifies the ship's resources, such as expending
		// a large amount of energy to restore shield strength.
		RESOURCE,
		// A short-ranged teleportation device.
		BLINK_DRIVE,
	};

	// Describes how the ability behaves when activated.
	enum Activation {
		// This ability doesn't appear in the HUD and can't be activated or deactivated.
		// It just grants some passive capability to the ship.
		PASSIVE,
		// This ability does something the moment it's activated.
		SINGLE_USE,
		// This ability can be toggled on or off at any time.
		TOGGLE,
		// This ability will only be active for a limited time when activated.
		TIMED,
		// This ability can be toggled on for a limited time, but can be toggled off at any time.
		TIMED_TOGGLE,
	};

	// Describes how the ability's cost is spent.
	enum SpendsCost {
		// The cost is expended one time at the moment of activation.
		ON_ACTIVATION,
		// The cost is spent every frame that the ability is active.
		WHILE_ACTIVE,
	};

	enum State {
		INACTIVE,
		// Moving from a state of inactive to active.
		ACTIVATING,
		ACTIVE,
		// Moving from a state of active to inactive.
		DEACTIVATING,
	};


public:
	Ability(Type type, Activation activation, SpendsCost spendsCost, bool stackable);
	virtual ~Ability() = default;

	// Load the generic information about the ability.
	void Load(const DataNode &node);
	// Load the type-specific information about the ability.
	// Returns true if the node was recognized.
	virtual bool LoadType(const DataNode &node) = 0;

	// Behavioral information of this ability.
	Type GetType() const;
	Activation GetActivation() const;
	SpendsCost GetSpendsCost() const;

	// Whether two abilities can be stacked into the same state tracker.
	bool CanStack(const Ability &other) const;

	// The icon of this ability in the HUD.
	const Sprite *Icon() const;

	// The sound that is played when the ability is activated, currently active, or deactivated.
	const Sound *GetActivationSound() const;
	const Sound *GetLiveSound() const;
	const Sound *GetDeactivationSound() const;

	// The effects that are created as sparks when the ability is activated, currently active, or deactivated.
	const std::map<const Effect *, double> &GetActivationSparks() const;
	const std::map<const Effect *, double> &GetLiveSparks() const;
	const std::map<const Effect *, double> &GetDeactivationSparks() const;
	// The number of frames that activation or deactivation sparks are created.
	int GetActivationSparkFrames() const;
	int GetDeactivationSparkFrames() const;

	// The cost of using this ability.
	const ResourceLevels &Cost() const;
	// How many frames the ability will be in the given state.
	int Duration(State state) const;

	// Whether this ability can be toggled by the given ship.
	virtual bool CanToggle(const Ship &ship, State state) const = 0;
	// Given the current state that an ability is in, return the next state that it should enter.
	virtual State NextState(const Ship &ship, State state) const = 0;
	// Whether this ability should be deactivated if it is currently activating or active.
	virtual bool ShouldDeactivate(const Ship &ship, State state) const = 0;
	// Called while the ability is in the corresponding state.
	void Activating(Ship &ship) const;
	void Active(Ship &ship) const;
	void Deactivating(Ship &ship) const;


protected:
	virtual void DoActivating(Ship &ship) const = 0;
	virtual void DoActive(Ship &ship) const = 0;
	virtual void DoDeactivating(Ship &ship) const = 0;


protected:
	Type type;
	Activation activation;
	SpendsCost spendsCost;

	bool stackable = false;

	const Sprite *icon = nullptr;

	const Sound *activationSound = nullptr;
	const Sound *liveSound = nullptr;
	const Sound *deactivationSound = nullptr;

	std::map<const Effect *, double> activationSparks;
	std::map<const Effect *, double> liveSparks;
	std::map<const Effect *, double> deactivationSparks;

	ResourceLevels cost;
	int cooldown = 0;
	int activationDuration = 0;
	int activeDuration = 0;
	int deactivationDuration = 0;
};
