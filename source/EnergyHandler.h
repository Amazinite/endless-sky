/* EnergyHandler.h
Copyright (c) 2022 by Amazinite

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef ENERGY_HANDLER_H_
#define ENERGY_HANDLER_H_

#include "EnergyLevels.h"

class Outfit;
class Ship;
class Weapon;



// A class which handles various aspects of a ship's energy levels,
// including taking damage, doing repairs, and calculating fractional
// thrust or turn values.
class EnergyHandler {
public:
	// Update the stored EnergyLevels for various actions a
	// ship can take (e.g. regenerating shields, thrusting).
	void Update(const Outfit &attributes);

	// Clear all levels of input and set hull to -1.
	void Kill(EnergyLevels &input) const;
	// Clear the damage over time levels of the input.
	void ClearDoT(EnergyLevels &input) const;

	// Repair the given stat up to the maximum given the energy input and cost.
	// Updates the available variable with the remaining amount of repairs that
	// can be done.
	void DoRepair(double &stat, double &available, double maximum, EnergyLevels &input, const EnergyLevels &cost) const;
	// Apply status effects and DoT resistances to the input.
	void DoStatusEffects(EnergyLevels &input, bool disabled) const;

	// Return true if the given input has the energy to expend on the cost. Does
	// not check DoT levels.
	bool CanExpendSimple(const EnergyLevels &input, const EnergyLevels &cost) const;
	// Return true if the given input has the energy to expend on the entire cost.
	bool CanExpend(const EnergyLevels &input, const EnergyLevels &cost) const;
	// Return true if the given input has the energy to expend on the firing cost.
	// This ignores any shield costs, allowing ships to fire a weapon even with
	// no shields. This also prevents a ship from disabling itself as a result
	// of any firing hull cost.
	bool CanFire(const EnergyLevels &input, const EnergyLevels &cost, double minHull) const;

	// Return the amount of value that the given input can output
	// given the maximum possible output and its cost.
	double FractionalUsage(const EnergyLevels &input, const EnergyLevels &cost, double output) const;

	// Apply damage * scale to the input. Hull, shields, energy, and fuel
	// are subtracted from input while all other levels are added to input.
	void Damage(EnergyLevels &input, const EnergyLevels &damage, double scale = 1.) const;

	// Construct an EnergyLevels object for the firing cost of the given weapon
	// when fired from the given ship.
	EnergyLevels FiringCost(const Weapon &weapon, const Ship &ship) const;


private:
	// Update the stored capacity for various EnergyLevels on a ship.
	void Capacity(const Outfit &attributes);

	// Update the stored EnergyLevels for each action a ship can take.
	void HullRepair(const Outfit &attributes);
	void ShieldRegen(const Outfit &attributes);

	void CorrosionResist(const Outfit &attributes);
	void DischargeResist(const Outfit &attributes);
	void IonizationResist(const Outfit &attributes);
	void BurnResist(const Outfit &attributes);
	void LeakageResist(const Outfit &attributes);
	void DisruptionResist(const Outfit &attributes);
	void SlownessResist(const Outfit &attributes);

	void Thrust(const Outfit &attributes);
	void Turn(const Outfit &attributes);
	void ReverseThrust(const Outfit &attributes);
	void AfterburnerThrust(const Outfit &attributes);

	void Cloak(const Outfit &attributes);


private:
	// A ship can freely access the EnergyLevels of its own handler.
	friend class Ship;

	EnergyLevels capacity;

	EnergyLevels hullRepair;
	EnergyLevels shieldRegen;

	EnergyLevels corrosionResist;
	EnergyLevels dischargeResist;
	EnergyLevels ionizationResist;
	EnergyLevels burnResist;
	EnergyLevels leakageResist;
	EnergyLevels disruptionResist;
	EnergyLevels slownessResist;

	EnergyLevels thrust;
	EnergyLevels turn;
	EnergyLevels reverseThrust;
	EnergyLevels afterburnerThrust;

	EnergyLevels cloak;
};

#endif
