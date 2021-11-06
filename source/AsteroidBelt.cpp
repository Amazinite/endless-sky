/* AsteroidBelt.cpp
Copyright (c) 2021 by Amazinite

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "AsteroidBelt.h"

#include "DataNode.h"
#include "Random.h"

using namespace std;



AsteroidBelt::AsteroidBelt()
{
}



AsteroidBelt::AsteroidBelt(const DataNode &node, int valueIndex)
{
	radius = node.Value(valueIndex);
	if(node.Size() >= valueIndex + 1)
		weight = max<int>(1, node.Value(valueIndex + 1));
	
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		bool hasValue = child.Size() >= 2;
		bool hasSecondValue = child.Size() >= 3;
		double value;
		double secondValue;
		
		if(!hasValue)
		{
			child.PrintTrace("Expected key to have a value:");
			continue;
		}
		value = child.Value(1);
		
		if(hasSecondValue)
			secondValue = child.Value(2);
		
		if(key == "eccentricity")
		{
			if(hasSecondValue)
			{
				minEccentricity = max(0., min(value, 0.99));
				maxEccentricity = max(minEccentricity, min(secondValue, 0.99));
			}
			else
				maxEccentricity = max(0., min(value, 0.99));
		}
		else if(key == "offset")
		{
			if(hasSecondValue)
			{
				minOffset = min(max(-radius, value), 0.);
				maxOffset = max(0., secondValue);
			}
			else
				maxOffset = max(0., value);
		}
		else if(!hasSecondValue)
		{
			child.PrintTrace("Expected key to have a second value:");
		}
		else if(key == "periapsis")
		{
			minPeriapsis = max(0., value);
			maxPeriapsis = max(minPeriapsis, secondValue);
		}
		else if(key == "apoapsis")
		{
			minApoapsis = max(0., value);
			maxApoapsis = max(minApoapsis, secondValue);
		}
	}
}



double AsteroidBelt::Radius() const
{
	return radius;
}



int AsteroidBelt::Weight() const
{
	return weight;
}



double AsteroidBelt::GetEccentricity() const
{
	return minEccentricity + Random::Real() * (maxEccentricity - minEccentricity);
}



double AsteroidBelt::GetOffset() const
{
	return minOffset + Random::Real() * (maxOffset - minOffset);
}



double AsteroidBelt::MinPeriapsis() const
{
	return minPeriapsis;
}



double AsteroidBelt::MaxPeriapsis() const
{
	return maxPeriapsis;
}



double AsteroidBelt::MinApoapsis() const
{
	return minApoapsis;
}



double AsteroidBelt::MaxApoapsis() const
{
	return maxApoapsis;
}
