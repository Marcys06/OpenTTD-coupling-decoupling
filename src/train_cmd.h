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

enum class MoveRailVehicleFlags : uint8_t {
	None                  = 0,
	MoveChain             = (1U << 0),
	Virtual               = (1U << 1),
	NewHead               = (1U << 2),
};
DECLARE_ENUM_AS_BIT_SET(MoveRailVehicleFlags)

DEF_CMD_TUPLE_LT (Commands::MoveRailVehicle,          CmdMoveRailVehicle,           {}, CommandType::VehicleConstruction, CmdDataT<VehicleID, VehicleID, MoveRailVehicleFlags>)
DEF_CMD_TUPLE_LT (Commands::ForceTrainProceed,        CmdForceTrainProceed,         {}, CommandType::VehicleManagement,   CmdDataT<VehicleID>)
DEF_CMD_TUPLE_LT (Commands::ReverseTrainDirection,    CmdReverseTrainDirection,     {}, CommandType::VehicleManagement,   CmdDataT<VehicleID, bool>)
DEF_CMD_TUPLE_LT (Commands::SetTrainSpeedRestriction, CmdSetTrainSpeedRestriction, {}, CommandType::VehicleManagement,   CmdDataT<VehicleID, uint16_t>)
DEF_CMD_TUPLE_LT (Commands::DecoupleTrain,            CmdDecoupleTrain,             {}, CommandType::VehicleManagement,   CmdDataT<VehicleID>)
DEF_CMD_TUPLE_LT (Commands::CoupleTrain,              CmdCoupleTrain,               {}, CommandType::VehicleManagement,   CmdDataT<VehicleID, VehicleID>)

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

inline bool CanDecoupleTrainAtStation(const Train *train)
{
	if (train == nullptr || !train->IsPrimaryVehicle()) return false;
	if (train->IsStoppedInDepot()) return false;
	if (!train->vehstatus.Test(VehState::Stopped) || train->cur_speed != 0) return false;
	if (!train->current_order.IsAnyLoadingType()) return false;

	const Train *last_unit = train;
	while (last_unit->GetNextUnit() != nullptr) last_unit = last_unit->GetNextUnit();
	return last_unit->IsWagon();
}

inline bool CanCoupleTrainAtStation(const Train *train, const Train *chain)
{
	if (train == nullptr || chain == nullptr) return false;
	if (!train->IsPrimaryVehicle() || train->IsStoppedInDepot()) return false;
	if (!train->vehstatus.Test(VehState::Stopped) || train->cur_speed != 0) return false;
	if (!train->current_order.IsAnyLoadingType()) return false;
	if (train->owner != chain->owner || !chain->IsFreeWagon() || chain->Previous() != nullptr) return false;
	if (chain->tile != train->tile) return false;
	return true;
}

inline CommandCost CmdDecoupleTrain(DoCommandFlags flags, VehicleID veh_id)
{
	Train *train = Train::GetIfValid(veh_id);
	if (train == nullptr || !train->IsPrimaryVehicle()) return CMD_ERROR;
	if (train->owner != _current_company) return CMD_ERROR;
	if (!CanDecoupleTrainAtStation(train)) return CMD_ERROR;

	Train *last_unit = train->GetLastUnit();
	if (flags.Test(DoCommandFlag::Execute) && !DetachTrainWagonChain(last_unit)) return CMD_ERROR;
	return CommandCost();
}

inline CommandCost CmdCoupleTrain(DoCommandFlags flags, VehicleID train_id, VehicleID wagon_id)
{
	Train *train = Train::GetIfValid(train_id);
	Train *chain = Train::GetIfValid(wagon_id);
	if (!CanCoupleTrainAtStation(train, chain)) return CMD_ERROR;
	if (train->owner != _current_company) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute) && !AttachTrainWagonChain(train, chain)) return CMD_ERROR;
	return CommandCost();
}

#endif /* TRAIN_CMD_H */