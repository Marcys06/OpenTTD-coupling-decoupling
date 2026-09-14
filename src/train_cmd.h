/*
 * This file is part of OpenTTD.
 * OpenTTD is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train_cmd.h Command definitions related to trains. */

#ifndef TRAIN_CMD_H
#define TRAIN_CMD_H

#include "company_func.h"
#include "command_type.h"
#include "vehicle_type.h"
#include "train.h"
#include "vehicle_func.h"
#include "debug.h"

enum class MoveRailVehicleFlags : uint8_t {
	None                  = 0,
	MoveChain             = (1U << 0),
	Virtual               = (1U << 1),
	NewHead               = (1U << 2),
};
DECLARE_ENUM_AS_BIT_SET(MoveRailVehicleFlags)

CommandCost CmdForceTrainProceed(DoCommandFlags flags, VehicleID veh_id);
CommandCost CmdStationCouplingOrForceProceed(DoCommandFlags flags, VehicleID veh_id);

DEF_CMD_TUPLE_LT (Commands::MoveRailVehicle,          CmdMoveRailVehicle,           {}, CommandType::VehicleConstruction, CmdDataT<VehicleID, VehicleID, MoveRailVehicleFlags>)
DEF_CMD_TUPLE_LT (Commands::ForceTrainProceed,        CmdStationCouplingOrForceProceed, {}, CommandType::VehicleManagement, CmdDataT<VehicleID>)
DEF_CMD_TUPLE_LT (Commands::DecoupleTrain,            CmdDecoupleTrain,              {}, CommandType::VehicleManagement, CmdDataT<VehicleID>)
DEF_CMD_TUPLE_LT (Commands::ReverseTrainDirection,    CmdReverseTrainDirection,     {}, CommandType::VehicleManagement, CmdDataT<VehicleID, bool>)
DEF_CMD_TUPLE_LT (Commands::SetTrainSpeedRestriction, CmdSetTrainSpeedRestriction, {}, CommandType::VehicleManagement,   CmdDataT<VehicleID, uint16_t>)

inline bool DetachTrainWagonChain(Train *part)
{
	if (part == nullptr || part->Previous() == nullptr) return false;
	if (part->IsEngine() || part->IsArticulatedPart()) return false;

	Train *front = part->First();
	if (front == nullptr || !front->IsFrontEngine()) return false;

	Train *tail = part->GetLastEnginePart();
	if (tail == nullptr) return false;

	Train *previous = part->Previous();
	Train *next = tail->Next();
	previous->SetNext(next);
	tail->SetNext(nullptr);
	part->SetFreeWagon();

	front->ConsistChanged(CCF_ARRANGE);
	part->ConsistChanged(CCF_ARRANGE);
	return true;
}

inline bool AttachTrainWagonChain(Train *dst, Train *chain)
{
	if (dst == nullptr || chain == nullptr) return false;
	if (!dst->IsFrontEngine() || dst->Previous() != nullptr) return false;
	if (!chain->IsFreeWagon() || chain->Previous() != nullptr) return false;
	if (chain->IsEngine() || chain->IsArticulatedPart()) return false;

	Train *last_unit = dst->GetLastUnit();
	if (last_unit == nullptr) return false;
	Train *last_part = last_unit->GetLastEnginePart();
	if (last_part == nullptr) return false;

	chain->ClearFreeWagon();
	last_part->SetNext(chain);
	dst->ConsistChanged(CCF_ARRANGE);
	return true;
}

inline const Train *FindCouplableWagonAtStation(const Train *train)
{
	if (train == nullptr) return nullptr;
	for (Train *wagon : VehiclesOnTile<VehicleType::Train>(train->tile)) {
		if (wagon->owner != train->owner) continue;
		if (wagon->vehstatus.Test(VehState::Crashed)) continue;
		if (!wagon->IsFreeWagon() || wagon->First() != wagon) continue;
		if (wagon->tile != train->tile) continue;
		return wagon;
	}
	return nullptr;
}

inline bool CanDecoupleTrainAtStation(const Train *train)
{
	if (train == nullptr) {
		Debug(misc, 0, "Coupling debug: invalid train");
		return false;
	}
	if (!train->IsPrimaryVehicle()) {
		Debug(misc, 0, "Coupling debug: vehicle {} is not primary", train->index);
		return false;
	}
	if (train->IsStoppedInDepot()) {
		Debug(misc, 0, "Coupling debug: train {} is in depot", train->index);
		return false;
	}
	/* During station loading OpenTTD may clear VehState::Stopped while keeping the train at zero speed.
	 * Coupling/decoupling is a zero-speed consist operation, so use speed as the authoritative motion check. */
	if (train->cur_speed != 0) {
		Debug(misc, 0, "Coupling debug: train {} is moving, cur_speed={}, vehstatus={}", train->index, train->cur_speed, train->vehstatus.base());
		return false;
	}

	const Train *last_unit = train;
	while (last_unit->GetNextUnit() != nullptr) last_unit = last_unit->GetNextUnit();
	if (last_unit->IsWagon()) {
		Debug(misc, 0, "Coupling debug: train {} can decouple, last unit {} is wagon", train->index, last_unit->index);
		return true;
	}

	const Train *found = FindCouplableWagonAtStation(train);
	if (found != nullptr) {
		Debug(misc, 0, "Coupling debug: train {} can couple free wagon {} at tile {}", train->index, found->index, train->tile);
		return true;
	}

	Debug(misc, 0, "Coupling debug: train {} has no detachable end wagon and no free wagon on tile {}", train->index, train->tile);
	return false;
}

inline bool CanCoupleTrainAtStation(const Train *train, const Train *chain)
{
	if (train == nullptr || chain == nullptr) {
		Debug(misc, 0, "Coupling debug: null train/chain");
		return false;
	}
	if (!train->IsPrimaryVehicle() || train->IsStoppedInDepot()) {
		Debug(misc, 0, "Coupling debug: destination train {} invalid primary/depot state", train->index);
		return false;
	}
	if (train->cur_speed != 0) {
		Debug(misc, 0, "Coupling debug: destination train {} moving, speed={}, vehstatus={}", train->index, train->cur_speed, train->vehstatus.base());
		return false;
	}
	if (train->owner != chain->owner) {
		Debug(misc, 0, "Coupling debug: owner mismatch train={} wagon={}", train->owner, chain->owner);
		return false;
	}
	if (!chain->IsFreeWagon()) {
		Debug(misc, 0, "Coupling debug: chain {} is not marked free wagon", chain->index);
		return false;
	}
	if (chain->Previous() != nullptr) {
		Debug(misc, 0, "Coupling debug: free wagon {} still has previous vehicle", chain->index);
		return false;
	}
	if (chain->tile != train->tile) {
		Debug(misc, 0, "Coupling debug: tile mismatch train={} wagon={}", train->tile, chain->tile);
		return false;
	}
	return true;
}

inline CommandCost CmdDecoupleTrain(DoCommandFlags flags, VehicleID veh_id)
{
	Train *train = Train::GetIfValid(veh_id);
	Debug(misc, 0, "Coupling debug: command veh={} flags={} execute={}", veh_id, flags.base(), flags.Test(DoCommandFlag::Execute));
	if (train == nullptr) {
		Debug(misc, 0, "Coupling debug: vehicle {} not found", veh_id);
		return CMD_ERROR;
	}
	if (!train->IsPrimaryVehicle()) {
		Debug(misc, 0, "Coupling debug: vehicle {} is not primary", veh_id);
		return CMD_ERROR;
	}
	if (train->owner != _current_company) {
		Debug(misc, 0, "Coupling debug: owner mismatch vehicle={} current_company={}", train->owner, _current_company);
		return CMD_ERROR;
	}
	if (!CanDecoupleTrainAtStation(train)) return CMD_ERROR;

	Train *last_unit = train->GetLastUnit();
	if (last_unit->IsWagon()) {
		Debug(misc, 0, "Coupling debug: detaching wagon chain starting at {}", last_unit->index);
		if (flags.Test(DoCommandFlag::Execute) && !DetachTrainWagonChain(last_unit)) {
			Debug(misc, 0, "Coupling debug: DetachTrainWagonChain failed for {}", last_unit->index);
			return CMD_ERROR;
		}
		return CommandCost();
	}

	const Train *found = FindCouplableWagonAtStation(train);
	if (found == nullptr) {
		Debug(misc, 0, "Coupling debug: no wagon found for coupling");
		return CMD_ERROR;
	}
	Train *chain = Train::GetIfValid(found->index);
	if (flags.Test(DoCommandFlag::Execute) && !AttachTrainWagonChain(train, chain)) {
		Debug(misc, 0, "Coupling debug: AttachTrainWagonChain failed for train={} wagon={}", train->index, chain->index);
		return CMD_ERROR;
	}
	Debug(misc, 0, "Coupling debug: coupling command accepted train={} wagon={}", train->index, chain->index);
	return CommandCost();
}

inline CommandCost CmdStationCouplingOrForceProceed(DoCommandFlags flags, VehicleID veh_id)
{
	Train *train = Train::GetIfValid(veh_id);
	if (train != nullptr && CanDecoupleTrainAtStation(train)) {
		Debug(misc, 0, "Coupling debug: ForceProceed redirected to coupling command for {}", veh_id);
		return CmdDecoupleTrain(flags, veh_id);
	}
	Debug(misc, 0, "Coupling debug: ForceProceed kept as normal command for {}", veh_id);
	return CmdForceTrainProceed(flags, veh_id);
}

#endif /* TRAIN_CMD_H */