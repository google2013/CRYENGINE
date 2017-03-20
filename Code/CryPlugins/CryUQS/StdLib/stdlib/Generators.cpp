// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void CStdLibRegistration::InstantiateGeneratorFactoriesForRegistration()
		{
			static const Client::CGeneratorFactory<CGenerator_PointsOnPureGrid> generatorFactory_PointsOnPureGrid("std::PointsOnPureGrid");
			static const Client::CGeneratorFactory<CGenerator_PointsOnNavMesh> generatorFactory_PointsOnNavMesh("std::PointsOnNavMesh");
		}

		//===================================================================================
		//
		// CGenerator_PointsOnPureGrid
		//
		//===================================================================================

		CGenerator_PointsOnPureGrid::CGenerator_PointsOnPureGrid(const SParams& params)
			: m_params(params)
		{
			// nothing
		}

		Client::IGenerator::EUpdateStatus CGenerator_PointsOnPureGrid::DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate)
		{
			const float halfSize = (float)m_params.size * 0.5f;

			const float minX = m_params.center.value.x - halfSize;
			const float maxX = m_params.center.value.x + halfSize;

			const float minY = m_params.center.value.y - halfSize;
			const float maxY = m_params.center.value.y + halfSize;

			std::vector<Vec3> tmpItems;

			for (float x = minX; x <= maxX; x += m_params.spacing)
			{
				for (float y = minY; y <= maxY; y += m_params.spacing)
				{
					tmpItems.emplace_back(x, y, m_params.center.value.z);
				}
			}

			itemListToPopulate.CreateItemsByItemFactory(tmpItems.size());
			for (size_t i = 0; i < tmpItems.size(); ++i)
			{
				itemListToPopulate.GetItemAtIndex(i).value = tmpItems[i];
			}

			return EUpdateStatus::FinishedGeneratingItems;
		}

		//===================================================================================
		//
		// CGenerator_PointsOnNavMesh
		//
		//===================================================================================

		CGenerator_PointsOnNavMesh::CGenerator_PointsOnNavMesh(const SParams& params)
			: m_params(params)
		{
			// nothing
		}

		Client::IGenerator::EUpdateStatus CGenerator_PointsOnNavMesh::DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate)
		{
			INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

			if (const NavigationMeshID meshID = pNavigationSystem->GetEnclosingMeshID(m_params.navigationAgentTypeID, m_params.pivot.value))
			{
				static const size_t maxTrianglesToLookFor = 1024;

				const AABB aabb(m_params.pivot.value + m_params.localAABBMins.value, m_params.pivot.value + m_params.localAABBMaxs.value);
				Vec3 triangleCenters[maxTrianglesToLookFor];
				const size_t numTrianglesFound = pNavigationSystem->GetTriangleCenterLocationsInMesh(meshID, m_params.pivot.value, aabb, triangleCenters, maxTrianglesToLookFor);

				if (numTrianglesFound > 0)
				{
					// warn if we hit the triangle limit (we might produce "incomplete" data)
					if (numTrianglesFound == maxTrianglesToLookFor)
					{
						CryWarning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_WARNING, "UQS: CGenerator_PointsOnNavMesh::DoUpdate: reached the triangle limit of %i (there might be more triangles that we cannot use)", (int)maxTrianglesToLookFor);
					}

					//
					// populate the given item-list and install an item-monitor that checks for NavMesh changes
					//

					std::unique_ptr<CItemMonitor_NavMeshChangesInAABB> pItemMonitorNavMeshChanges(new CItemMonitor_NavMeshChangesInAABB(m_params.navigationAgentTypeID));
					itemListToPopulate.CreateItemsByItemFactory(numTrianglesFound);

					for (size_t i = 0; i < numTrianglesFound; ++i)
					{
						itemListToPopulate.GetItemAtIndex(i).value = triangleCenters[i];
						pItemMonitorNavMeshChanges->AddPointToMonitoredArea(triangleCenters[i]);
					}

					Core::IHubPlugin::GetHub().GetQueryManager().AddItemMonitorToQuery(updateContext.queryID, std::move(pItemMonitorNavMeshChanges));
				}
			}

			// debug-persist the AABB
			IF_UNLIKELY(updateContext.blackboard.pDebugRenderWorldPersistent)
			{
				updateContext.blackboard.pDebugRenderWorldPersistent->AddAABB(AABB(m_params.pivot.value + m_params.localAABBMins.value, m_params.pivot.value + m_params.localAABBMaxs.value), Col_White);
			}

			return EUpdateStatus::FinishedGeneratingItems;
		}

	}
}
