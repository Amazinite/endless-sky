/* EnergyLevels.h
Copyright (c) 2022 by Amazinite

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef ENERGY_LEVELS_H_
#define ENERGY_LEVELS_H_



// A class representing the the magnitude of various "energy levels"
// that a ship has, including a ship's HP, energy, heat, fuel, and
// the amounts of various DoT applied to the ship. EnergyLevels can 
// represent these such levels on a ship, or represent changes to be
// applied to a ship, such as an amount of damage to be taken or the
// resources required for thrusting or turning.
class EnergyLevels {
public:
	double hull = 0.;
	double shields = 0.;
	double energy = 0.;
	double heat = 0.;
	double fuel = 0.;

 	// Accrued "corrosion damage" that will affect a ship's hull over time.
	double corrosion = 0.;
 	// Accrued "discharge damage" that will affect a ship's shields over time.
	double discharge = 0.;
 	// Accrued "ion damage" that will affect a ship's energy over time.
	double ionization = 0.;
 	// Accrued "burn damage" that will affect a ship's heat over time.
	double burn = 0.;
 	// Accrued "leak damage" that will affect a ship's fuel over time.
	double leakage = 0.;

 	// Accrued "disruption damage" that will affect a ship's shield effectiveness over time.
	double disruption = 0.;
 	// Accrued "slowing damage" that will affect a ship's movement over time.
	double slowness = 0.;

	// A wildcard level whose purpose may change depending on how this
	// object is being used. For example, where EnergyLevels represents the
	// cost of resisting a DoT effect, the wildcard would contain the amount
	// of resistance while all other fields contain the cost for resisting
	// the DoT.
	double wildcard = 0.;
};

#endif
