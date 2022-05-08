/* EnergyHandler.cpp
Copyright (c) 2022 by Amazinite

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "EnergyHandler.h"

#include "Outfit.h"

#include <cmath>

using namespace std;



// Update the stored EnergyLevels for various actions a
// ship can take (e.g. regenerating shields, thrusting).
void EnergyHandler::Update(const Outfit &attributes)
{
	HullRepair(attributes);
	ShieldRegen(attributes);

	CorrosionResist(attributes);
	DischargeResist(attributes);
	IonizationResist(attributes);
	BurnResist(attributes);
	LeakageResist(attributes);
	DisruptionResist(attributes);
	SlownessResist(attributes);

	Thrust(attributes);
	ReverseThrust(attributes);
	Turn(attributes);
	Afterburner(attributes);

	Cloak(attributes);
}



// Clear all levels of input and set hull to -1.
void EnergyHandler::Kill(EnergyLevels &input) const
{
	input.hull = -1.;
	input.shields = 0.;
	input.energy = 0.;
	input.heat = 0.;
	input.fuel = 0.;
	ClearDoT(input);
}



// Clear the damage over time levels of the input.
void EnergyHandler::ClearDoT(EnergyLevels &input) const
{
	input.discharge = 0.;
	input.corrosion = 0.;
	input.ionization = 0.;
	input.leakage = 0.;
	input.burn = 0.;
	input.disruption = 0.;
	input.slowness = 0.;
}



// Repair the given stat up to the maximum given the energy input and cost.
// Updates the available variable with the remaining amount of repairs that
// can be done.
void EnergyHandler::DoRepair(double &stat, double &available, double maximum, EnergyLevels &input, const EnergyLevels &cost) const
{
	if(available <= 0. || stat >= maximum)
		return;

	if(cost.energy > 0.)
		available = min(available, input.energy / cost.energy);
	if(cost.heat < 0.)
		available = min(available, input.heat / -cost.heat);
	if(cost.fuel > 0.)
		available = min(available, input.fuel / cost.fuel);

	double transfer = min(available, maximum - stat);
	if(transfer > 0.)
	{
		stat += transfer;
		available -= transfer;
		input.energy -= transfer * cost.energy;
		input.heat += transfer * cost.heat;
		input.fuel -= transfer * cost.fuel;
	}
}



// Apply status effects and DoT resistances to the input.
void EnergyHandler::DoStatusEffects(EnergyLevels &input, bool disabled) const
{
	input.hull -= input.corrosion;
	input.shields -= input.discharge;
	input.energy -= input.ionization;
	input.heat += input.burn;
	input.fuel -= input.leakage;

	auto DoResistance = [&input, &disabled](double &stat, const EnergyLevels &cost)
	{
		if(!stat)
			return;

		if(disabled || cost.wildcard <= 0.)
		{
			stat = max(0., .99 * stat);
			return;
		}

		// Calculate how much resistance can be used assuming no
		// resource cost.
		double resistance = .99 * stat - max(0., .99 * stat - cost.wildcard);

		// Limit the resistance by the available resources.
		if(cost.energy > 0.)
			resistance = min(resistance, input.energy / cost.energy);
		if(cost.heat < 0.)
			resistance = min(resistance, input.heat / -cost.heat);
		if(cost.fuel > 0.)
			resistance = min(resistance, input.fuel / cost.fuel);

		if(resistance > 0.)
		{
			stat = max(0., .99 * stat - resistance);
			input.energy -= resistance * cost.energy;
			input.heat += resistance * cost.heat;
			input.fuel -= resistance * cost.fuel;
		}
		else
			stat = max(0., .99 * stat);
	};

	DoResistance(input.corrosion, corrosionResist);
	DoResistance(input.discharge, dischargeResist);
	DoResistance(input.ionization, ionizationResist);
	DoResistance(input.burn, burnResist);
	DoResistance(input.leakage, leakageResist);
	DoResistance(input.disruption, disruptionResist);
	DoResistance(input.slowness, slownessResist);
}



// Return true if the given input has the energy to expend on the cost.
bool EnergyHandler::CanExpend(const EnergyLevels &input, const EnergyLevels &cost) const
{
	if(input.hull < cost.hull)
		return false;
	if(input.shields < cost.shields)
		return false;
	if(input.energy < cost.energy)
		return false;
	if(input.heat < -cost.heat)
		return false;
	if(input.fuel < cost.fuel)
		return false;
	if(input.corrosion < -cost.corrosion)
		return false;
	if(input.discharge < -cost.discharge)
		return false;
	if(input.ionization < -cost.ionization)
		return false;
	if(input.burn < -cost.burn)
		return false;
	if(input.leakage < -cost.leakage)
		return false;
	if(input.disruption < -cost.disruption)
		return false;
	if(input.slowness < -cost.slowness)
		return false;
	return true;
}



// Return the amount of value that the given input can output
// given the maximum possible output and its cost.
double EnergyHandler::FractionalUsage(EnergyLevels &input, const EnergyLevels &cost, double output) const
{
	double scale = 1.;
	auto ScaleOutput = [&scale](double input, double cost)
	{
		if(input < cost * scale)
			scale = input / cost;
	};
	ScaleOutput(input.hull, cost.hull);
	ScaleOutput(input.shields, cost.shields);
	ScaleOutput(input.energy, cost.energy);
	ScaleOutput(input.heat, -cost.heat);
	ScaleOutput(input.fuel, cost.fuel);
	ScaleOutput(input.corrosion, -cost.corrosion);
	ScaleOutput(input.discharge, -cost.discharge);
	ScaleOutput(input.ionization, -cost.ionization);
	ScaleOutput(input.burn, -cost.burn);
	ScaleOutput(input.leakage, -cost.leakage);
	ScaleOutput(input.disruption, -cost.disruption);
	ScaleOutput(input.slowness, -cost.slowness);

	return scale * output;
}



// Apply damage * scale to the input. Hull, shields, energy, and fuel
// are subtracted from input while all other levels are added to input.
void EnergyHandler::Damage(EnergyLevels &input, const EnergyLevels &damage, double scale) const
{
	input.hull -= scale * damage.hull;
	input.shields -= scale * damage.shields;
	input.energy -= scale * damage.energy;
	input.heat += scale * damage.heat;
	input.fuel -= scale * damage.fuel;

	input.corrosion += scale * damage.corrosion;
	input.discharge += scale * damage.discharge;
	input.ionization += scale * damage.ionization;
	input.burn += scale * damage.burn;
	input.leakage += scale * damage.leakage;
	input.disruption += scale * damage.disruption;
	input.slowness += scale * damage.slowness;
}



// Update the stored EnergyLevels for each action a ship can take.
void EnergyHandler::HullRepair(const Outfit &attributes)
{
	hullRepairLevels.wildcard = attributes.Get("hull repair rate") * (1. + attributes.Get("hull repair multiplier"));
	// Save hull repair costs as per unit of hull repair.
	hullRepairLevels.energy = attributes.Get("hull energy") * (1. + attributes.Get("hull energy multiplier")) / hullRepairLevels.wildcard;
	hullRepairLevels.heat = attributes.Get("hull heat") * (1. + attributes.Get("hull heat multiplier")) / hullRepairLevels.wildcard;
	hullRepairLevels.fuel = attributes.Get("hull fuel") * (1. + attributes.Get("hull fuel multiplier")) / hullRepairLevels.wildcard;
}



void EnergyHandler::ShieldRegen(const Outfit &attributes)
{
	shieldRegenLevels.wildcard = attributes.Get("shield generation") * (1. + attributes.Get("shield generation multiplier"));
	// Save shield regen costs as per unit of shield regen.
	shieldRegenLevels.energy = attributes.Get("shield energy") * (1. + attributes.Get("shield energy multiplier")) / shieldRegenLevels.wildcard;
	shieldRegenLevels.heat = attributes.Get("shield heat") * (1. + attributes.Get("shield heat multiplier")) / shieldRegenLevels.wildcard;
	shieldRegenLevels.fuel = attributes.Get("shield fuel") * (1. + attributes.Get("shield fuel multiplier")) / shieldRegenLevels.wildcard;
}



void EnergyHandler::CorrosionResist(const Outfit &attributes)
{
	corrosionResist.wildcard = attributes.Get("corrosion resistance");
	// Save resistance costs as per unit of resistance.
	corrosionResist.energy = attributes.Get("corrosion resistance energy") / corrosionResist.wildcard;
	corrosionResist.heat = attributes.Get("corrosion resistance heat") / corrosionResist.wildcard;
	corrosionResist.fuel = attributes.Get("corrosion resistance fuel") / corrosionResist.wildcard;
}



void EnergyHandler::DischargeResist(const Outfit &attributes)
{
	dischargeResist.wildcard = attributes.Get("discharge resistance");
	// Save resistance costs as per unit of resistance.
	dischargeResist.energy = attributes.Get("discharge resistance energy") / dischargeResist.wildcard;
	dischargeResist.heat = attributes.Get("discharge resistance heat") / dischargeResist.wildcard;
	dischargeResist.fuel = attributes.Get("discharge resistance fuel") / dischargeResist.wildcard;
}



void EnergyHandler::IonizationResist(const Outfit &attributes)
{
	ionizationResist.wildcard = attributes.Get("ion resistance");
	// Save resistance costs as per unit of resistance.
	ionizationResist.energy = attributes.Get("ion resistance energy") / ionizationResist.wildcard;
	ionizationResist.heat = attributes.Get("ion resistance heat") / ionizationResist.wildcard;
	ionizationResist.fuel = attributes.Get("ion resistance fuel") / ionizationResist.wildcard;
}



void EnergyHandler::BurnResist(const Outfit &attributes)
{
	burnResist.wildcard = attributes.Get("burn resistance");
	// Save resistance costs as per unit of resistance.
	burnResist.energy = attributes.Get("burn resistance energy") / burnResist.wildcard;
	burnResist.heat = attributes.Get("burn resistance heat") / burnResist.wildcard;
	burnResist.fuel = attributes.Get("burn resistance fuel") / burnResist.wildcard;
}



void EnergyHandler::LeakageResist(const Outfit &attributes)
{
	leakageResist.wildcard = attributes.Get("leak resistance");
	// Save resistance costs as per unit of resistance.
	leakageResist.energy = attributes.Get("leak resistance energy") / leakageResist.wildcard;
	leakageResist.heat = attributes.Get("leak resistance heat") / leakageResist.wildcard;
	leakageResist.fuel = attributes.Get("leak resistance fuel") / leakageResist.wildcard;
}



void EnergyHandler::DisruptionResist(const Outfit &attributes)
{
	disruptionResist.wildcard = attributes.Get("disruption resistance");
	// Save resistance costs as per unit of resistance.
	disruptionResist.energy = attributes.Get("disruption resistance energy") / disruptionResist.wildcard;
	disruptionResist.heat = attributes.Get("disruption resistance heat") / disruptionResist.wildcard;
	disruptionResist.fuel = attributes.Get("disruption resistance fuel") / disruptionResist.wildcard;
}



void EnergyHandler::SlownessResist(const Outfit &attributes)
{
	slownessResist.wildcard = attributes.Get("slowing resistance");
	// Save resistance costs as per unit of resistance.
	slownessResist.energy = attributes.Get("slowing resistance energy") / slownessResist.wildcard;
	slownessResist.heat = attributes.Get("slowing resistance heat") / slownessResist.wildcard;
	slownessResist.fuel = attributes.Get("slowing resistance fuel") / slownessResist.wildcard;
}



void EnergyHandler::Thrust(const Outfit &attributes)
{
	thrustLevels.wildcard = attributes.Get("thrust");

	thrustLevels.hull = attributes.Get("thrusting hull");
	thrustLevels.shields = attributes.Get("thrusting shields");
	thrustLevels.energy = attributes.Get("thrusting energy");
	thrustLevels.heat = attributes.Get("thrusting heat");
	thrustLevels.fuel = attributes.Get("thrusting fuel");

	thrustLevels.corrosion = attributes.Get("thrusting corrosion");
	thrustLevels.discharge = attributes.Get("thrusting discharge");
	thrustLevels.ionization = attributes.Get("thrusting ion");
	thrustLevels.burn = attributes.Get("thrusting burn");
	thrustLevels.leakage = attributes.Get("thrusting leakage");
	thrustLevels.disruption = attributes.Get("thrusting disruption");
	thrustLevels.slowness = attributes.Get("thrusting slowing");
}



void EnergyHandler::ReverseThrust(const Outfit &attributes)
{
	reverseThrustLevels.wildcard = attributes.Get("reverse thrust");

	reverseThrustLevels.hull = attributes.Get("reverse thrusting hull");
	reverseThrustLevels.shields = attributes.Get("reverse thrusting shields");
	reverseThrustLevels.energy = attributes.Get("reverse thrusting energy");
	reverseThrustLevels.heat = attributes.Get("reverse thrusting heat");
	reverseThrustLevels.fuel = attributes.Get("reverse thrusting fuel");

	reverseThrustLevels.corrosion = attributes.Get("reverse thrusting corrosion");
	reverseThrustLevels.discharge = attributes.Get("reverse thrusting discharge");
	reverseThrustLevels.ionization = attributes.Get("reverse thrusting ion");
	reverseThrustLevels.burn = attributes.Get("reverse thrusting burn");
	reverseThrustLevels.leakage = attributes.Get("reverse thrusting leakage");
	reverseThrustLevels.disruption = attributes.Get("reverse thrusting disruption");
	reverseThrustLevels.slowness = attributes.Get("reverse thrusting slowing");
}



void EnergyHandler::Turn(const Outfit &attributes)
{
	turnLevels.wildcard = attributes.Get("turn");

	turnLevels.hull = attributes.Get("turning hull");
	turnLevels.shields = attributes.Get("turning shields");
	turnLevels.energy = attributes.Get("turning energy");
	turnLevels.heat = attributes.Get("turning heat");
	turnLevels.fuel = attributes.Get("turning fuel");

	turnLevels.corrosion = attributes.Get("turning corrosion");
	turnLevels.discharge = attributes.Get("turning discharge");
	turnLevels.ionization = attributes.Get("turning ion");
	turnLevels.burn = attributes.Get("turning burn");
	turnLevels.leakage = attributes.Get("turning leakage");
	turnLevels.disruption = attributes.Get("turning disruption");
	turnLevels.slowness = attributes.Get("turning slowing");
}



void EnergyHandler::Afterburner(const Outfit &attributes)
{
	afterburnerLevels.wildcard = attributes.Get("afterburner thrust");

	afterburnerLevels.hull = attributes.Get("afterburner hull");
	afterburnerLevels.shields = attributes.Get("afterburner shields");
	afterburnerLevels.energy = attributes.Get("afterburner energy");
	afterburnerLevels.heat = attributes.Get("afterburner heat");
	afterburnerLevels.fuel = attributes.Get("afterburner fuel");

	afterburnerLevels.corrosion = attributes.Get("afterburner corrosion");
	afterburnerLevels.discharge = attributes.Get("afterburner discharge");
	afterburnerLevels.ionization = attributes.Get("afterburner ion");
	afterburnerLevels.burn = attributes.Get("afterburner burn");
	afterburnerLevels.leakage = attributes.Get("afterburner leakage");
	afterburnerLevels.disruption = attributes.Get("afterburner disruption");
	afterburnerLevels.slowness = attributes.Get("afterburner slowing");
}



void EnergyHandler::Cloak(const Outfit &attributes)
{
	cloakLevels.wildcard = attributes.Get("cloak");

	cloakLevels.energy = attributes.Get("cloaking energy");
	cloakLevels.heat = attributes.Get("cloaking heat");
	cloakLevels.fuel = attributes.Get("cloaking fuel");
}
