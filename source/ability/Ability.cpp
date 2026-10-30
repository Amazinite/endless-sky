/* Ability.cpp
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

#include "Ability.h"

#include "../audio/Audio.h"
#include "../DataNode.h"
#include "../Effect.h"
#include "../GameData.h"
#include "../image/SpriteSet.h"

using namespace std;



Ability::Ability(Type type, Activation activation, SpendsCost spendsCost, bool stackable)
	: type(type), activation(activation), spendsCost(spendsCost), stackable(stackable)
{
}



void Ability::Load(const DataNode &node)
{
	for(const DataNode &child: node)
	{
		if(activation != PASSIVE)
		{
			if(!LoadType(child))
				child.PrintTrace("Skipping unrecognized attribute:");
			continue;
		}
		const string &key = child.Token(0);
		bool hasValue = child.Size() >= 2;
		if(key == "icon" && hasValue)
			icon = SpriteSet::Get(child.Token(1));
		else if(key == "activation sound" && hasValue)
			activationSound = Audio::Get(child.Token(1));
		else if(key == "live sound" && hasValue)
			liveSound = Audio::Get(child.Token(1));
		else if(key == "deactivation sound" && hasValue)
			deactivationSound = Audio::Get(child.Token(1));
		else if(key == "activation sparks" && hasValue)
			activationSparks[GameData::Effects().Get(child.Token(1))] = child.Size() >= 3 ? child.Value(2) : 1.;
		else if(key == "live sparks" && hasValue)
			liveSparks[GameData::Effects().Get(child.Token(1))] = child.Size() >= 3 ? child.Value(2) : 1.;
		else if(key == "deactivation sparks" && hasValue)
			deactivationSparks[GameData::Effects().Get(child.Token(1))] = child.Size() >= 3 ? child.Value(2) : 1.;
		else if(key == "cost")
			cost.Load(child);
		else if(key == "cooldown" && hasValue)
			cooldown = child.Value(1);
		else if(key == "activation duration" && hasValue && activation != SINGLE_USE)
			activationDuration = child.Value(1);
		else if(key == "active duration" && hasValue && activation != SINGLE_USE && activation != TOGGLE)
			activeDuration = child.Value(1);
		else if(key == "deactivation duration" && hasValue && activation != SINGLE_USE)
			deactivationDuration = child.Value(1);
		else if(!LoadType(child))
			child.PrintTrace("Skipping unrecognized attribute:");
	}
}



Ability::Type Ability::GetType() const
{
	return type;
}



Ability::Activation Ability::GetActivation() const
{
	return activation;
}



Ability::SpendsCost Ability::GetSpendsCost() const
{
	return spendsCost;
}



bool Ability::CanStack(const Ability &other) const
{
	if(!stackable || !other.stackable)
		return false;
	if(type != other.type)
		return false;
	if(activation != other.activation)
		return false;
	if(cooldown != other.cooldown)
		return false;
	if(activationDuration != other.activationDuration)
		return false;
	if(activeDuration != other.activeDuration)
		return false;
	if(deactivationDuration != other.deactivationDuration)
		return false;
	return icon == other.icon;
}



const Sprite *Ability::Icon() const
{
	return icon;
}



const Sound *Ability::GetActivationSound() const
{
	return activationSound;
}



const Sound *Ability::GetLiveSound() const
{
	return liveSound;
}



const Sound *Ability::GetDeactivationSound() const
{
	return deactivationSound;
}



const std::map<const Effect *, double> &Ability::GetActivationSparks() const
{
	return activationSparks;
}



const std::map<const Effect *, double> &Ability::GetLiveSparks() const
{
	return liveSparks;
}



const std::map<const Effect *, double> &Ability::GetDeactivationSparks() const
{
	return deactivationSparks;
}



const ResourceLevels &Ability::Cost() const
{
	return cost;
}



int Ability::Duration(State state) const
{
	switch(state)
	{
		case INACTIVE:
			return cooldown;
		case ACTIVATING:
			return activationDuration;
		case ACTIVE:
			return activeDuration;
		case DEACTIVATING:
			return deactivationDuration;
		default:
			return 0;
	}
}
