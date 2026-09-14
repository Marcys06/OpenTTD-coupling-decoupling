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

enum class MoveRailVehicleFlags : uint8_t {
	None                  = 0,         ///< No flag set.
	MoveChain             = (1U << 0), ///< Move all vehicles following the source vehicle
	Virtual               = (1U << 1), ///< This is a virtual vehicle (for creating TemplateVehicles)
	NewHead               = (1U << 2), ///< When moving a head vehicle, always reset the head state
};
DECLARE_ENUM_AS_BIT_SET(MoveRailVehicleFlags)

DEF_CMD_TUPLE_LT (Commands::MoveRailVehicle,          CmdMoveRailVehicle,           {}, CommandType::VehicleConstruction, CmdDataT<VehicleID, VehicleID, MoveRailVehicleFlags>)
DEF_CMD_TUPLE_LT (Commands::ForceTrainProceed,        CmdForceTrainProceed,         {}, CommandType::VehicleManagement,   CmdDataT<VehicleID>)
DEF_CMD_TUPLE_LT (Commands::ReverseTrainDirection,    CmdReverseTrainDirection,     {}, CommandType::VehicleManagement,   CmdDataT<VehicleID, bool>)
DEF_CMD_TUPLE_LT (Commands::SetTrainSpeedRestriction, CmdSetTrainSpeedRestriction, {}, CommandType::VehicleManagement,   CmdDataT<VehicleID, uint16_t>)
DEF_CMD_TUPLE_LT (Commands::DecoupleTrain,            CmdDecoupleTrain,             {}, CommandType::VehicleManagement,   CmdDataT<VehicleID>)

/**
 * Detach a wagon unit and all following units from a train consist.
 *
 * The detached chain keeps the original Train objects and vehicle IDs. No vehicle
 * is cloned or recreated. The chain becomes a free wagon chain and can subsequently
 * be attached to another consist with AttachTrainWagonChain().
 *
 * Only logical wagon-unit boundaries are accepted. An articulated vehicle therefore
 * cannot be split between its individual articulated parts.
 *
 * @param part First vehicle of the wagon unit to detach.
 * @return true when the chain was detached, false when the request is invalid.
 */
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

	front->ConsistChanged(CCF_ARRANGE);
	part->ConsistChanged(CCF_ARRANGE);
	return true;
}

/**
 * Attach a detached wagon chain to the rear of a train consist.
 *
 * The chain must already be detached (Previous() == nullptr) and must not start
 * with an engine. The physical Train objects and their vehicle IDs are preserved.
 *
 * @param dst Front engine of the destination train.
 * @param chain First vehicle of a free wagon chain.
 * @return true when the chain was attached, false when the request is invalid.
 */
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

	last_part->SetNext(chain);
	dst->ConsistChanged(CCF_ARRANGE);
	return true;
}

/**
 * Return whether a train can be decoupled at its current station stop.
 * Coupling/decoupling is a route operation, not a depot operation.
 */
inline bool CanDecoupleTrainAtStation(Train *train)
{
	if (train == nullptr || !train->IsPrimaryVehicle()) return false;
	if (train->IsStoppedInDepot()) return false;
	if (!train->vehstatus.Test(VehState::Stopped) || train->cur_speed != 0) return false;
	if (!train->current_order.IsAnyLoadingType()) return false;

	Train *last_unit = train->GetLastUnit();
	return last_unit != nullptr && last_unit->IsWagon();
}

/** Prototype command for the first in-game coupling test. */
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

#endif /* TRAIN_CMD_H */
