// The native grid is measured from world X/Z zero, independently of texture
// bounds. Do not subtract terrain offsets or use a panned view as player position.
class ATAK_MapGrid
{
	static int GetSpacing(ATAK_MapViewState view)
	{
		if (100 * Math.Min(view.ScaleX(), view.ScaleY()) >= 40)
			return 100;
		return 1000;
	}

	static float NormalizeCoordinate(float coordinate, int cellSize)
	{
		// Quantize before wrapping so tiny negative values stay in the correct
		// cell. Match the native cursor's negative-coordinate 100 km wrap.
		coordinate = Math.Floor(coordinate / cellSize) * cellSize;
		if (coordinate < 0)
			coordinate -= Math.Floor(coordinate / 100000) * 100000;
		return coordinate;
	}

	static string PadCoordinate(int coordinate, int digits)
	{
		string text = coordinate.ToString();
		while (text.Length() < digits)
			text = "0" + text;
		return text;
	}

	static string FormatPosition(vector position)
	{
		position[0] = ATAK_MapGrid.NormalizeCoordinate(position[0], 100);
		position[2] = ATAK_MapGrid.NormalizeCoordinate(position[2], 100);
		int gridX = 0;
		int gridZ = 0;
		SCR_MapEntity.GetGridPos(position, gridX, gridZ, 2, 4);
		return string.Format("%1 %2", ATAK_MapGrid.PadCoordinate(gridX, 3), ATAK_MapGrid.PadCoordinate(gridZ, 3));
	}

	static string FormatAxis(float coordinate, int spacing)
	{
		int resolution = 2;
		int digits = 3;
		if (spacing == 1000)
		{
			resolution = 3;
			digits = 2;
		}
		vector position = Vector(ATAK_MapGrid.NormalizeCoordinate(coordinate, spacing), 0, 0);
		int gridX = 0;
		int gridZ = 0;
		SCR_MapEntity.GetGridPos(position, gridX, gridZ, resolution, 4);
		return ATAK_MapGrid.PadCoordinate(gridX, digits);
	}
}

class ATAK_MapGridController
{
	protected Widget m_Parent;
	protected bool m_bCompact;
	protected CanvasWidget m_Canvas;
	protected TextWidget m_wPlayerGrid;
	protected ref array<ref CanvasWidgetCommand> m_Commands = {};
	protected ref array<ref LineDrawCommand> m_LinePool = {};
	protected ref array<ref ATAK_MapLabelWidget> m_Labels = {};
	protected SCR_MapEntity m_LastMap;
	protected vector m_vLastCenter;
	protected float m_fLastRadius;
	protected float m_fLastPixelScale;
	protected bool m_bDirty = true;
	protected string m_sLastReadout;
	protected static const int MAX_GRID_LINES = 128;

	bool Start(Widget parent, ATAK_MapViewState view, bool showReadout = true)
	{
		m_Parent = parent;
		m_bCompact = view.Compact;
		m_Canvas = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.CanvasWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, parent));
		if (!m_Canvas)
			return false;
		FrameSlot.SetAnchorMin(m_Canvas, 0, 0);
		FrameSlot.SetAnchorMax(m_Canvas, 1, 1);
		FrameSlot.SetOffsets(m_Canvas, 0, 0, 0, 0);
		m_Canvas.SetSizeInUnits(Vector(view.CanvasWidth, view.CanvasHeight, 0));
		m_Canvas.SetZoom(1);
		m_Canvas.SetOffsetPx(Vector(0, 0, 0));
		m_Canvas.SetIsColorInherited(false);
		m_Canvas.SetZOrder(-8);
		if (showReadout)
		{
			m_wPlayerGrid = CreateGridText(30 * view.CanvasHeight / 1024);
			if (!m_wPlayerGrid)
			{
				Destroy();
				return false;
			}
			FrameSlot.SetAlignment(m_wPlayerGrid, 0, 0);
			FrameSlot.SetPos(m_wPlayerGrid, view.CanvasWidth * 0.03, view.CanvasHeight - 100 * view.CanvasHeight / 1024);
			FrameSlot.SetSize(m_wPlayerGrid, view.CanvasWidth * 0.94, 84 * view.CanvasHeight / 1024);
			m_wPlayerGrid.SetZOrder(20);
		}
		Stop();
		return true;
	}

	protected TextWidget CreateGridText(int fontSize)
	{
		TextWidget textWidget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, m_Parent));
		if (!textWidget)
			return null;
		FrameSlot.SetAnchorMin(textWidget, 0, 0);
		FrameSlot.SetAnchorMax(textWidget, 0, 0);
		textWidget.SetFont("{D3E75BCBECEB273E}UI/Fonts/NotoSans/NotoSans-Enhanced-Regular-JP-KOR-SC-LAT.fnt");
		textWidget.SetExactFontSize(fontSize);
		textWidget.SetTextWrapping(false);
		textWidget.SetOutline(2, 0xFFF4F0DD);
		textWidget.SetColorInt(0xFF142B43);
		if (m_bCompact)
		{
			textWidget.SetColorInt(0xFFFFFFFF);
			textWidget.SetOutline(1, 0xFF07121B);
			textWidget.SetBold(true);
		}
		textWidget.SetIsColorInherited(false);
		textWidget.SetZOrder(4);
		return textWidget;
	}

	void Update(ATAK_MapData mapData, ATAK_MapViewState view, vector player)
	{
		if (!m_Canvas)
			return;
		int spacing = ATAK_MapGrid.GetSpacing(view);
		if (m_wPlayerGrid)
		{
			string scale = "100 m";
			if (spacing == 1000)
				scale = "1 km";
			string readout = string.Format("PLAYER GRID %1 (100 m)\nEAST / NORTH | LINES %2", ATAK_MapGrid.FormatPosition(player), scale);
			if (readout != m_sLastReadout)
			{
				m_wPlayerGrid.SetText(readout);
				m_sLastReadout = readout;
			}
			m_wPlayerGrid.SetVisible(true);
		}
		// Show before measuring: a hidden/new canvas may not have a usable scale
		// until layout runs. Leave it visible so the next update can finish setup.
		m_Canvas.SetVisible(true);
		float pixelScale = m_Canvas.PixelPerUnit();
		if (pixelScale <= 0)
			return;
		if (!m_bDirty && m_LastMap == mapData.Entity && m_vLastCenter == view.Center
			&& m_fLastRadius == view.Radius && m_fLastPixelScale == pixelScale)
			return;

		RenderGrid(mapData, view, spacing, pixelScale);
		m_LastMap = mapData.Entity;
		m_vLastCenter = view.Center;
		m_fLastRadius = view.Radius;
		m_fLastPixelScale = pixelScale;
		m_bDirty = false;
	}

	protected void AddGridLine(vector a, vector b, bool major, float pixelScale)
	{
		int index = m_Commands.Count();
		LineDrawCommand line;
		if (index < m_LinePool.Count())
			line = m_LinePool[index];
		else
		{
			line = new LineDrawCommand();
			line.m_Vertices = new array<float>();
			line.m_Vertices.Resize(4);
			m_LinePool.Insert(line);
		}
		line.m_bShouldEnclose = false;
		line.m_iColor = 0xAA20354A;
		line.m_fWidth = 2 * pixelScale;
		if (major)
		{
			line.m_iColor = 0xE620354A;
			line.m_fWidth = 3 * pixelScale;
		}
		line.m_fOutlineWidth = 0;
		vector pixelA = m_Canvas.PosToPixels(a);
		vector pixelB = m_Canvas.PosToPixels(b);
		line.m_Vertices[0] = pixelA[0];
		line.m_Vertices[1] = pixelA[1];
		line.m_Vertices[2] = pixelB[0];
		line.m_Vertices[3] = pixelB[1];
		m_Commands.Insert(line);
	}

	protected bool ShowAxisLabel(int index, string text, vector point, ATAK_MapViewState view)
	{
		ATAK_MapLabelWidget label;
		if (index < m_Labels.Count())
			label = m_Labels[index];
		else
		{
			int fontSize = 26 * view.CanvasHeight / 1024;
			if (view.Compact) fontSize = 14;
			TextWidget widget = CreateGridText(fontSize);
			if (!widget)
				return false;
			label = new ATAK_MapLabelWidget();
			label.m_wText = widget;
			FrameSlot.SetAlignment(widget, 0.5, 0.5);
			m_Labels.Insert(label);
		}
		if (label.Text != text)
		{
			FrameSlot.SetSize(label.m_wText, 512, 80);
			label.m_wText.SetText(text);
			float width, height;
			label.m_wText.GetTextSize(width, height);
			float minHeight = 28;
			if (view.Compact) minHeight = 14;
			FrameSlot.SetSize(label.m_wText, Math.Max(width, 32) + 6, Math.Max(height, minHeight) + 6);
			label.Text = text;
		}
		FrameSlot.SetPos(label.m_wText, point[0], point[1]);
		label.m_wText.SetVisible(true);
		return true;
	}

	protected void RenderGrid(ATAK_MapData mapData, ATAK_MapViewState view, int spacing, float pixelScale)
	{
		m_Commands.Clear();
		float minX = Math.Max(view.Center[0] - view.Radius, mapData.Offset[0]);
		float maxX = Math.Min(view.Center[0] + view.Radius, mapData.Offset[0] + mapData.Size[0]);
		float minZ = Math.Max(view.Center[2] - view.HalfHeight(), mapData.Offset[2]);
		float maxZ = Math.Min(view.Center[2] + view.HalfHeight(), mapData.Offset[2] + mapData.Size[2]);
		if (maxX <= minX || maxZ <= minZ)
		{
			for (int hidden = 0; hidden < m_Labels.Count(); hidden++)
				m_Labels[hidden].m_wText.SetVisible(false);
			m_Canvas.SetDrawCommands(m_Commands);
			return;
		}
		float unit = view.CanvasHeight / 1024;
		float eastLeft = 110 * unit;
		float eastRight = 40 * unit;
		float eastGap = 80 * unit;
		float eastBottom = 24 * unit;
		float northX = 46 * unit;
		float northTop = 30 * unit;
		float northBottom = 60 * unit;
		float northGap = 40 * unit;
		if (view.Compact)
		{
			// Fixed HUD spacing fits larger glyphs; thin labels, retain every line.
			eastLeft = 54;
			eastRight = 24;
			eastGap = 52;
			eastBottom = 14;
			northX = 26;
			northTop = 18;
			northBottom = 30;
			northGap = 28;
		}
		int labels;
		float lastEastingX = -1000000;
		float lastNorthingY = 1000000;
		int firstX = Math.Ceil(minX / spacing);
		int lastX = Math.Floor(maxX / spacing);
		for (int gx = firstX; gx <= lastX && m_Commands.Count() < MAX_GRID_LINES; gx++)
		{
			int worldX = gx * spacing;
			vector a = view.WorldToDisplay(Vector(worldX, 0, minZ));
			vector b = view.WorldToDisplay(Vector(worldX, 0, maxZ));
			AddGridLine(a, b, worldX % 1000 == 0, pixelScale);
			if (a[0] >= eastLeft && a[0] <= view.CanvasWidth - eastRight && a[0] - lastEastingX >= eastGap)
			{
				if (ShowAxisLabel(labels, ATAK_MapGrid.FormatAxis(worldX, spacing), Vector(a[0], view.CanvasHeight - ATAK_DeviceUI.FooterHeight(view) - eastBottom, 0), view))
				{
					labels++;
					lastEastingX = a[0];
				}
			}
		}
		int firstZ = Math.Ceil(minZ / spacing);
		int lastZ = Math.Floor(maxZ / spacing);
		for (int gz = firstZ; gz <= lastZ && m_Commands.Count() < MAX_GRID_LINES; gz++)
		{
			int worldZ = gz * spacing;
			vector a = view.WorldToDisplay(Vector(minX, 0, worldZ));
			vector b = view.WorldToDisplay(Vector(maxX, 0, worldZ));
			AddGridLine(a, b, worldZ % 1000 == 0, pixelScale);
			if (a[1] >= ATAK_DeviceUI.HeaderHeight(view) + northTop && a[1] <= view.CanvasHeight - ATAK_DeviceUI.FooterHeight(view) - northBottom && lastNorthingY - a[1] >= northGap)
			{
				if (ShowAxisLabel(labels, ATAK_MapGrid.FormatAxis(worldZ, spacing), Vector(northX, a[1], 0), view))
				{
					labels++;
					lastNorthingY = a[1];
				}
			}
		}
		for (int i = labels; i < m_Labels.Count(); i++)
			m_Labels[i].m_wText.SetVisible(false);
		// The canvas borrows this persistent array, just like the road/arrow layers.
		m_Canvas.SetDrawCommands(m_Commands);
	}

	void Stop()
	{
		m_bDirty = true;
		if (m_Canvas)
			m_Canvas.SetVisible(false);
		if (m_wPlayerGrid)
			m_wPlayerGrid.SetVisible(false);
		foreach (ATAK_MapLabelWidget label : m_Labels)
			label.m_wText.SetVisible(false);
	}

	void Destroy()
	{
		Stop();
		if (m_Canvas)
			m_Canvas.RemoveFromHierarchy();
		if (m_wPlayerGrid)
			m_wPlayerGrid.RemoveFromHierarchy();
		foreach (ATAK_MapLabelWidget label : m_Labels)
			label.m_wText.RemoveFromHierarchy();
		m_Canvas = null;
		m_wPlayerGrid = null;
		m_Parent = null;
		m_Labels.Clear();
		m_Commands.Clear();
		m_LinePool.Clear();
	}
}
