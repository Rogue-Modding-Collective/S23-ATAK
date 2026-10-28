// Terrain metadata only: never open or reconfigure the world's native map UI.
class ATAK_MapData
{
	SCR_MapEntity Entity;
	ResourceName Texture;
	vector Offset;
	vector Size;
	string UnavailableReason;

	bool Load()
	{
		Entity = SCR_MapEntity.GetMapInstance();
		if (!Entity)
			return Unavailable("No terrain map available");

		EntityPrefabData prefabData = Entity.GetPrefabData();
		BaseContainer prefab;
		if (prefabData)
			prefab = prefabData.GetPrefab();
		if (!prefab)
			return Unavailable("Terrain map configuration unavailable");

		Texture = ResourceName.Empty;
		prefab.Get("Satellite background image", Texture);
		if (Texture.IsEmpty())
			return Unavailable("Terrain imagery unavailable");

		Offset = Entity.Offset();
		Size = Entity.Size();
		if (Size[0] <= 0 || Size[2] <= 0)
			return Unavailable("Terrain bounds unavailable");

		UnavailableReason = string.Empty;
		return true;
	}

	protected bool Unavailable(string reason)
	{
		if (UnavailableReason != reason)
			PrintFormat("[ATAK] %1", reason);
		UnavailableReason = reason;
		return false;
	}
}

// Session view state and coordinate math, shared by phone/tablet presentations.
// X is east, Z is north. Radius is the visible half-width in world metres.
class ATAK_MapViewState
{
	vector Center;
	float Radius = 550;
	float MinRadius = 50;
	float MaxRadius = 5000;
	float CanvasWidth = 1024;
	float CanvasHeight = 1024;
	float DisplayAspect = 0.461538;
	bool Follow = true;
	bool Compact; // Ordinary square HUD presentation; physical displays use full chrome.

	void Configure(float width, float height, float aspect, float initialRadius, float minRadius, float maxRadius)
	{
		CanvasWidth = Math.Max(1, width);
		CanvasHeight = Math.Max(1, height);
		DisplayAspect = Math.Max(0.1, aspect);
		MinRadius = Math.Max(1, minRadius);
		MaxRadius = Math.Max(MinRadius, maxRadius);
		Radius = Math.Clamp(initialRadius, MinRadius, MaxRadius);
	}

	float HalfHeight()
	{
		return Radius / DisplayAspect;
	}

	float ScaleX()
	{
		return CanvasWidth / (2 * Radius);
	}

	float ScaleY()
	{
		return CanvasHeight / (2 * HalfHeight());
	}

	vector WorldToDisplay(vector position)
	{
		return Vector(CanvasWidth * 0.5 + (position[0] - Center[0]) * ScaleX(),
			CanvasHeight * 0.5 - (position[2] - Center[2]) * ScaleY(), 0);
	}

	void CenterOn(vector position)
	{
		Center = position;
		Follow = true;
	}

	void Pan(float east, float north, float timeSlice, vector mapOffset, vector mapSize)
	{
		if (east == 0 && north == 0)
			return;

		// One half-width per second; normalize diagonals and cap hitch movement.
		float length = Math.Sqrt(east * east + north * north);
		float distance = Radius * Math.Clamp(timeSlice, 0, 0.1) / Math.Max(1, length);
		Center[0] = Math.Clamp(Center[0] + east * distance, mapOffset[0], mapOffset[0] + mapSize[0]);
		Center[2] = Math.Clamp(Center[2] + north * distance, mapOffset[2], mapOffset[2] + mapSize[2]);
		Follow = false;
	}

	void StepZoom(bool zoomIn)
	{
		float factor = 1.5;
		if (zoomIn)
			factor = 1 / factor;
		Radius = Math.Clamp(Radius * factor, MinRadius, MaxRadius);
	}
}
