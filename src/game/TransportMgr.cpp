/*
 * Copyright (C) Continued-MaNGOS Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "TransportMgr.h"
#include "Policies/Singleton.h"
#include "TransportSystem.h"
#include "ProgressBar.h"
#include "movement/spline.h"

using namespace Movement;

INSTANTIATE_SINGLETON_1(TransportMgr);

TransportMgrInfo::TransportMgrInfo(GameObjectInfo const* _goInfo) :
    m_goInfo(_goInfo),
    m_period(0),
    m_spawned(false)
{
    CalculateWaypoints();
}

TransportMgrInfo::~TransportMgrInfo()
{
    // Delete splines
    for (TransportSplineMap::const_iterator itr = m_waypoints.begin(); itr != m_waypoints.end(); ++itr)
        delete itr->second;
}

Movement::Spline<int32>* TransportMgrInfo::GetTransportSplineForMapId(uint32 mapId)
{
    TransportSplineMap::const_iterator itr = m_waypoints.find(mapId);
    MANGOS_ASSERT(itr != m_waypoints.end());

    return itr->second;
}

uint32 TransportMgrInfo::GetNextMapId(uint32 currentMapId)
{
    TransportSplineMap::const_iterator itr = m_waypoints.find(currentMapId);
    MANGOS_ASSERT(itr != m_waypoints.end());

    ++itr;

    if (itr == m_waypoints.end())
        itr = m_waypoints.begin();

    return itr->first;
}

void TransportMgrInfo::CalculateWaypoints()
{
    MANGOS_ASSERT(m_goInfo && m_goInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT && m_goInfo->moTransport.taxiPathId < sTaxiPathNodesByPath.size());

    TaxiPathNodeList const& path = TransportMgr::GetTaxiPathNodeList(m_goInfo->moTransport.taxiPathId);
    MANGOS_ASSERT(path.size() > 0); // Check for empty path

    std::map < uint32 /*mapId*/, SplineBase::ControlArray > transportPaths;

    // First split transport paths
    for (uint32 i = 0; i < path.size(); ++i)
    {
        transportPaths[path[i].mapid].push_back(G3D::Vector3(path[i].x, path[i].y, path[i].z));

        if (path[i].delay)
            m_period += path[i].delay * 1000; // Delay is in seconds
    }

    for (std::map<uint32, SplineBase::ControlArray>::const_iterator itr = transportPaths.begin(); itr != transportPaths.end(); ++itr)
    {
        Spline<int32>* transportSpline = new Spline<int32>();

        // ToDo: Add support for cyclic transport paths
        transportSpline->init_spline(&itr->second[0], itr->second.size(), SplineBase::ModeCatmullrom);

        // The same struct is defined in MoveSpline.cpp
        struct InitCacher
        {
            float velocityInv;
            int32 time;

            InitCacher(float _velocity) :
                velocityInv(1000.f / _velocity), time(1) {}

            inline int operator()(Movement::Spline<int32>& s, int32 i)
            {
                time += (s.SegLength(i) * velocityInv);
                return time;
            }
        };

        InitCacher init(m_goInfo->moTransport.moveSpeed);
        transportSpline->initLengths(init);

        // All points are at same coords
        MANGOS_ASSERT(transportSpline->length() > 1);

        m_waypoints[itr->first] = transportSpline;
        m_period += transportSpline->length();
    }
}

TransportMgr::~TransportMgr()
{
    // Delete transport infos
    for (TransportMgrInfoMap::const_iterator itr = m_transportMgrInfos.begin(); itr != m_transportMgrInfos.end(); ++itr)
        delete itr->second;
}

void TransportMgr::InsertTransporter(GameObjectInfo const* goInfo)
{
    MANGOS_ASSERT(goInfo);

    TransportMgrInfo* transportMgrInfo = new TransportMgrInfo(goInfo);

    m_transportMgrInfos.insert(TransportMgrInfoMap::value_type(goInfo->id, transportMgrInfo));
}

void TransportMgr::LoadTransporterForMap(Map* map)
{
    MANGOS_ASSERT(map);

    for (TransportMgrInfoMap::const_iterator itr = m_transportMgrInfos.begin(); itr != m_transportMgrInfos.end(); ++itr)
    {
        if ((!itr->second->IsSpawned() || map->Instanceable()) && itr->second->IsVisitingThisMap(map->GetId()))
        {
            // Instance transporter must be always in one map. Note: This iterates over all transporters, hence we must check this way
            MANGOS_ASSERT(map->IsContinent() || !itr->second->IsMultiMapTransporter());

            // Needed to avoid creation of multiple multi-map transporter
            itr->second->SetSpawned(CreateTransporter(itr->second, map) != NULL);
        }
    }
}

void TransportMgr::ReachedLastWaypoint(GOTransportBase const* transportBase)
{
    DEBUG_LOG("TransportMgr: Transporter %u reached last waypoint on map %u", transportBase->GetOwner()->GetEntry(), transportBase->GetOwner()->GetMapId());

    MANGOS_ASSERT(transportBase && transportBase->GetOwner()->GetObjectGuid().IsMOTransport());

    TransportMgrInfoMap::const_iterator itr = m_transportMgrInfos.find(transportBase->GetOwner()->GetEntry());
    MANGOS_ASSERT(itr != m_transportMgrInfos.end());

    // Transporter that only move on one map don't need multi-map teleportation
    if (!itr->second->IsMultiMapTransporter())
        return;

    uint32 nextMapId = itr->second->GetNextMapId(transportBase->GetOwner()->GetMapId());

    DEBUG_LOG("TransportMgr: Transporter %u teleport to map %u", transportBase->GetOwner()->GetEntry(), nextMapId);

    Map* nextMap = sMapMgr.CreateMap(nextMapId, NULL);
    MANGOS_ASSERT(nextMap);

    // Create new transporter on the next map
    CreateTransporter(itr->second, nextMap);

    /* Teleport player passengers to the next map,
       and destroy the transporter and it's other passengers */
    for (PassengerMap::const_iterator passengerItr = transportBase->GetPassengers().begin(); passengerItr != transportBase->GetPassengers().end();)
    {
        MANGOS_ASSERT(passengerItr->first);

        if (passengerItr->first->GetTypeId() == TYPEID_PLAYER)
        {
            Player* plr = (Player*)passengerItr->first;

            // Is this correct?
            if (plr->isDead() && !plr->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                plr->ResurrectPlayer(1.0f);

            TransportInfo* transportInfo = plr->GetTransportInfo();
            MANGOS_ASSERT(transportInfo);

            // ToDo: Implement teleport to transports
            // TeleportTo unboards passenger
            //if (!plr->TeleportTo(dynInfo->first /*new map id*/, transportInfo->GetLocalPositionX(), transportInfo->GetLocalPositionY(),
            //  transportInfo->GetLocalPositionZ(), transportInfo->GetLocalOrientation(), 0, NULL, staticInfo->second.goInfo->id))
            //  plr->RepopAtGraveyard();                    // teleport to near graveyard if on transport, looks blizz like :)

            passengerItr = transportBase->GetPassengers().begin();
        }
        else
            ++passengerItr;
    }

#ifdef DEBUG_SHOW_MOT_WAYPOINTS
    transportBase->GetOwner()->GetMap()->MonsterYellToMap(GetCreatureTemplateStore(1), 42, LANG_UNIVERSAL, NULL);
#endif

    // Destroy old transporter
    transportBase->GetOwner()->Delete();
}

Movement::Spline<int32> const* TransportMgr::GetTransportSpline(uint32 goEntry, uint32 mapId)
{
    TransportMgrInfoMap::const_iterator itr = m_transportMgrInfos.find(goEntry);

    if (itr == m_transportMgrInfos.end())
        return NULL;

    return itr->second->GetTransportSplineForMapId(mapId);
}

TaxiPathNodeList const& TransportMgr::GetTaxiPathNodeList(uint32 pathId)
{
    MANGOS_ASSERT(pathId < sTaxiPathNodesByPath.size() && "Generating transport path failed. Check DBC files or transport GO data0 field.");
    return sTaxiPathNodesByPath[pathId];
}

GameObject* TransportMgr::CreateTransporter(TransportMgrInfo* transportMgrInfo, Map* map)
{
    MANGOS_ASSERT(transportMgrInfo && map);

    GameObjectInfo const* goInfo = transportMgrInfo->GetGameObjectInfo();

    Movement::Spline<int32>* startSpline = transportMgrInfo->GetTransportSplineForMapId(map->GetId());
    G3D::Vector3 const& startPos = startSpline->getPoint(startSpline->first());

    DEBUG_LOG("Create transporter %s, map %u", goInfo->name, map->GetId());

    GameObject* transporter = new GameObject;

    // Guid == Entry :? Orientation :?
    if (!transporter->Create(goInfo->id, goInfo->id, map, PHASEMASK_ANYWHERE, startPos.x, startPos.y, startPos.z, 0.0f))
    {
        delete transporter;
        return NULL;
    }

    transporter->SetName(goInfo->name);
    // Most massive object transporter are always active objects
    transporter->SetActiveObjectState(true);
    // Set period
    transporter->SetUInt32Value(GAMEOBJECT_LEVEL, transportMgrInfo->GetPeriod());
    // Add the transporter to the map
    map->Add<GameObject>(transporter);

#ifdef DEBUG_SHOW_MOT_WAYPOINTS
    // Debug helper, view waypoints
    TaxiPathNodeList const& path = GetTaxiPathNodeList(goInfo->moTransport.taxiPathId);
    for (uint32 i = 0; i < path.size(); ++i)
    {
        if (path[i].mapid != map->GetId())
            continue;

        if (Creature* pSummoned = transporter->SummonCreature(1, path[i].x, path[i].y, path[i].z + 30.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, transportMgrInfo->GetPeriod(), true))
            pSummoned->SetObjectScale(1.0f + (path.size() - i) * 1.0f);
    }
    // Debug helper, yell a bit
    map->MonsterYellToMap(GetCreatureTemplateStore(1), 41, LANG_UNIVERSAL, NULL);
#endif

    return transporter;
}
