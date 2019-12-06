/* Variant.h
Copyright (c) 2019 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef VARIANT_H_
#define VARIANT_H_

#include "GameData.h"

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

class DataNode;
class Ship;


// A variant represents a collection of ships that may be spawned by a fleet.
// Each variant contains one or more ships or variants.
class Variant {
public:
	Variant() = default;
	// Construct and Load() at the same time.
	Variant(const DataNode &node);
	
	void Load(const DataNode &node, bool global = false);
	
	int Weight() const;
	std::vector<const Ship *> Ships() const;
	
private:
	
private:
	int weight;
	std::vector<const Ship *> ships;
	std::vector<const Variant *> variant;

};



#endif
