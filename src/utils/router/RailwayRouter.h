/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2020 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    RailwayRouter.h
/// @author  Jakob Erdmann
/// @date    Tue, 25 Feb 2020
///
// The RailwayRouter builds a special network for railway routing to handle train reversal restrictions (delegates to a SUMOAbstractRouter)
/****************************************************************************/
#pragma once
#include <config.h>

#include <string>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <utils/common/MsgHandler.h>
#include <utils/common/SUMOTime.h>
#include <utils/common/ToString.h>
#include <utils/iodevices/OutputDevice.h>
#include "SUMOAbstractRouter.h"
#include "DijkstraRouter.h"
#include "RailEdge.h"

//#define RailwayRouter_DEBUG_ROUTES

// ===========================================================================
// class definitions
// ===========================================================================
/**
 * @class RailwayRouter
 * The router for pedestrians (on a bidirectional network of sidewalks and crossings)
 */
template<class E, class V>
class RailwayRouter : public SUMOAbstractRouter<E, V> {

private:


    typedef RailEdge<E, V> _RailEdge;
    typedef SUMOAbstractRouter<_RailEdge, V> _InternalRouter;
    typedef DijkstraRouter<_RailEdge, V> _InternalDijkstra;

public:

    /// Constructor
    RailwayRouter(const std::vector<E*>& edges, bool unbuildIsWarning, typename SUMOAbstractRouter<E, V>::Operation effortOperation,
                   typename SUMOAbstractRouter<E, V>::Operation ttOperation = nullptr, bool silent = false,
                   const bool havePermissions = false, const bool haveRestrictions = false) :
        SUMOAbstractRouter<E, V>("RailwayRouter", true, effortOperation, ttOperation, havePermissions, haveRestrictions),
        myInternalRouter(nullptr)
    {
        myStaticOperation = effortOperation;
        std::vector<_RailEdge*> railEdges1; // a RailEdge for existing edge
        for (E* edge : edges) {
            railEdges1.push_back(edge->getRailwayRoutingEdge());
        }
        int numericalID = railEdges1.back()->getNumericalID() + 1;
        std::vector<_RailEdge*> railEdges2 = railEdges1; // including additional edges for direction reversal
        for (_RailEdge* railEdge : railEdges1) {
            railEdge->init(railEdges2, numericalID, myMaxTrainLength);
        }
        myInternalRouter = new _InternalDijkstra(railEdges2, unbuildIsWarning, &getTravelTimeStatic, nullptr, silent, nullptr, havePermissions, haveRestrictions);

    }

    /// Destructor
    virtual ~RailwayRouter() {
        delete myInternalRouter;
    }

    SUMOAbstractRouter<E, V>* clone() {
        return new RailwayRouter<E, V>(this);
    }

    /** @brief Builds the route between the given edges using the minimum effort at the given time
        The definition of the effort depends on the wished routing scheme */
    bool compute(const E* from, const E* to, const V* const vehicle, SUMOTime msTime, std::vector<const E*>& into, bool silent = false) {
        std::vector<const _RailEdge*> intoTmp;
        bool success = myInternalRouter->compute(from->getRailwayRoutingEdge(), to->getRailwayRoutingEdge(), vehicle, msTime, intoTmp, silent);
        if (success) {
            for (const _RailEdge* railEdge : intoTmp) {
                railEdge->insertOriginalEdges(vehicle->getLength(), into);
            }
        }
        return success;
    }

    void prohibit(const std::vector<E*>& toProhibit) {
        std::vector<_RailEdge*> railEdges; 
        for (E* edge : toProhibit) {
            railEdges.push_back(edge->getRailwayRoutingEdge());
        }
        myInternalRouter->prohibit(railEdges);
    }


private:
    RailwayRouter(RailwayRouter* other) :
        SUMOAbstractRouter<E, V>(other),
        myInternalRouter(other->myInternalRouter->clone())
    {}

    static inline double getTravelTimeStatic(const RailEdge<E, V>* const edge, const V* const veh, double time) {
        if (edge->getOriginal() != nullptr) {
            return  (*myStaticOperation)(edge->getOriginal(), veh, time);
        } else {
            return myReversalPenalty + veh->getLength() * myReversalPenaltyFactor;
        }
    }


private:
    _InternalRouter* myInternalRouter;

    /// @brief The object's operation to perform. (hack)
    static typename SUMOAbstractRouter<E, V>::Operation myStaticOperation;

    static double myReversalPenalty;
    static double myReversalPenaltyFactor;
    static double myMaxTrainLength;

private:
    /// @brief Invalidated assignment operator
    RailwayRouter& operator=(const RailwayRouter& s);

};

template<class E, class V>
typename SUMOAbstractRouter<E, V>::Operation RailwayRouter<E, V>::myStaticOperation(nullptr);
template<class E, class V>
double RailwayRouter<E, V>::myReversalPenalty(60);
template<class E, class V>
double RailwayRouter<E, V>::myReversalPenaltyFactor(0.2); // 1/v
template<class E, class V>
double RailwayRouter<E, V>::myMaxTrainLength(5000);


