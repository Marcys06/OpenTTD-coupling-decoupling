/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train_cmd.h Command definitions related to trains. */

#ifndef TRAIN_CMD_H
#define TRAIN_CMD_H

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

/**
 * Detach a wagon unit and all following units from a train consist.
 *
 * The returned chain keeps the original Train objects and vehicle IDs. No vehicle
 * is cloned or recreated. The detached chain becomes a free wagon chain and can
 * subsequently be attached to another consist with AttachTrainWagonChain().
 *
 * The operation is deliberately limited to logical wagon-unit boundaries. An
 * articulated vehicle may therefore only be detached from its first part.
 *
 * @param part First vehicle of the wagon unit to detach.
 * @return true when the chain was detached, false when the request is invalid.
 */
inline bool DetachTrainWagonChain(Train *part)
{
	if (part == nullptr || part->Previous() == nullptr) return false;
	if (part->IsEngine() || part->IsArticulatedPart()) return false;

	Train *front = part->GetFrontEngine();
	if (front == nullptr || !front->IsFrontEngine()) return false;

	/* Do not allow a split inside an articulated vehicle. */
	Train *tail = part->GetLastEnginePart();
	if (tail == nullptr) return false;

	Train *next = tail->Next();
	part->Previous()->SetNext(next);
	tail->SetNext(nullptr);

	front->ConsistChanged(CCF_ARRANGE);
	part->ConsistChanged(CCF_ARRANGE);
	return true;
}

/**
 * Attach a detached wagon chain to the rear of a train consist.
 *
 * Both arguments must describe existing physical Train objects. The chain must
 * already be detached (Previous() == nullptr) and must not contain an engine.
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

	Train *tail = chain;
	while (tail->Next() != nullptr) {
		tail = tail->Next();
	}

	Train *last_unit = dst->GetLastUnit();
	if (last_unit == nullptr) return false;

	/* GetLastUnit() points to the first part of the last logical unit. */
	Train *last_part = last_unit->GetLastEnginePart();
	if (last_part == nullptr) return false;

	last_part->SetNext(chain);
	dst->ConsistChanged(CCF_ARRANGE);
	return true;
}

#endif /* TRAIN_CMD_H */
