/* AmmoDisplay.cpp
Copyright (c) 2022 by warp-core, 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "AbilityDisplay.h"

#include "Ability.h"
#include "AbilityTracker.h"
#include "../Color.h"
#include "../text/Font.h"
#include "../text/FontSet.h"
#include "../text/Format.h"
#include "../GameData.h"
#include "../PlayerInfo.h"
#include "../Point.h"
#include "../Rectangle.h"
#include "../Ship.h"
#include "../image/Sprite.h"
#include "../image/SpriteSet.h"
#include "../shader/SpriteShader.h"
#include "../Weapon.h"

using namespace std;



AbilityDisplay::AbilityDisplay(PlayerInfo &player)
	: player(player)
{
}



void AbilityDisplay::Reset()
{
	abilities.clear();
}



void AbilityDisplay::Update(const Ship &flagship)
{
	Reset();
	for(const AbilityTracker &it : flagship.AbilityTrackers())
		abilities[&it] = it.StateString(flagship);
}



void AbilityDisplay::Draw(const Rectangle &abilityBox, const Point &iconDimensions) const
{
	const Font &font = FontSet::Get(14);
	iconZones.clear();

	const double &iconWidth = iconDimensions.X();
	const double &iconHeight = iconDimensions.Y();
	// Pad the list by the same amount on all four sides.
	double padding = .5 * (abilityBox.Width() - iconWidth);
	const Sprite *selectedSprite = SpriteSet::Get("ui/ammo selected");
	const Sprite *unselectedSprite = SpriteSet::Get("ui/ammo unselected");
	const Color &selectedColor = *GameData::Colors().Get("bright");
	const Color &unselectedColor = *GameData::Colors().Get("dim");

	// This is the bottom left corner of the ammo display.
	auto pos = Point(abilityBox.Left() + padding, abilityBox.Bottom() - padding);
	// These offsets are relative to that corner.
	auto boxOff = Point(iconWidth - .5 * selectedSprite->Width(), .5 * iconHeight);
	auto textOff = Point(5. + iconWidth - .5 * iconHeight, .5 * (iconHeight - font.Height()));
	auto iconOff = Point(.5 * iconHeight, .5 * iconHeight);
	const double iconCenterX = (abilityBox.Right() + abilityBox.Left()) / 2.;
	const set<const AbilityTracker *> &selected = player.SelectedAbilities();
	for(const auto &[ability, state] : abilities)
	{
		pos.Y() -= iconHeight;
		if(pos.Y() < abilityBox.Top() + padding)
			break;

		bool isSelected = selected.contains(ability);

		SpriteShader::Draw(ability->Icon(), pos + iconOff);
		SpriteShader::Draw(isSelected ? selectedSprite : unselectedSprite, pos + boxOff);

		auto iconCenter = Point(iconCenterX, pos.Y() + iconHeight / 2.);
		iconZones.emplace_back(iconCenter, iconDimensions, ability);

		Point textPos = pos + textOff + Point(-font.Width(state), 0.);
		font.Draw(state, textPos, isSelected ? selectedColor : unselectedColor);
	}
}



bool AbilityDisplay::Click(const Point &clickPoint, bool control) const
{
	for(const auto &it : iconZones)
		if(it.Contains(clickPoint))
		{
			if(!control)
				player.DeselectAllAbilities();
			player.ToggleAnyAbility(it.Value());
			return true;
		}
	return false;
}



bool AbilityDisplay::Click(const Rectangle &clickBox) const
{
	bool reselected = false;
	for(const auto &it : iconZones)
		if(it.Overlaps(clickBox))
		{
			if(!reselected)
			{
				reselected = true;
				player.DeselectAllAbilities();
			}
			player.ToggleAnyAbility(it.Value());
		}
	return reselected;
}
