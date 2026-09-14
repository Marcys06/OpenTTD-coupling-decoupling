/*
 * This file is part of OpenTTD.
 * OpenTTD is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file train_coupling_order.h Helpers shared by train coupling order UI code. */

#ifndef TRAIN_COUPLING_ORDER_H
#define TRAIN_COUPLING_ORDER_H

#include "command_func.h"
#include "vehicle_type.h"
#include "vehicleid_type.h"

/** Sentinel used by the order GUI for a coupling/decoupling action. */
static constexpr uint16_t TRAIN_COUPLING_ORDER_ACTION = 0xFFFF;

/** Return whether this order action represents train coupling/decoupling. */
constexpr bool IsTrainCouplingOrderAction(uint16_t action)
{
	return action == TRAIN_COUPLING_ORDER_ACTION;
}

/** Post the station coupling/decoupling command for a train. */
inline bool PostTrainCouplingOrderAction(VehicleID vehicle)
{
	return Command<Commands::DecoupleTrain>::Post(vehicle, VehicleID::Invalid()).Succeeded();
}

#endif /* TRAIN_COUPLING_ORDER_H */