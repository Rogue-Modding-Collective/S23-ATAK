// Read-only cartographic data. No native-map view state or mission edits.
class ATAK_MapRoad
{
	ref array<vector> Points = {};
	float Width;
	float MinX;
	float MaxX;
	float MinZ;
	float MaxZ;
}

class ATAK_MapPlace
{
	string Name;
	vector Position;
	int Type;
}

class ATAK_MapOverlayData
{
	ref array<ref ATAK_MapRoad> Roads = {};
	ref array<ref ATAK_MapPlace> Places = {};
	int m_iRevision;
	string RoadStatus;

	protected SCR_MapEntity m_Map;
	protected ref array<BaseRoad> m_PendingRoads = {};
	protected int m_iNextRoad;
	protected int m_iPointCount;
	protected bool m_bCoverage;
	protected bool m_bBuilding;
	protected float m_fMinX;
	protected float m_fMaxX;
	protected float m_fMinZ;
	protected float m_fMaxZ;
	protected float m_fRoadRetry;
	protected float m_fPlaceRetry;
	protected string m_sLastRoadWarning;
	protected int m_iLastPlaceCount = -1;

	void Update(ATAK_MapData mapData, ATAK_MapViewState view, float timeSlice)
	{
		if (m_Map != mapData.Entity)
		{
			m_Map = mapData.Entity;
			Roads.Clear();
			Places.Clear();
			m_PendingRoads.Clear();
			m_bCoverage = false;
			m_bBuilding = false;
			m_fRoadRetry = 0;
			m_fPlaceRetry = 0;
			m_iRevision++;
		}
		if (!m_Map)
			return;

		m_fPlaceRetry -= timeSlice;
		if (m_fPlaceRetry <= 0)
		{
			LoadPlaces();
			m_fPlaceRetry = 15;
			if (Places.IsEmpty())
				m_fPlaceRetry = 5;
		}

		m_fRoadRetry -= timeSlice;
		bool inside = m_bCoverage && view.Center[0] - view.Radius >= m_fMinX
			&& view.Center[0] + view.Radius <= m_fMaxX
			&& view.Center[2] - view.HalfHeight() >= m_fMinZ
			&& view.Center[2] + view.HalfHeight() <= m_fMaxZ;
		// Prefetch a margin and retain it while moving/zooming inside it. Retry
		// unavailable local road data slowly, rather than query on every frame.
		if (!inside && m_fRoadRetry <= 0)
			BeginRoadQuery(view);
		if (m_bBuilding)
			ReadRoadBatch();
	}

	protected void SetRoadWarning(string text)
	{
		RoadStatus = text;
		if (m_sLastRoadWarning != text)
		{
			PrintFormat("[ATAK] %1", text);
			m_sLastRoadWarning = text;
		}
	}

	protected void BeginRoadQuery(ATAK_MapViewState view)
	{
		m_fRoadRetry = 1;
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		RoadNetworkManager manager;
		if (aiWorld)
			manager = aiWorld.GetRoadNetworkManager();
		if (!manager)
		{
			SetRoadWarning("Local road network unavailable");
			m_fRoadRetry = 5;
			return;
		}

		m_fMinX = view.Center[0] - view.Radius * 1.5;
		m_fMaxX = view.Center[0] + view.Radius * 1.5;
		m_fMinZ = view.Center[2] - view.HalfHeight() * 1.5;
		m_fMaxZ = view.Center[2] + view.HalfHeight() * 1.5;
		m_PendingRoads.Clear();
		manager.GetRoadsInAABB(Vector(m_fMinX, m_Map.MinElevation() - 10, m_fMinZ),
			Vector(m_fMaxX, m_Map.MaxElevation() + 10, m_fMaxZ), m_PendingRoads);
		Roads.Clear();
		m_iNextRoad = 0;
		m_iPointCount = 0;
		m_bCoverage = !m_PendingRoads.IsEmpty();
		m_bBuilding = m_bCoverage;
		m_iRevision++;
		if (!m_bCoverage)
		{
			SetRoadWarning("No local roads returned for this area");
			m_fRoadRetry = 5;
			return;
		}
		RoadStatus = "Loading roads";
	}

	protected void ReadRoadBatch()
	{
		int started = System.GetTickCount();
		int batchCount;
		// Limit extraction to 16 roads / approximately 3 ms per visible update.
		// Bound retained point data even when a zoomed-out query spans the terrain.
		while (m_iNextRoad < m_PendingRoads.Count() && batchCount < 16 && m_iPointCount < 100000)
		{
			BaseRoad source = m_PendingRoads[m_iNextRoad++];
			batchCount++;
			if (!source)
				continue;
			float width = source.GetWidth();
			if (width <= 0)
				continue;

			ATAK_MapRoad road = new ATAK_MapRoad();
			source.GetPoints(road.Points);
			int count = road.Points.Count();
			if (count < 2)
				continue;
			if (m_iPointCount + count > 100000)
			{
				m_iPointCount = 100000;
				break;
			}
			road.Width = width;
			vector first = road.Points[0];
			road.MinX = first[0];
			road.MaxX = road.MinX;
			road.MinZ = first[2];
			road.MaxZ = road.MinZ;
			foreach (vector point : road.Points)
			{
				road.MinX = Math.Min(road.MinX, point[0]);
				road.MaxX = Math.Max(road.MaxX, point[0]);
				road.MinZ = Math.Min(road.MinZ, point[2]);
				road.MaxZ = Math.Max(road.MaxZ, point[2]);
			}
			m_iPointCount += count;
			Roads.Insert(road);
			if (System.GetTickCount() - started >= 3)
				break;
		}
		m_iRevision++;
		if (m_iNextRoad >= m_PendingRoads.Count() || m_iPointCount >= 100000)
		{
			m_bBuilding = false;
			m_PendingRoads.Clear();
			RoadStatus = string.Empty;
			if (m_iPointCount >= 100000)
				RoadStatus = "Road cache detail limit";
			else if (Roads.IsEmpty())
			{
				RoadStatus = "No local road geometry";
				m_bCoverage = false;
				m_fRoadRetry = 5;
			}
			PrintFormat("[ATAK] Cartography: %1 roads, %2 points, %3 place names", Roads.Count(), m_iPointCount, Places.Count());
			m_sLastRoadWarning = string.Empty;
		}
	}

	protected void LoadPlaces()
	{
		Places.Clear();
		// Settlement names first so label collision handling favors towns over
		// minor terrain names. Exclude units, bases, objectives and editor markers.
		array<int> types = {EMapDescriptorType.MDT_NAME_CITY, EMapDescriptorType.MDT_NAME_TOWN,
			EMapDescriptorType.MDT_NAME_VILLAGE, EMapDescriptorType.MDT_NAME_SETTLEMENT,
			EMapDescriptorType.MDT_NAME_ISLAND, EMapDescriptorType.MDT_NAME_LOCAL,
			EMapDescriptorType.MDT_NAME_HILL, EMapDescriptorType.MDT_NAME_GENERIC};
		array<MapItem> items = {};
		foreach (int type : types)
		{
			items.Clear();
			m_Map.GetByType(items, type);
			foreach (MapItem item : items)
			{
				if (!item)
					continue;
				IEntity entity = item.Entity();
				string name = item.GetDisplayName();
				if (!entity || name.IsEmpty())
					continue;
				ATAK_MapPlace place = new ATAK_MapPlace();
				place.Name = name;
				// Avoid MapItem's packed 2D coordinate convention: the descriptor's
				// owner supplies an ordinary world X/Y/Z position.
				place.Position = entity.GetOrigin();
				place.Type = type;
				Places.Insert(place);
			}
		}
		if (m_iLastPlaceCount != Places.Count())
		{
			PrintFormat("[ATAK] Cartography: %1 static place names from map descriptors", Places.Count());
			m_iLastPlaceCount = Places.Count();
		}
		m_iRevision++;
	}
}

// Liang-Barsky clipping in display coordinates, including crossing segments
// whose two endpoints are both outside the phone's view.
class ATAK_MapClip
{
	static bool ClipBoundary(float p, float q, inout float enter, inout float leave)
	{
		if (p == 0)
			return q >= 0;
		float t = q / p;
		if (p < 0)
		{
			if (t > leave)
				return false;
			enter = Math.Max(enter, t);
		}
		else
		{
			if (t < enter)
				return false;
			leave = Math.Min(leave, t);
		}
		return true;
	}

	static bool Segment(inout vector a, inout vector b, float width, float height)
	{
		float dx = b[0] - a[0];
		float dy = b[1] - a[1];
		float enter = 0;
		float leave = 1;
		if (!ATAK_MapClip.ClipBoundary(-dx, a[0], enter, leave)
			|| !ATAK_MapClip.ClipBoundary(dx, width - a[0], enter, leave)
			|| !ATAK_MapClip.ClipBoundary(-dy, a[1], enter, leave)
			|| !ATAK_MapClip.ClipBoundary(dy, height - a[1], enter, leave))
			return false;
		b = Vector(a[0] + leave * dx, a[1] + leave * dy, 0);
		a = Vector(a[0] + enter * dx, a[1] + enter * dy, 0);
		return true;
	}
}
