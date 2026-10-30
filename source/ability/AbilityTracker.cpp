/* AbilityState.cpp
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

#include "AbilityTracker.h"

#include "../ship/ResourceLevels.h"
#include "../Ship.h"

using namespace std;



AbilityTracker::AbilityTracker(const Ability &ability)
{
	AddAbility(ability);
	activation = ability.GetActivation();
	spendsCost = ability.GetSpendsCost();
	icon = ability.Icon();
}



bool AbilityTracker::AddAbility(const Ability &ability)
{
	if(abilities.back()->CanStack(ability))
	{
		abilities.push_back(&ability);
		return true;
	}
	return false;
}



Ability::State AbilityTracker::State() const
{
	return state;
}



Ability::Type AbilityTracker::Type() const
{
	return abilities.back()->GetType();
}



const std::vector<const Ability *> &AbilityTracker::Abilities() const
{
	return abilities;
}



const Sprite *AbilityTracker::Icon() const
{
	return icon;
}



std::string AbilityTracker::StateString(const Ship &ship) const
{
	switch(activation)
	{
		case Ability::Activation::SINGLE_USE:
		{
			if(cooldown)
				return "on cooldown";
			if(!ship.AvailableResources().CanExpend(Cost()))
				return "low resources";
			return "ready";
		}
		default:
		{
			if(cooldown)
				return "on cooldown";
			if(state == Ability::State::ACTIVATING)
				return "activating";
			if(state == Ability::State::ACTIVE)
				return "active";
			if(state == Ability::State::DEACTIVATING)
				return "deactivating";
			return "ready";
		}
	}
}



bool AbilityTracker::CanToggle(const Ship &ship) const
{
	for(const Ability *ability : abilities)
		if(!ability->CanToggle(ship, state))
			return false;
	bool canToggle = true;
	switch(activation)
	{
		case Ability::Activation::PASSIVE:
			canToggle = false;
		case Ability::Activation::SINGLE_USE:
			canToggle = !cooldown;
		case Ability::Activation::TIMED:
			canToggle = !cooldown && !duration;
		default:
			canToggle = !cooldown;
	}
	if(abilities.back()->NextState(ship, state) == Ability::State::ACTIVE
			&& spendsCost == Ability::SpendsCost::ON_ACTIVATION)
		canToggle &= ship.AvailableResources().CanExpend(Cost());
	return canToggle;
}



void AbilityTracker::Toggle(Ship &ship)
{
	// All abilities in this state share the same cooldowns and durations,
	// so just grab the last one and use it to determine the toggling behavior.
	const Ability *ability = abilities.back();
	state = ability->NextState(ship, state);
	if(state == Ability::State::INACTIVE)
		cooldown = ability->Duration(state);
	else
		duration = ability->Duration(state);
	if(state == Ability::State::ACTIVE && spendsCost == Ability::SpendsCost::ON_ACTIVATION)
		ship.SpendResources(Cost());
}



void AbilityTracker::Step(Ship &ship)
{
	switch(state)
	{
		case Ability::State::INACTIVE:
		{
			// Stepping an inactive ability will only decrease its cooldown timer.
			if(cooldown)
				--cooldown;
			break;
		}
		case Ability::State::ACTIVATING:
		{
			// Determine if the ability should be forced to deactivate.
			for(const Ability *ability : abilities)
				if(ability->ShouldDeactivate(ship, state))
				{
					Toggle(ship);
					return;
				}
			for(const Ability *ability : abilities)
				ability->Activating(ship);
			// Determine when the ACTIVATING state converts into ACTIVE.
			if(duration && !--duration)
				Toggle(ship);
			break;
		}
		case Ability::State::ACTIVE:
		{
			// First determine if the ability cost can be spent this frame.
			// If not, the ability will be forced to deactivate.
			if(spendsCost == Ability::SpendsCost::WHILE_ACTIVE)
			{
				ResourceLevels cost = Cost();
				if(ship.AvailableResources().CanExpend(cost))
					ship.SpendResources(cost);
				else
				{
					Toggle(ship);
					return;
				}
			}
			// Next, determine if the ability should be forced to deactivate
			// for other reasons.
			for(const Ability *ability : abilities)
				if(ability->ShouldDeactivate(ship, state))
				{
					Toggle(ship);
					return;
				}
			// The ability will remain active this frame.
			for(const Ability *ability : abilities)
				ability->Active(ship);
			// Determine when the ACTIVE state converts into DEACTIVATING or INACTIVE.
			if(duration && !--duration)
				Toggle(ship);
			break;
		}
		case Ability::State::DEACTIVATING:
		{
			for(const Ability *ability : abilities)
				ability->Deactivating(ship);
			// Determine when the DEACTIVATING state converts into INACTIVE.
			if(duration && !--duration)
				Toggle(ship);
			break;
		}
	}
}



ResourceLevels AbilityTracker::Cost() const
{
	ResourceLevels cost;
	for(const Ability *ability : abilities)
		cost += ability->Cost();
	return cost;
}
