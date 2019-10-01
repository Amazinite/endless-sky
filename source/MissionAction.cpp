/* MissionAction.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "MissionAction.h"

#include "CargoHold.h"
#include "ConversationPanel.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "Format.h"
#include "GameData.h"
#include "GameEvent.h"
#include "Messages.h"
#include "Outfit.h"
#include "PlayerInfo.h"
#include "Random.h"
#include "Ship.h"
#include "UI.h"

#include <cstdlib>
#include <vector>

using namespace std;

namespace {
	void DoGift(PlayerInfo &player, const Outfit *outfit, int count, UI *ui)
	{
		Ship *flagship = player.Flagship();
		bool isSingle = (abs(count) == 1);
		string nameWas = (isSingle ? outfit->Name() : outfit->PluralName());
		if(!flagship || !count || nameWas.empty())
			return;
		
		nameWas += (isSingle ? " was" : " were");
		string message;
		if(isSingle)
		{
			char c = tolower(nameWas.front());
			bool isVowel = (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u');
			message = (isVowel ? "An " : "A ");
		}
		else
			message = to_string(abs(count)) + " ";
		
		message += nameWas;
		if(count > 0)
			message += " added to your ";
		else
			message += " removed from your ";
		
		bool didCargo = false;
		bool didShip = false;
		// If not landed, transfers must be done into the flagship's CargoHold.
		CargoHold &cargo = (player.GetPlanet() ? player.Cargo() : flagship->Cargo());
		int cargoCount = cargo.Get(outfit);
		if(count < 0 && cargoCount)
		{
			int moved = min(cargoCount, -count);
			count += moved;
			cargo.Remove(outfit, moved);
			didCargo = true;
		}
		while(count)
		{
			int moved = (count > 0) ? 1 : -1;
			if(flagship->Attributes().CanAdd(*outfit, moved))
			{
				flagship->AddOutfit(outfit, moved);
				didShip = true;
			}
			else
				break;
			count -= moved;
		}
		if(count > 0)
		{
			// Ignore cargo size limits.
			int size = cargo.Size();
			cargo.SetSize(-1);
			cargo.Add(outfit, count);
			cargo.SetSize(size);
			didCargo = true;
			if(ui)
			{
				string special = "The " + nameWas;
				special += " put in your cargo hold because there is not enough space to install ";
				special += (isSingle ? "it" : "them");
				special += " in your ship.";
				ui->Push(new Dialog(special));
			}
		}
		if(didCargo && didShip)
			message += "cargo hold and your flagship.";
		else if(didCargo)
			message += "cargo hold.";
		else
			message += "flagship.";
		Messages::Add(message);
	}
	
	int CountInCargo(const Outfit *outfit, const PlayerInfo &player)
	{
		int available = 0;
		// If landed, all cargo from available ships is pooled together.
		if(player.GetPlanet())
			available += player.Cargo().Get(outfit);
		// Otherwise only count outfits in the cargo holds of in-system ships.
		else
		{
			const System *here = player.GetSystem();
			for(const auto &ship : player.Ships())
			{
				if(ship->IsDisabled() || ship->IsParked())
					continue;
				if(ship->GetSystem() == here || (ship->CanBeCarried()
						&& !ship->GetSystem() && ship->GetParent()->GetSystem() == here))
					available += ship->Cargo().Get(outfit);
			}
		}
		return available;
	}
	
	string GetLocations(MissionAction::CheckLocations locations)
	{
		string locs;
		
		if(locations.flag == true)
			locs.append("flagship ");
		if(locations.escorts == true)
			locs.append("escorts ");
		if(locations.installed == true)
			locs.append("installed ");
		if(locations.cargo == true)
			locs.append("cargo ");
		if(locations.present == true)
			locs.append("present ");
		if(locations.absent == true)
			locs.append("absent ");
		if(locations.parked == true)
			locs.append("parked ");
		if(locations.unparked == true)
			locs.append("unparked ");
		
		// Pop back the extraneous space.
		if(!locs.empty())
			locs.pop_back();
		return locs;
	}
}



// Construct and Load() at the same time.
MissionAction::MissionAction(const DataNode &node, const string &missionName)
{
	Load(node, missionName);
}



void MissionAction::Load(const DataNode &node, const string &missionName)
{
	if(node.Size() >= 2)
		trigger = node.Token(1);
	if(node.Size() >= 3)
		system = node.Token(2);
	
	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		bool hasValue = (child.Size() >= 2);
		
		if(key == "log")
		{
			bool isSpecial = (child.Size() >= 3);
			string &text = (isSpecial ?
				specialLogText[child.Token(1)][child.Token(2)] : logText);
			Dialog::ParseTextNode(child, isSpecial ? 3 : 1, text);
		}
		else if(key == "dialog")
		{
			// Dialog text may be supplied from a stock named phrase, a
			// private unnamed phrase, or directly specified.
			if(hasValue && child.Token(1) == "phrase")
			{
				if(!child.HasChildren() && child.Size() == 3)
					stockDialogPhrase = GameData::Phrases().Get(child.Token(2));
				else
					child.PrintTrace("Skipping unsupported dialog phrase syntax:");
			}
			else if(!hasValue && child.HasChildren() && (*child.begin()).Token(0) == "phrase")
			{
				const DataNode &firstGrand = (*child.begin());
				if(firstGrand.Size() == 1 && firstGrand.HasChildren())
					dialogPhrase.Load(firstGrand);
				else
					firstGrand.PrintTrace("Skipping unsupported dialog phrase syntax:");
			}
			else
				Dialog::ParseTextNode(child, 1, dialogText);
		}
		else if(key == "conversation" && child.HasChildren())
			conversation.Load(child);
		else if(key == "conversation" && hasValue)
			stockConversation = GameData::Conversations().Get(child.Token(1));
		else if(key == "outfit" && hasValue)
		{
			int count = (child.Size() < 3 ? 1 : static_cast<int>(child.Value(2)));
			if(count)
				gifts[GameData::Outfits().Get(child.Token(1))] = count;
			else
			{
				// outfit <outfit> 0 means the player must have this outfit.
				child.PrintTrace("Warning: deprecated use of \"outfit\" with count of 0. Use \"require <outfit>\" instead:");
				CheckLocations location;
				requiredOutfits[GameData::Outfits().Get(child.Token(1))] = make_pair(1,location);
			}
		}
		else if(key == "require" && hasValue)
		{
			int count = (child.Size() < 3 ? 1 : static_cast<int>(child.Value(2)));
			
			CheckLocations location;
			for(const DataNode &it : child)
			{
				location.empty = false;
				for(int i = 0; i < it.Size(); ++i)
				{
					if(it.Token(i) == "flagship")
						location.flag = true;
					else if(it.Token(i) == "escorts")
						location.escorts = true;
					else if(it.Token(i) == "installed")
						location.installed = true;
					else if(it.Token(i) == "cargo")
						location.cargo = true;
					else if(it.Token(i) == "present")
						location.present = true;
					else if(it.Token(i) == "absent")
						location.absent = true;
					else if(it.Token(i) == "parked")
						location.parked = true;
					else if(it.Token(i) == "unparked")
						location.unparked = true;
				}
			}
			
			if(count >= 0)
				requiredOutfits[GameData::Outfits().Get(child.Token(1))] = make_pair(count, location);
			else
				child.PrintTrace("Skipping invalid \"require\" amount:");
		}
		else if(key == "payment")
		{
			if(child.Size() == 1)
				paymentMultiplier += 150;
			if(child.Size() >= 2)
				payment += child.Value(1);
			if(child.Size() >= 3)
				paymentMultiplier += child.Value(2);
		}
		else if(key == "event" && hasValue)
		{
			int minDays = (child.Size() >= 3 ? child.Value(2) : 0);
			int maxDays = (child.Size() >= 4 ? child.Value(3) : minDays);
			if(maxDays < minDays)
				swap(minDays, maxDays);
			events[GameData::Events().Get(child.Token(1))] = make_pair(minDays, maxDays);
		}
		else if(key == "fail")
		{
			string toFail = child.Size() >= 2 ? child.Token(1) : missionName;
			fail.insert(toFail);
			// Create a GameData reference to this mission name.
			GameData::Missions().Get(toFail);
		}
		else if(key == "system")
		{
			if(system.empty() && child.HasChildren())
				systemFilter.Load(child);
			else
				child.PrintTrace("Unsupported use of \"system\" LocationFilter:");
		}
		else
			conditions.Add(child);
	}
}



// Note: the Save() function can assume this is an instantiated mission, not
// a template, so it only has to save a subset of the data.
void MissionAction::Save(DataWriter &out) const
{
	if(system.empty())
		out.Write("on", trigger);
	else
		out.Write("on", trigger, system);
	out.BeginChild();
	{
		if(!systemFilter.IsEmpty())
		{
			out.Write("system");
			// LocationFilter indentation is handled by its Save method.
			systemFilter.Save(out);
		}
		if(!logText.empty())
		{
			out.Write("log");
			out.BeginChild();
			{
				// Break the text up into paragraphs.
				for(const string &line : Format::Split(logText, "\n\t"))
					out.Write(line);
			}
			out.EndChild();
		}
		for(const auto &it : specialLogText)
			for(const auto &eit : it.second)
			{
				out.Write("log", it.first, eit.first);
				out.BeginChild();
				{
					// Break the text up into paragraphs.
					for(const string &line : Format::Split(eit.second, "\n\t"))
						out.Write(line);
				}
				out.EndChild();
			}
		if(!dialogText.empty())
		{
			out.Write("dialog");
			out.BeginChild();
			{
				// Break the text up into paragraphs.
				for(const string &line : Format::Split(dialogText, "\n\t"))
					out.Write(line);
			}
			out.EndChild();
		}
		if(!conversation.IsEmpty())
			conversation.Save(out);
		
		for(const auto &it : gifts)
			out.Write("outfit", it.first->Name(), it.second);
		for(const auto &it : requiredOutfits)
		{
			int count = it.second.first;
			CheckLocations locations = it.second.second;
			out.Write("require", it.first->Name(), count);
			if(!locations.empty)
			{
				out.BeginChild();
				out.Write(GetLocations(locations));
				out.EndChild();
			}
		}
		if(payment)
			out.Write("payment", payment);
		for(const auto &it : events)
		{
			if(it.second.first == it.second.second)
				out.Write("event", it.first->Name(), it.second.first);
			else
				out.Write("event", it.first->Name(), it.second.first, it.second.second);
		}
		for(const auto &name : fail)
			out.Write("fail", name);
		
		conditions.Save(out);
	}
	out.EndChild();
}



int MissionAction::Payment() const
{
	return payment;
}



// Check if this action can be completed right now. It cannot be completed
// if it takes away money or outfits that the player does not have.
bool MissionAction::CanBeDone(const PlayerInfo &player, const shared_ptr<Ship> &boardingShip) const
{
	if(player.Accounts().Credits() < -payment)
		return false;
	
	const Ship *flagship = player.Flagship();
	for(const auto &it : gifts)
	{
		// If this outfit is being given, the player doesn't need to have it.
		if(it.second > 0)
			continue;
		
		// Outfits may always be taken from the flagship. If landed, they may also be taken from
		// the collective cargohold of any in-system, non-disabled escorts (player.Cargo()). If
		// boarding, consider only the flagship's cargo hold. If in-flight, show mission status
		// by checking the cargo holds of ships that would contribute to player.Cargo if landed.
		int available = flagship ? flagship->OutfitCount(it.first) : 0;
		available += boardingShip ? flagship->Cargo().Get(it.first)
				: CountInCargo(it.first, player);
		
		if(available < -it.second)
			return false;
	}
	
	for(const auto &it : requiredOutfits)
	{
		int available = 0;
		int count = it.second.first;
		CheckLocations locations = it.second.second;
		
		bool checkFlag = locations.flag;
		bool checkEscorts = locations.escorts;
		bool checkInstalled = locations.installed;
		bool checkCargo = locations.cargo;
		bool checkPresent = locations.present;
		bool checkAbsent = locations.absent;
		bool checkParked = locations.parked;
		bool checkUnparked = locations.unparked;
		
		if(!locations.empty)
		{
			// The following if statements swap location checks to true if certain keywords have
			// been omitted, e.g, not specifying installed or cargo means check both.
			if(!checkFlag && !checkEscorts) {
				checkEscorts = true;
				if(!checkAbsent && !checkParked)
					checkFlag = true;
			}
			if(!checkCargo && !checkInstalled)
			{
				checkCargo = true;
				checkInstalled = true;
			}
			if(!checkPresent && !checkAbsent)
			{
				checkPresent = true;
				checkAbsent = true;
			}
			if(!checkParked && !checkUnparked)
			{
				checkParked = true;
				checkUnparked = true;
			}
		}
		else if(!count)
		{
			// Requiring the player to have 0 of this outfit and not specifying where to look
			// means all ships and all cargo holds must be checked, even if the ship is disabled, 
			// parked, or out-of-system.
			checkFlag = true;
			checkEscorts = true;
			checkInstalled = true;
			checkCargo = true;
			checkPresent = true;
			checkAbsent = true;
			checkParked = true;
			checkUnparked = true;
		}
		else
		{
			// If no check locations were specified and the required amount is greater than zero, 
			// check only the flagship.
			checkFlag = true;
			checkCargo = true;
			checkInstalled = true;
		}
		
		// Now that the check booleans have been handled, check the flagship
		// if necessary.
		if(checkFlag)
		{
			if(checkCargo)
				available += boardingShip ? flagship->Cargo().Get(it.first)
					: CountInCargo(it.first, player);
			if(checkInstalled)
				available += flagship ? flagship->OutfitCount(it.first) : 0;
			
			// If the required amount is 0 and an outfit has already been found,
			// then return false.
			if(!count && available > 0)
				return false;
		}
		
		
		// If the escorts need to be checked, then iterate through all the player's ships
		if(checkEscorts)
		{
			checkFlag = true;
			for(const auto &ship : player.Ships())
			{
				// Ignore destroyed ships.
				if(ship->IsDestroyed())
					continue;
				// The first ship in list is the flagship, so skip over it.
				if(checkFlag)
				{
					checkFlag = false;
					continue;
				}

				if(checkCargo)
				{
					if(checkPresent && ship->GetSystem() == player.GetSystem())
					{
						// If the player is landed and not parked then check the player's pooled cargo
						if(player.GetPlanet() && !ship->IsParked())
							available += player.Cargo().Get(it.first);
						else
							available += ship->Cargo().Get(it.first);
					}
					if(checkAbsent && ship->GetSystem() != player.GetSystem())
						available += ship->Cargo().Get(it.first);
				}
				
				if(checkInstalled)
					if((checkPresent && ship->GetSystem() == player.GetSystem()) ||
						(checkAbsent && ship->GetSystem() != player.GetSystem()))
						if((checkParked && ship->IsParked()) || 
							(checkUnparked && !ship->IsParked()))
							available += ship->OutfitCount(it.first);
				
				if(!count && available > 0)
					return false;
			}
		}
		
		if(available < count)
			return false;
	}
	
	// An `on enter` MissionAction may have defined a LocationFilter that
	// specifies the systems in which it can occur.
	if(!systemFilter.IsEmpty() && !systemFilter.Matches(player.GetSystem()))
		return false;
	return true;
}



void MissionAction::Do(PlayerInfo &player, UI *ui, const System *destination, const shared_ptr<Ship> &ship) const
{
	bool isOffer = (trigger == "offer");
	if(!conversation.IsEmpty() && ui)
	{
		// Conversations offered while boarding or assisting reference a ship,
		// which may be destroyed depending on the player's choices.
		ConversationPanel *panel = new ConversationPanel(player, conversation, destination, ship);
		if(isOffer)
			panel->SetCallback(&player, &PlayerInfo::MissionCallback);
		// Use a basic callback to handle forced departure outside of `on offer`
		// conversations.
		else
			panel->SetCallback(&player, &PlayerInfo::BasicCallback);
		ui->Push(panel);
	}
	else if(!dialogText.empty() && ui)
	{
		map<string, string> subs;
		subs["<first>"] = player.FirstName();
		subs["<last>"] = player.LastName();
		if(player.Flagship())
			subs["<ship>"] = player.Flagship()->Name();
		string text = Format::Replace(dialogText, subs);
		
		if(isOffer)
			ui->Push(new Dialog(text, player, destination));
		else
			ui->Push(new Dialog(text));
	}
	else if(isOffer && ui)
		player.MissionCallback(Conversation::ACCEPT);
	
	if(!logText.empty())
		player.AddLogEntry(logText);
	for(const auto &it : specialLogText)
		for(const auto &eit : it.second)
			player.AddSpecialLog(it.first, eit.first, eit.second);
	
	// If multiple outfits are being transferred, first remove them before
	// adding any new ones.
	for(const auto &it : gifts)
		if(it.second < 0)
			DoGift(player, it.first, it.second, ui);
	for(const auto &it : gifts)
		if(it.second > 0)
			DoGift(player, it.first, it.second, ui);
	
	if(payment)
		player.Accounts().AddCredits(payment);
	
	for(const auto &it : events)
		player.AddEvent(*it.first, player.GetDate() + it.second.first);
	
	if(!fail.empty())
	{
		// If this action causes this or any other mission to fail, mark that
		// mission as failed. It will not be removed from the player's mission
		// list until it is safe to do so.
		for(const Mission &mission : player.Missions())
			if(fail.count(mission.Identifier()))
				player.FailMission(mission);
	}
	
	// Check if applying the conditions changes the player's reputations.
	player.SetReputationConditions();
	conditions.Apply(player.Conditions());
	player.CheckReputationConditions();
}



MissionAction MissionAction::Instantiate(map<string, string> &subs, const System *origin, int jumps, int payload) const
{
	MissionAction result;
	result.trigger = trigger;
	result.system = system;
	// Convert any "distance" specifiers into "near <system>" specifiers.
	result.systemFilter = systemFilter.SetOrigin(origin);
	
	for(const auto &it : events)
	{
		// Allow randomization of event times. The second value in the pair is
		// always greater than or equal to the first, so Random::Int() will
		// never be called with a value less than 1.
		int day = it.second.first + Random::Int(it.second.second - it.second.first + 1);
		result.events[it.first] = make_pair(day, day);
	}
	result.gifts = gifts;
	result.requiredOutfits = requiredOutfits;
	result.payment = payment + (jumps + 1) * payload * paymentMultiplier;
	// Fill in the payment amount if this is the "complete" action.
	string previousPayment = subs["<payment>"];
	if(result.payment)
		subs["<payment>"] = Format::Credits(abs(result.payment))
			+ (result.payment == 1 ? " credit" : " credits");
	
	if(!logText.empty())
		result.logText = Format::Replace(logText, subs);
	for(const auto &it : specialLogText)
		for(const auto &eit : it.second)
			result.specialLogText[it.first][eit.first] = Format::Replace(eit.second, subs);
	
	// Create any associated dialog text from phrases, or use the directly specified text.
	string dialogText = stockDialogPhrase ? stockDialogPhrase->Get()
		: (!dialogPhrase.Name().empty() ? dialogPhrase.Get()
		: this->dialogText);
	if(!dialogText.empty())
		result.dialogText = Format::Replace(dialogText, subs);
	
	if(stockConversation)
		result.conversation = stockConversation->Substitute(subs);
	else if(!conversation.IsEmpty())
		result.conversation = conversation.Substitute(subs);
	
	result.fail = fail;
	
	result.conditions = conditions;
	
	// Restore the "<payment>" value from the "on complete" condition, for use
	// in other parts of this mission.
	if(result.payment && trigger != "complete")
		subs["<payment>"] = previousPayment;
	
	return result;
}
