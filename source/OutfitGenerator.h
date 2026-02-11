/* OutfitGenerator.h
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

#include "ConditionSet.h"
#include "LocationFilter.h"
#include "ship/ResourceLevels.h"

#include <map>
#include <optional>
#include <set>
#include <utility>

class CargoHold;
class ConditionsStore;
class DataNode;
class Outfit;
class Planet;
class PlayerInfo;
class System;



// A class representing a component of an outfit that generates other outfits.
class OutfitGenerator {
public:
	// Events that occur this generator to create new outfits.
	enum class Trigger {
		// When landing on a planet with an outfitter.
		OUTFITTER,
		// On any landing.
		LANDING,
		// On every day change.
		DAILY,
		// After a timer has elapsed.
		TIMER,
	};

	// The location of where to place the new outfits,
	// or where to pull outfits from that are a part of the cost.
	enum class Location {
		INSTALL,
		CARGO
	};

	// How the cost is applied when generating outfits.
	enum class Cost {
		PER_ITEM,
		PER_CYCLE
	};


public:
	OutfitGenerator() = default;
	OutfitGenerator(const DataNode &node, const ConditionsStore *playerConditions,
		const std::set<const System *> *visitedSystems, const std::set<const Planet *> *visitedPlanets);
	void Load(const DataNode &node, const ConditionsStore *playerConditions,
		const std::set<const System *> *visitedSystems, const std::set<const Planet *> *visitedPlanets);

	Trigger GetTrigger() const;
	const Outfit *GetOutfit() const;
	int TimerTicks() const;

	// Whether this generator is currently active for the player.
	bool IsActive(const PlayerInfo &player) const;
	// Whether the presence of this generator should cause its outfit to stock the local outfitter.
	bool StocksOutfitter() const;

	// Add generated outfits to the given ship or cargo hold.
	// Returns true if anything was generated.
	bool Generate(Ship &ship, CargoHold &cargo, PlayerInfo *player) const;


private:
	// Returns how many cycles can be generated and the size of each cycle.
	std::pair<int, int> CanGenerate(const Ship &ship, const CargoHold &cargo, const PlayerInfo *player) const;


private:
	// What will be generated, how much, and where it will be sent to.
	const Outfit *outfit = nullptr;
	// An empty optional means to generate as much as will fit.
	std::optional<int> count;
	Location generateLocation = Location::INSTALL;

	// When and where it will be generated.
	Trigger trigger = Trigger::OUTFITTER;
	int timerTicks = 0;
	LocationFilter activateLocation;
	ConditionSet activateConditions;
	LocationFilter deactivateLocation;
	ConditionSet deactivateConditions;

	// How much it costs per item or cycle to generate.
	Cost costType = Cost::PER_ITEM;
	int64_t creditCost = 0;
	std::map<const Outfit *, int> outfitCost;
	Location costLocation = Location::INSTALL;
	ResourceLevels resourceCost;

	// Whether the local outfitter should be stocked
	// with the outfit that this generates.
	bool stocksOutfitter = false;
};
