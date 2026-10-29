// Owns widgets and renders a north-up view. Data and navigation are device-neutral.
class ATAK_ScreenDisplayController
{
	protected IEntity m_pOwner;
	protected IEntity m_pViewer;
	protected Widget m_wRoot;
	protected FrameWidget m_wMapCanvas;
	protected RTTextureWidget m_wRenderTarget;
	protected ImageWidget m_wMapImage;
	protected CanvasWidget m_wMapMarker;
	// CanvasWidget keeps a pointer to this array: retain it for the widget lifetime.
	protected ref array<ref CanvasWidgetCommand> m_aMarkerCommands = {};
	protected ref PolygonDrawCommand m_MarkerShadow;
	protected ref PolygonDrawCommand m_MarkerBorder;
	protected ref PolygonDrawCommand m_MarkerFill;
	protected ref ATAK_DeviceUIState m_UIState = new ATAK_DeviceUIState();
	protected ref ATAK_DeviceUI m_UI;
	protected int m_iAppliedBrightness = -1;
	protected ref ATAK_MapData m_MapData = new ATAK_MapData();
	protected ref ATAK_MapViewState m_View = new ATAK_MapViewState();
	protected ref ATAK_MapOverlayController m_Overlays;
	protected ref ATAK_MapGridController m_Grid;
	protected bool m_bMapDataLoaded;
	protected bool m_bRunning;
	protected float m_fRetryAfter;
	protected string m_sClock = "--:--";
	protected bool m_bHUD;
	protected ref ATAK_SquadData m_Squad = new ATAK_SquadData();
	protected float m_fSquadRefreshAfter;
	protected TextWidget m_wHUDSquad;
	protected TextWidget m_wHUDTitle;
	protected TextWidget m_wHUDReadout;

	void ConfigureMap(float width, float height, float aspect, float initialRadius, float minRadius, float maxRadius)
	{
		m_View.Configure(width, height, aspect, initialRadius, minRadius, maxRadius);
	}

	bool Start(IEntity owner, ResourceName layout)
	{
		if (!owner || layout == ResourceName.Empty)
			return false;

		if (m_wRoot && m_pOwner == owner)
		{
			if (!m_bRunning)
				Recenter();
			m_pViewer = SCR_PlayerController.GetLocalControlledEntity();
			m_wRenderTarget.SetRenderTarget(owner);
			m_wRenderTarget.SetEnabled(true);
			if (m_UI)
				m_UI.Invalidate();
			if (!m_bRunning) m_fSquadRefreshAfter = 0;
			m_bRunning = true;
			return true;
		}

		Destroy();
		m_wRoot = GetGame().GetWorkspace().CreateWidgets(layout);
		if (!m_wRoot)
			return false;

		m_wRenderTarget = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("RTTexture0"));
		m_wMapCanvas = FrameWidget.Cast(m_wRoot.FindAnyWidget("MapCanvas"));
		m_wMapMarker = CanvasWidget.Cast(m_wRoot.FindAnyWidget("MapMarker"));
		if (!m_wRenderTarget || !m_wMapCanvas || !m_wMapMarker)
		{
			Destroy();
			return false;
		}

		Widget prototypeText = m_wRoot.FindAnyWidget("Text0");
		if (prototypeText)
			prototypeText.SetVisible(false);

		FrameSlot.SetSize(m_wRoot, m_View.CanvasWidth, m_View.CanvasHeight);
		FrameSlot.SetSize(m_wRenderTarget, m_View.CanvasWidth, m_View.CanvasHeight);
		if (!CreateMapLayers(m_wRenderTarget))
			return false;

		m_UI = new ATAK_DeviceUI();
		if (!m_UI.Start(m_wRenderTarget, m_View))
		{
			Destroy();
			return false;
		}
		m_UI.SetClock(m_sClock);

		m_pOwner = owner;
		m_pViewer = SCR_PlayerController.GetLocalControlledEntity();
		Recenter();
		m_wRenderTarget.SetRenderTarget(owner);
		m_wRenderTarget.SetEnabled(true);
		m_bRunning = true;
		return true;
	}


	// Both the mesh RT and ordinary HUD widgets use these exact map layers.
	protected bool CreateMapLayers(Widget parent)
	{
		m_wMapImage = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(
			WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.STRETCH | WidgetFlags.BLEND | WidgetFlags.CUSTOMUV | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, m_wMapCanvas));
		if (!m_wMapImage)
		{
			Destroy();
			return false;
		}

		FrameSlot.SetAnchorMin(m_wMapImage, 0, 0);
		FrameSlot.SetAnchorMax(m_wMapImage, 0, 0);
		FrameSlot.SetOffsets(m_wMapImage, 0, 0, 0, 0);
		FrameSlot.SetAlignment(m_wMapImage, 0, 0);
		m_wMapImage.SetZOrder(-1);
		m_wMapImage.SetVisible(false);
		FrameSlot.SetAnchorMin(m_wMapMarker, 0, 0);
		FrameSlot.SetAnchorMax(m_wMapMarker, 1, 1);
		FrameSlot.SetOffsets(m_wMapMarker, 0, 0, 0, 0);
		FrameSlot.SetAlignment(m_wMapMarker, 0, 0);
		m_wMapMarker.SetSizeInUnits(Vector(m_View.CanvasWidth, m_View.CanvasHeight, 0));
		m_wMapMarker.SetZoom(1);
		m_wMapMarker.SetOffsetPx(Vector(0, 0, 0));
		m_wMapMarker.SetZOrder(10);
		m_wMapMarker.SetIsColorInherited(false);
		m_MarkerShadow = CreateMarkerPolygon(0xFF071829);
		m_MarkerBorder = CreateMarkerPolygon(0xFFFFFFFF);
		m_MarkerFill = CreateMarkerPolygon(0xFF1680FF);
		m_wMapMarker.SetVisible(false);

		m_Overlays = new ATAK_MapOverlayController();
		if (!m_Overlays.Start(parent, m_View))
			Print("[ATAK] Could not create cartographic overlay canvas");
		m_Grid = new ATAK_MapGridController();
		if (!m_Grid.Start(parent, m_View, false))
			Print("[ATAK] Could not create map grid display");

		return true;
	}

	bool StartHUD(float size, int corner)
	{
		if (m_wRoot && m_bHUD)
		{
			m_wRoot.SetVisible(true);
			m_pViewer = SCR_PlayerController.GetLocalControlledEntity();
			m_bRunning = true;
			return true;
		}
		Destroy();
		m_bHUD = true;
		ConfigureMap(size, size, 1, 550, 50, 5000);
		m_View.Compact = true;
		m_wRoot = GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.CLIPCHILDREN,
			new Color(1, 1, 1, 1), 0, GetGame().GetWorkspace());
		if (!m_wRoot)
			return false;
		float anchorX = 0;
		float anchorY = 0;
		if (corner == 1 || corner == 3) anchorX = 1;
		if (corner >= 2) anchorY = 1;
		FrameSlot.SetAnchorMin(m_wRoot, anchorX, anchorY);
		FrameSlot.SetAnchorMax(m_wRoot, anchorX, anchorY);
		FrameSlot.SetAlignment(m_wRoot, anchorX, anchorY);
		FrameSlot.SetPos(m_wRoot, 24 * (1 - 2 * anchorX), 24 * (1 - 2 * anchorY));
		FrameSlot.SetSize(m_wRoot, size, size);
		m_wRoot.SetZOrder(5);
		ImageWidget background = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.STRETCH,
			new Color(0.035, 0.05, 0.07, 1), 0, m_wRoot));
		m_wMapCanvas = FrameWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.FrameWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, m_wRoot));
		m_wMapMarker = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.CanvasWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, m_wRoot));
		if (!background || !m_wMapCanvas || !m_wMapMarker)
		{
			Destroy();
			return false;
		}
		FrameSlot.SetAnchorMin(background, 0, 0);
		FrameSlot.SetAnchorMax(background, 1, 1);
		FrameSlot.SetOffsets(background, 0, 0, 0, 0);
		background.SetZOrder(-20);
		FrameSlot.SetAnchorMin(m_wMapCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wMapCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wMapCanvas, 0, 0, 0, 0);
		m_wMapCanvas.SetZOrder(-10);
		if (!CreateMapLayers(m_wRoot))
			return false;
		ImageWidget header = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.STRETCH,
			new Color(0.035, 0.05, 0.07, 0.95), 0, m_wRoot));
		if (!header)
		{
			Destroy();
			return false;
		}
		FrameSlot.SetAnchorMin(header, 0, 0);
		FrameSlot.SetAnchorMax(header, 0, 0);
		FrameSlot.SetPos(header, 0, 0);
		FrameSlot.SetSize(header, size, 52);
		header.SetZOrder(30);
		m_wHUDTitle = CreateHUDText(8, 4, size - 16, 22);
		m_wHUDSquad = CreateHUDText(8, 26, size - 16, 22);
		m_wHUDReadout = CreateHUDText(8, size - 28, size - 16, 22);
		if (!m_wHUDTitle || !m_wHUDReadout || !m_wHUDSquad)
		{
			Destroy();
			return false;
		}
		m_wHUDTitle.SetText("ATAK | N UP");
		m_pViewer = SCR_PlayerController.GetLocalControlledEntity();
		m_bRunning = true;
		return true;
	}

	protected TextWidget CreateHUDText(float x, float y, float width, float height)
	{
		TextWidget label = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, m_wRoot));
		if (!label)
			return null;
		FrameSlot.SetAnchorMin(label, 0, 0);
		FrameSlot.SetAnchorMax(label, 0, 0);
		FrameSlot.SetPos(label, x, y);
		FrameSlot.SetSize(label, width, height);
		label.SetFont("{D3E75BCBECEB273E}UI/Fonts/NotoSans/NotoSans-Enhanced-Regular-JP-KOR-SC-LAT.fnt");
		label.SetExactFontSize(14);
		label.SetOutline(2, 0xFF07121B);
		label.SetIsColorInherited(false);
		label.SetZOrder(31);
		return label;
	}

	void UpdateHUD(float radius, int brightness, float timeSlice)
	{
		m_View.Radius = Math.Clamp(radius, m_View.MinRadius, m_View.MaxRadius);
		m_View.Follow = true;
		m_UIState.m_iBrightnessPercent = brightness;
		UpdateMapDisplay(timeSlice);
	}

	bool HasDisplay()
	{
		return m_bRunning && m_UI && m_pViewer && m_pViewer == SCR_PlayerController.GetLocalControlledEntity();
	}

	bool IsMapPage()
	{
		return m_UIState.m_ePage == ATAK_EDevicePage.ATAK_MAP;
	}

	void BootHome()
	{
		m_UIState.BootHome();
		m_fSquadRefreshAfter = 0;
	}

	void NavigateUI(int east, int north)
	{
		m_UIState.Navigate(east, north);
	}

	void SelectUI()
	{
		ATAK_EDevicePage previousPage = m_UIState.m_ePage;
		m_UIState.Select();
		if (previousPage != m_UIState.m_ePage) m_fSquadRefreshAfter = 0;
	}

	void RefreshBindings()
	{
		if (m_UI)
			m_UI.Invalidate();
	}

	bool HasMap()
	{
		return m_bRunning && m_bMapDataLoaded && m_pViewer && m_pViewer == SCR_PlayerController.GetLocalControlledEntity();
	}

	void Pan(float east, float north, float timeSlice)
	{
		if (IsMapPage() && HasMap())
			m_View.Pan(east, north, timeSlice, m_MapData.Offset, m_MapData.Size);
	}

	void StepZoom(bool zoomIn)
	{
		if (IsMapPage())
			m_View.StepZoom(zoomIn);
	}

	void Recenter()
	{
		m_View.Follow = true;
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (player)
			m_View.CenterOn(player.GetOrigin());
	}

	protected bool LoadMapData()
	{
		if (!m_MapData.Load())
			return false;
		if (!m_wMapImage.LoadImageTexture(0, m_MapData.Texture))
		{
			m_MapData.UnavailableReason = "Terrain imagery could not be loaded";
			return false;
		}
		m_bMapDataLoaded = true;
		return true;
	}

	protected void RefreshSquad(float timeSlice)
	{
		m_fSquadRefreshAfter -= timeSlice;
		if (m_fSquadRefreshAfter > 0) return;
		m_fSquadRefreshAfter = 1;
		int previousGroup = m_Squad.m_iGroupId;
		m_Squad.Refresh(!m_bHUD && m_UIState.m_ePage == ATAK_EDevicePage.ATAK_SQUAD);
		if (previousGroup != m_Squad.m_iGroupId) m_UIState.m_iSquadOffset = 0;
		m_UIState.m_iSquadCount = m_Squad.m_Members.Count();
		m_UIState.m_iSquadOffset = Math.ClampInt(m_UIState.m_iSquadOffset, 0, Math.Max(0, m_UIState.m_iSquadCount - 8));
		if (m_UI) m_UI.SetSquad(m_Squad);
		if (m_wHUDSquad)
		{
			float width = FrameSlot.GetSizeX(m_wHUDSquad);
			ATAK_DeviceUI.FitText(m_wHUDSquad, m_Squad.Identity(), width);
		}
	}

	void UpdateMapDisplay(float timeSlice = 0)
	{
		if (!m_bRunning || !m_wMapImage)
			return;

		RefreshSquad(timeSlice);
		if (m_UI)
			m_UI.Draw(m_UIState);
		if (!m_bHUD && !m_UIState.ShowsMap())
		{
			HideMapLayers();
			return; // No terrain/road/grid polling behind the launcher.
		}
		if (m_iAppliedBrightness != m_UIState.m_iBrightnessPercent)
		{
			float brightness = m_UIState.m_iBrightnessPercent / 100.0;
			m_wMapImage.SetIsColorInherited(false);
			m_wMapImage.SetColor(new Color(brightness, brightness, brightness, 1));
			m_iAppliedBrightness = m_UIState.m_iBrightnessPercent;
		}

		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player || player != m_pViewer)
		{
			HideMap("Player unavailable");
			return;
		}

		// Only retry unavailable terrain data once per second; never open a map menu.
		if (m_MapData.Entity != SCR_MapEntity.GetMapInstance())
		{
			m_bMapDataLoaded = false;
			m_fRetryAfter = 0;
		}
		if (!m_bMapDataLoaded)
		{
			m_fRetryAfter -= timeSlice;
			if (m_fRetryAfter <= 0)
			{
				m_fRetryAfter = 1;
				LoadMapData();
			}
			if (!m_bMapDataLoaded)
			{
				HideMap(m_MapData.UnavailableReason);
				return;
			}
		}

		vector position = player.GetOrigin();
		if (m_View.Follow)
			m_View.CenterOn(position);

		// Crop the view to terrain bounds. Position the cropped image with the SAME
		// transform as the marker, leaving background beyond the terrain edges.
		float minX = Math.Max(m_View.Center[0] - m_View.Radius, m_MapData.Offset[0]);
		float maxX = Math.Min(m_View.Center[0] + m_View.Radius, m_MapData.Offset[0] + m_MapData.Size[0]);
		float minZ = Math.Max(m_View.Center[2] - m_View.HalfHeight(), m_MapData.Offset[2]);
		float maxZ = Math.Min(m_View.Center[2] + m_View.HalfHeight(), m_MapData.Offset[2] + m_MapData.Size[2]);
		if (maxX <= minX || maxZ <= minZ)
		{
			HideMap("Outside terrain imagery");
			return;
		}

		float u0 = (minX - m_MapData.Offset[0]) / m_MapData.Size[0];
		float u1 = (maxX - m_MapData.Offset[0]) / m_MapData.Size[0];
		float v0 = 1 - (maxZ - m_MapData.Offset[2]) / m_MapData.Size[2];
		float v1 = 1 - (minZ - m_MapData.Offset[2]) / m_MapData.Size[2];
		float uv[4][2];
		uv[0][0] = u0; uv[0][1] = v0;
		uv[1][0] = u1; uv[1][1] = v0;
		uv[2][0] = u1; uv[2][1] = v1;
		uv[3][0] = u0; uv[3][1] = v1;
		m_wMapImage.SetUV(uv);

		vector topLeft = m_View.WorldToDisplay(Vector(minX, 0, maxZ));
		FrameSlot.SetSize(m_wMapImage, (maxX - minX) * m_View.ScaleX(), (maxZ - minZ) * m_View.ScaleY());
		FrameSlot.SetPos(m_wMapImage, topLeft[0], topLeft[1]);
		m_wMapImage.SetVisible(true);

		if (m_Grid)
			m_Grid.Update(m_MapData, m_View, position);
		if (m_Overlays)
			m_Overlays.Update(m_MapData, m_View, position, timeSlice);

		vector marker = m_View.WorldToDisplay(position);
		vector angles = player.GetYawPitchRoll();
		float yaw = angles[0] * Math.DEG2RAD;
		UpdatePlayerMarker(marker, yaw);
		float marginY = 30 * m_View.CanvasHeight / 1024;
		float marginX = marginY * m_View.CanvasWidth / (m_View.CanvasHeight * m_View.DisplayAspect);
		bool markerVisible = marker[0] >= marginX && marker[0] <= m_View.CanvasWidth - marginX
			&& marker[1] >= marginY && marker[1] <= m_View.CanvasHeight - marginY;
		m_wMapMarker.SetVisible(markerVisible);
		int heading = angles[0] + 360;
		heading = heading % 360;
		string mode = "FOLLOW";
		if (!m_View.Follow)
			mode = "FREE PAN";
		int halfWidth = m_View.Radius;
		string warning;
		if (m_Overlays)
			warning = m_Overlays.GetStatus();
		if (m_UI)
			m_UI.SetMapInfo(m_UIState, position, heading, mode, halfWidth, ATAK_MapGrid.GetSpacing(m_View), warning);
		if (m_wHUDReadout)
		{
			string readout = string.Format("GRID %1 | +/- %2 m", ATAK_MapGrid.FormatPosition(position), halfWidth);
			if (m_wHUDReadout.GetText() != readout) m_wHUDReadout.SetText(readout);
		}
	}

	protected PolygonDrawCommand CreateMarkerPolygon(int color)
	{
		PolygonDrawCommand polygon = new PolygonDrawCommand();
		polygon.m_iColor = color;
		polygon.m_Vertices = new array<float>();
		polygon.m_Vertices.Resize(6);
		m_aMarkerCommands.Insert(polygon);
		return polygon;
	}

	protected void UpdatePlayerMarker(vector center, float yaw)
	{
		float sinYaw = Math.Sin(yaw);
		float cosYaw = Math.Cos(yaw);
		// Largest first: dark rim, white outline, blue arrow. No font/texture asset.
		SetMarkerTriangle(m_MarkerShadow, center, sinYaw, cosYaw, 28, 20, 22);
		SetMarkerTriangle(m_MarkerBorder, center, sinYaw, cosYaw, 24, 17, 19);
		SetMarkerTriangle(m_MarkerFill, center, sinYaw, cosYaw, 20, 14, 16);
		m_wMapMarker.SetDrawCommands(m_aMarkerCommands);
	}

	protected void SetMarkerTriangle(PolygonDrawCommand polygon, vector center, float sinYaw, float cosYaw,
		float tip, float halfWidth, float back)
	{
		float size = m_View.CanvasHeight / 1024;
		if (m_bHUD) size = Math.Max(size, 0.45);
		float aspectCorrection = m_View.CanvasWidth / (m_View.CanvasHeight * m_View.DisplayAspect);
		for (int i = 0; i < 3; i++)
		{
			float x = 0;
			float y = -tip;
			if (i == 1)
			{
				x = halfWidth;
				y = back;
			}
			else if (i == 2)
			{
				x = -halfWidth;
				y = back;
			}

			// Rotate in physical screen space BEFORE compensating for the portrait
			// surface. This preserves both arrow proportions and heading at any yaw.
			float displayX = center[0] + (x * cosYaw - y * sinYaw) * size * aspectCorrection;
			float displayY = center[1] + (x * sinYaw + y * cosYaw) * size;
			vector pixel = m_wMapMarker.PosToPixels(Vector(displayX, displayY, 0));
			polygon.m_Vertices[i * 2] = pixel[0];
			polygon.m_Vertices[i * 2 + 1] = pixel[1];
		}
	}

	protected void HideMapLayers()
	{
		if (m_Grid)
			m_Grid.Stop();
		if (m_Overlays)
			m_Overlays.Stop();
		if (m_wMapImage)
			m_wMapImage.SetVisible(false);
		if (m_wMapMarker)
			m_wMapMarker.SetVisible(false);
	}

	protected void HideMap(string reason)
	{
		HideMapLayers();
		if (m_wHUDReadout) m_wHUDReadout.SetText("MAP UNAVAILABLE");
		if (m_UI)
			m_UI.SetMapUnavailable(m_UIState, reason);
	}

	bool SetClock(string clockText)
	{
		m_sClock = clockText;
		if (m_UI)
			m_UI.SetClock(clockText);
		return m_UI != null;
	}

	void Stop()
	{
		m_bRunning = false;
		m_fSquadRefreshAfter = 0;
		if (m_bHUD && m_wRoot) m_wRoot.SetVisible(false);
		HideMapLayers();
		if (m_UI)
			m_UI.Stop();
		if (m_wRenderTarget)
		{
			m_wRenderTarget.SetEnabled(false);
			if (m_pOwner && !m_pOwner.IsDeleted())
				m_wRenderTarget.RemoveRenderTarget(m_pOwner);
		}
	}

	void Destroy()
	{
		Stop();
		if (m_UI)
			m_UI.Destroy();
		m_UI = null;
		if (m_Overlays)
			m_Overlays.Destroy();
		m_Overlays = null;
		if (m_Grid)
			m_Grid.Destroy();
		m_Grid = null;
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		m_wMapImage = null;
		m_wMapMarker = null;
		m_aMarkerCommands.Clear();
		m_MarkerShadow = null;
		m_MarkerBorder = null;
		m_MarkerFill = null;
		m_wMapCanvas = null;
		m_wRenderTarget = null;
		m_wRoot = null;
		m_wHUDTitle = null;
		m_wHUDSquad = null;
		m_wHUDReadout = null;
		m_bHUD = false;
		m_View.Compact = false;
		m_pOwner = null;
		m_pViewer = null;
		m_bMapDataLoaded = false;
		m_fRetryAfter = 0;
		m_iAppliedBrightness = -1;
	}
}
