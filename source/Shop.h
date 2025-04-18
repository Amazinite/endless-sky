/* Shop.h
Copyright (c) 2025 by Amazinite

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
#include "DataNode.h"
#include "LocationFilter.h"
#include "Sale.h"
#include "Set.h"

#include <string>

class ConditionsStore;
class Planet;



// Class representing a shop of items. Shops are able to be added to planets to destingate
// that this shop should always stock that planet, or they can be given a condition set and/or
// location filter that allows them to optionally appear on a planet.
template <class Item>
class Shop {
public:
	Shop();
	Shop(const DataNode &node, const Set<Item> &items, const ConditionsStore *playerConditions);

	void Load(const DataNode &node, const Set<Item> &items, const ConditionsStore *playerConditions);

	// This shop's name.
	const std::string &Name() const;

	// All the items that this shop has in stock.
	const Sale<Item> &Stock() const;

	// Whether this shop is able to stock the given planet.
	bool CanStock(const Planet *planet) const;


private:
	std::string name;
	Sale<Item> stock;

	ConditionSet toSell;
	LocationFilter location;
};



template <class Item>
Shop<Item>::Shop()
{
}



template <class Item>
Shop<Item>::Shop(const DataNode &node, const Set<Item> &items, const ConditionsStore *playerConditions)
{
	Load(node, items, playerConditions);
}



template <class Item>
void Shop<Item>::Load(const DataNode &node, const Set<Item> &items, const ConditionsStore *playerConditions)
{
	name = node.Token(1);

	for(const DataNode &child : node)
	{
		const bool add = (child.Token(0) == "add");
		const bool remove = (child.Token(0) == "remove");

		const std::string &key = child.Token((add || remove) ? 1 : 0);
		const int valueIndex = (add || remove) ? 2 : 1;
		const bool hasValue = child.Size() > valueIndex;

		if(key == "to" && hasValue && child.Token(valueIndex) == "sell")
		{
			if(remove)
				toSell = ConditionSet{};
			else
				toSell.Load(child, playerConditions);
		}
		else if(key == "location")
		{
			if(remove)
				location = LocationFilter{};
			else
				location.Load(child);
		}
		else if(key == "stock")
		{
			if(remove)
				stock.clear();
			else
				stock.Load(child, items);
		}
		else
			stock.LoadSingle(child, items);
	}
}



template <class Item>
const std::string &Shop<Item>::Name() const
{
	return name;
}



template <class Item>
const Sale<Item> &Shop<Item>::Stock() const
{
	return stock;
}



template <class Item>
bool Shop<Item>::CanStock(const Planet *planet) const
{
	// If this shop doesn't have a defined condition set or location filter,
	// then it's only being used to explicitly define for each planet.
	if(toSell.IsEmpty() && location.IsEmpty())
		return false;

	// A shop is allowed to only define conditions, or a location filter, or both.
	// If both are specified, both must be true.
	return toSell.Test() && (location.IsEmpty() ? true : location.Matches(planet));
}
