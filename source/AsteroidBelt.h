/* AsteroidBelt.h
Copyright (c) 2021 by Amazinite

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef ASTEROID_BELT_H_
#define ASTEROID_BELT_H_

class DataNode;



// Class representing the characteristics of a belt of minable asteroids.
// AsteroidBelts are stored in WeightedLists by System, and therefore also
// have a weight.
class AsteroidBelt {
public:
	AsteroidBelt();
	AsteroidBelt(const DataNode &node, int valueIndex);
	
	double Radius() const;
	int Weight() const;
	
	double GetEccentricity() const;
	double GetOffset() const;
	
	double MinPeriapsis() const;
	double MaxPeriapsis() const;
	double MinApoapsis() const;
	double MaxApoapsis() const;
	
private:
	double radius = 1500.;
	int weight = 1;
	
	double minEccentricity = 0.;
	double maxEccentricity = 0.6;
	
	double minPeriapsis = 0.4;
	double maxPeriapsis = 1.3;
	double minApoapsis = 0.8;
	double maxApoapsis = 4.0;
	
	double minOffset = 0.;
	double maxOffset = 0.;
};



#endif
