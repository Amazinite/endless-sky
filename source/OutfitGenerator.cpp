/* OutfitGenerator.cpp
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

#include "OutfitGenerator.h"

#include "CargoHold.h"
#include "DataNode.h"
#include "GameData.h"
#include "Logger.h"
#include "PlayerInfo.h"
#include "Ship.h"

#include <limits>

using namespace std;



OutfitGenerator::OutfitGenerator(const DataNode &node, const ConditionsStore *playerConditions,
		const set<const System *> *visitedSystems, const set<const Planet *> *visitedPlanets)
{
	Load(node, playerConditions, visitedSystems, visitedPlanets);
}



void OutfitGenerator::Load(const DataNode &node, const ConditionsStore *playerConditions,
		const set<const System *> *visitedSystems, const set<const Planet *> *visitedPlanets)
{
	if(node.Size() < 2)
		return;
	outfit = GameData::Outfits().Get(node.Token(1));
	if(node.Size() >= 3)
		count = node.Value(2);

	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		bool hasValue = child.Size() >= 2;
		if(key == "install")
			generateLocation = Location::INSTALL;
		else if(key == "cargo")
			generateLocation = Location::CARGO;
		else if(key == "stocks outfitter")
			stocksOutfitter = true;
		else if(key == "outfitter")
			trigger = Trigger::OUTFITTER;
		else if(key == "landing")
			trigger = Trigger::LANDING;
		else if(key == "daily")
			trigger = Trigger::DAILY;
		else if(key == "timer" && hasValue)
		{
			trigger = Trigger::TIMER;
			timerTicks = child.Value(1);
		}
		else if(key == "cost")
		{
			if(hasValue)
			{
				const string &value = child.Token(1);
				if(value == "per item")
					costType = Cost::PER_ITEM;
				else if(value == "per cycle")
					costType = Cost::PER_CYCLE;
			}

			for(const DataNode &grand : child)
			{
				const string &grandKey = child.Token(0);
				if(key == "from installed")
					costLocation = Location::INSTALL;
				else if(key == "from cargo")
					costLocation = Location::CARGO;
				else if(grand.Size() < 2)
					child.PrintTrace("Skipping attribute with no value specified:");
				else if(grandKey == "credits")
					creditCost = grand.Value(1);
				else if(grandKey == "outfit")
				{
					int costCount =  child.Size() >= 3 ? max<int>(child.Value(2), 1) : 1;
					outfitCost[GameData::Outfits().Get(child.Token(1))] = costCount;
				}
				else
					resourceCost.LoadSingle(grand);
			}
		}
		else if(key == "activation")
		{
			for(const DataNode &grand : child)
			{
				const string &grandKey = child.Token(0);
				if(grandKey == "location")
					activateLocation.Load(grand, visitedSystems, visitedPlanets);
				else if(grandKey == "conditions")
					activateConditions.Load(grand, playerConditions);
				else
					child.PrintTrace("Skipping unrecognized attribute:");
			}
		}
		else if(key == "deactivation")
		{
			for(const DataNode &grand : child)
			{
				const string &grandKey = child.Token(0);
				if(grandKey == "location")
					deactivateLocation.Load(grand, visitedSystems, visitedPlanets);
				else if(grandKey == "conditions")
					deactivateConditions.Load(grand, playerConditions);
				else
					child.PrintTrace("Skipping unrecognized attribute:");
			}
		}
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



OutfitGenerator::Trigger OutfitGenerator::GetTrigger() const
{
	return trigger;
}



const Outfit *OutfitGenerator::GetOutfit() const
{
	return outfit;
}



int OutfitGenerator::TimerTicks() const
{
	return timerTicks;
}



bool OutfitGenerator::IsActive(const PlayerInfo &player) const
{
	if(!activateConditions.Test() || (!deactivateConditions.IsEmpty() && deactivateConditions.Test()))
		return false;
	if(activateLocation.IsEmpty() && deactivateConditions.IsEmpty())
		return true;
	const Planet *planet = player.GetPlanet();
	if(planet)
	{
		return (activateLocation.IsEmpty() || activateLocation.Matches(planet))
			&& (deactivateLocation.IsEmpty() || !deactivateLocation.Matches(planet));
	}
	const System *system = player.GetSystem();
	return (activateLocation.IsEmpty() || activateLocation.Matches(system))
		&& (deactivateLocation.IsEmpty() || !deactivateLocation.Matches(system));
}



bool OutfitGenerator::StocksOutfitter() const
{
	return stocksOutfitter;
}



bool OutfitGenerator::Generate(Ship &ship, CargoHold &cargo, PlayerInfo *player) const
{
	Logger::Log("OutfitGenerator::Generate", Logger::Level::INFO);
	if(!outfit || !outfit->IsDefined())
		return false;
	if(player && !IsActive(*player))
		return false;

	auto [cycles, cycleSize] = CanGenerate(ship, cargo, player);
	Logger::Log(to_string(cycles) + " " + to_string(cycleSize), Logger::Level::INFO);
	if(!cycles)
		return false;

	if(player && creditCost)
		player->Accounts().AddCredits(-creditCost * cycles);
	ship.TakeDamage(resourceCost * cycles);
	for(const auto &[outfit, cost] : outfitCost)
		ship.AddOutfit(outfit, -cost * cycles);

	if(generateLocation == Location::INSTALL)
		ship.AddOutfit(outfit, cycles * cycleSize);
	else
		cargo.Add(outfit, cycles * cycleSize);
	return true;
}



pair<int, int> OutfitGenerator::CanGenerate(const Ship &ship, const CargoHold &cargo, const PlayerInfo *player) const
{
	Logger::Log("\tOutfitGenerator::CanGenerate", Logger::Level::INFO);
	// Determine up to how many "cycles" we can generate.
	// A cycle is a single application of the cost of generating outfits.
	// The cycle size is how many outfits will be generated per cycle.
	double cycles;
	int cycleSize = 1;
	// If no outfit count was provided, then attempt to generate as many
	// outfits as this ship can handle. Each cycle is only a single item.
	if(!count.has_value())
		cycles = numeric_limits<double>::infinity();
	// If the cost type is per-item, then we're allowed to generate less than the
	// full amount if possible, as long as we can still generate at least one.
	else if(costType == Cost::PER_ITEM)
		cycles = *count;
	// If the cost type is per-cycle, then either the whole cycle is able
	// to be generated at once, or nothing gets generated.
	else
	{
		cycles = 1.;
		cycleSize = *count;
	}

	// How many cycles is this ship able to pay for?
	if(player && creditCost)
		cycles = min<double>(cycles, player->Accounts().Credits() / creditCost);
	cycles = min<double>(cycles, ship.AvailableResources().MultipleUsage(resourceCost));
	for(const auto &[outfit, count] : outfitCost)
	{
		if(costLocation == Location::INSTALL)
			cycles = min<double>(cycles, ship.OutfitCount(outfit) / count);
		else
			cycles = min<double>(cycles, cargo.Get(outfit) / count);
	}
	if(cycles < 1.)
		return make_pair(0, 0);

	// How many outfits can actually fit in the target location?
	int outfitCount = (cycles == numeric_limits<int>::infinity()) ? numeric_limits<int>::max() : cycleSize * cycles;
	if(generateLocation == Location::INSTALL)
		outfitCount = ship.Attributes().CanAdd(*outfit, outfitCount);
	else if(outfit->Mass())
		outfitCount = cargo.Free() / (outfit->Mass() * outfitCount);

	int finalCycles;
	if(outfitCount == numeric_limits<int>::max())
	{
		Logger::Log("Attempted to generate the outfit " + outfit->TrueName() + " for the "
			+ ship.TrueModelName() + " \"" + ship.GivenName() + "\", but this generator is configured to create "
			"as many outfits as possible and the costs of the generator and space on the ship have not limited "
			"how many outfits could be generated. Limiting to 1.", Logger::Level::WARNING);
		finalCycles = 1;
	}
	else if(outfitCount < cycleSize)
		finalCycles = 0;
	else if(cycleSize == 1)
		finalCycles = outfitCount;
	else
		finalCycles = 1;
	return make_pair(finalCycles, cycleSize);
}
