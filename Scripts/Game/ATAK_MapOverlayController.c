class ATAK_MapLabelWidget
{
	TextWidget m_wText;
	string Text;
	int FontSize;
	vector Size;
}

// A presentation owned by one screen. Terrain sources and widget lifetimes are
// independent of the phone gadget so the tablet can use the same overlay.
class ATAK_MapOverlayController
{
	protected Widget m_Parent;
	protected bool m_bCompact;
	protected CanvasWidget m_Canvas;
	protected ref ATAK_MapOverlayData m_Data = new ATAK_MapOverlayData();
	protected ref array<ref CanvasWidgetCommand> m_Commands = {};
	protected ref array<ref LineDrawCommand> m_LinePool = {};
	protected ref array<ref ATAK_MapLabelWidget> m_Labels = {};
	protected ref array<vector> m_LabelMins = {};
	protected ref array<vector> m_LabelMaxs = {};
	protected vector m_vLastCenter;
	protected float m_fLastRadius;
	protected float m_fLastPixelScale;
	protected int m_iLastRevision = -1;
	protected bool m_bActive;
	protected bool m_bDrawLimit;
	protected static const int MAX_DRAW_SEGMENTS = 8192;
	protected static const int MAX_LABELS = 20;

	bool Start(Widget parent, ATAK_MapViewState view)
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
		m_Canvas.SetZOrder(-5);
		m_Canvas.SetIsColorInherited(false);
		Resume();
		return true;
	}

	void Resume()
	{
		if (m_bActive)
			return;
		m_bActive = true;
		m_iLastRevision = -1;
	}

	string GetStatus()
	{
		if (m_bDrawLimit)
			return "Road display detail limit";
		return m_Data.RoadStatus;
	}

	void Update(ATAK_MapData mapData, ATAK_MapViewState view, vector player, float timeSlice)
	{
		if (!m_Canvas)
			return;
		if (!m_bActive)
			Resume();
		m_Data.Update(mapData, view, timeSlice);
		// Home/off hides this canvas. Show it before measuring so native layout
		// can restore a usable scale; otherwise neither roads nor labels resume.
		m_Canvas.SetVisible(true);
		float pixelScale = m_Canvas.PixelPerUnit();
		if (pixelScale <= 0)
			return;
		if (m_iLastRevision == m_Data.m_iRevision && m_fLastRadius == view.Radius
			&& m_vLastCenter == view.Center && m_fLastPixelScale == pixelScale)
			return;

		RenderRoads(view, pixelScale);
		RenderLabels(view, player);
		m_iLastRevision = m_Data.m_iRevision;
		m_fLastRadius = view.Radius;
		m_vLastCenter = view.Center;
		m_fLastPixelScale = pixelScale;
	}

	protected LineDrawCommand NextLine(ATAK_MapRoad road, ATAK_MapViewState view, float pixelScale)
	{
		int index = m_Commands.Count();
		LineDrawCommand line;
		if (index < m_LinePool.Count())
			line = m_LinePool[index];
		else
		{
			line = new LineDrawCommand();
			line.m_Vertices = new array<float>();
			m_LinePool.Insert(line);
		}
		line.m_Vertices.Clear();
		line.m_bShouldEnclose = false;
		line.m_iColor = 0xFFFFD45A;
		if (road.Width < 4)
			line.m_iColor = 0xFFD6AE70;
		if (road.Width < 2)
			line.m_iColor = 0xFFC5BE9D;
		// Boost screen-space strokes by 50% for readability on the physical phone.
		line.m_fWidth = 1.5 * Math.Clamp(road.Width * view.ScaleY(), 2, 5) * pixelScale;
		line.m_fOutlineWidth = 0.75 * pixelScale;
		line.m_iOutlineColor = 0xFF5C4A2F;
		m_Commands.Insert(line);
		return line;
	}

	protected void AddPoint(LineDrawCommand line, vector display)
	{
		vector pixel = m_Canvas.PosToPixels(display);
		line.m_Vertices.Insert(pixel[0]);
		line.m_Vertices.Insert(pixel[1]);
	}

	protected void RenderRoads(ATAK_MapViewState view, float pixelScale)
	{
		m_Commands.Clear();
		m_bDrawLimit = false;
		int segments;
		// Main roads have priority if the visible geometry reaches the draw budget.
		for (int pass = 0; pass < 2; pass++)
		{
			foreach (ATAK_MapRoad road : m_Data.Roads)
			{
				if ((pass == 0 && road.Width < 4) || (pass == 1 && road.Width >= 4))
					continue;
				if ((road.Width < 2 && view.Radius > 600) || (road.Width < 4 && view.Radius > 2200))
					continue;
				if (road.MaxX < view.Center[0] - view.Radius || road.MinX > view.Center[0] + view.Radius
					|| road.MaxZ < view.Center[2] - view.HalfHeight() || road.MinZ > view.Center[2] + view.HalfHeight())
					continue;

				LineDrawCommand line = null;
				vector last;
				for (int i = 1; i < road.Points.Count(); i++)
				{
					vector a = view.WorldToDisplay(road.Points[i - 1]);
					vector b = view.WorldToDisplay(road.Points[i]);
					if (!ATAK_MapClip.Segment(a, b, view.CanvasWidth, view.CanvasHeight))
					{
						line = null;
						continue;
					}
					if (a == b)
						continue;
					if (!line || last != a)
					{
						line = NextLine(road, view, pixelScale);
						AddPoint(line, a);
					}
					AddPoint(line, b);
					last = b;
					segments++;
					if (segments >= MAX_DRAW_SEGMENTS)
						break;
				}
				if (segments >= MAX_DRAW_SEGMENTS)
					break;
			}
			if (segments >= MAX_DRAW_SEGMENTS)
				break;
		}
		m_bDrawLimit = segments >= MAX_DRAW_SEGMENTS;
		// Retain both arrays: CanvasWidget borrows the commands, rather than owns them.
		m_Canvas.SetDrawCommands(m_Commands);
	}

	protected ATAK_MapLabelWidget GetLabel(int index, string text, int fontSize)
	{
		ATAK_MapLabelWidget label;
		if (index < m_Labels.Count())
			label = m_Labels[index];
		else
		{
			TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING,
				new Color(1, 1, 1, 1), 0, m_Parent));
			if (!widget)
				return null;
			label = new ATAK_MapLabelWidget();
			label.m_wText = widget;
			FrameSlot.SetAnchorMin(widget, 0, 0);
			FrameSlot.SetAnchorMax(widget, 0, 0);
			FrameSlot.SetAlignment(widget, 0.5, 0.5);
			widget.SetFont("{D3E75BCBECEB273E}UI/Fonts/NotoSans/NotoSans-Enhanced-Regular-JP-KOR-SC-LAT.fnt");
			widget.SetTextWrapping(false);
			widget.SetOutline(2, 0xFFF4F0DD);
			widget.SetColorInt(0xFF17212B);
			if (m_bCompact)
			{
				widget.SetColorInt(0xFFFFFFFF);
				widget.SetOutline(1, 0xFF07121B);
				widget.SetBold(true);
			}
			widget.SetIsColorInherited(false);
			widget.SetZOrder(-2);
			m_Labels.Insert(label);
		}
		if (label.Text != text || label.FontSize != fontSize)
		{
			FrameSlot.SetSize(label.m_wText, 1024, 128);
			label.m_wText.SetExactFontSize(fontSize);
			label.m_wText.SetText(text);
			float width, height;
			label.m_wText.GetTextSize(width, height);
			label.Size = Vector(Math.Max(32, width) + 8, Math.Max(fontSize, height) + 8, 0);
			FrameSlot.SetSize(label.m_wText, label.Size[0], label.Size[1]);
			label.Text = text;
			label.FontSize = fontSize;
		}
		return label;
	}

	protected bool Overlaps(vector min, vector max)
	{
		for (int i = 0; i < m_LabelMins.Count(); i++)
		{
			vector otherMin = m_LabelMins[i];
			vector otherMax = m_LabelMaxs[i];
			if (min[0] < otherMax[0] && max[0] > otherMin[0]
				&& min[1] < otherMax[1] && max[1] > otherMin[1])
				return true;
		}
		return false;
	}

	protected void RenderLabels(ATAK_MapViewState view, vector player)
	{
		m_LabelMins.Clear();
		m_LabelMaxs.Clear();
		// Reserve the existing status header and the own-position arrow.
		m_LabelMins.Insert(Vector(0, 0, 0));
		m_LabelMaxs.Insert(Vector(view.CanvasWidth, ATAK_DeviceUI.HeaderHeight(view), 0));
		// Leave the grid's northing labels and bottom easting/readout area clear.
		m_LabelMins.Insert(Vector(0, 0, 0));
		float axisGutter = 100 * view.CanvasHeight / 1024;
		if (view.Compact) axisGutter = 50;
		m_LabelMaxs.Insert(Vector(axisGutter, view.CanvasHeight, 0));
		m_LabelMins.Insert(Vector(0, view.CanvasHeight - ATAK_DeviceUI.FooterHeight(view) - 48 * view.CanvasHeight / 1024, 0));
		m_LabelMaxs.Insert(Vector(view.CanvasWidth, view.CanvasHeight, 0));
		vector marker = view.WorldToDisplay(player);
		float arrowY = 36 * view.CanvasHeight / 1024;
		float arrowX = arrowY * view.CanvasWidth / (view.CanvasHeight * view.DisplayAspect);
		m_LabelMins.Insert(Vector(marker[0] - arrowX, marker[1] - arrowY, 0));
		m_LabelMaxs.Insert(Vector(marker[0] + arrowX, marker[1] + arrowY, 0));
		int visible;
		foreach (ATAK_MapPlace place : m_Data.Places)
		{
			bool minor = place.Type == EMapDescriptorType.MDT_NAME_LOCAL
				|| place.Type == EMapDescriptorType.MDT_NAME_HILL || place.Type == EMapDescriptorType.MDT_NAME_GENERIC;
			if (minor && view.Radius > 1500)
				continue;
			if (place.Type == EMapDescriptorType.MDT_NAME_SETTLEMENT && view.Radius > 3500)
				continue;
			vector point = view.WorldToDisplay(place.Position);
			if (point[0] < 0 || point[0] > view.CanvasWidth || point[1] < 0 || point[1] > view.CanvasHeight)
				continue;
			int fontSize = 34 * view.CanvasHeight / 1024;
			if (minor)
				fontSize = 28 * view.CanvasHeight / 1024;
			// HUD font sizes are independent of map zoom and the physical screen.
			if (view.Compact)
			{
				fontSize = 16;
				if (minor) fontSize = 14;
			}
			ATAK_MapLabelWidget label = GetLabel(visible, place.Name, fontSize);
			if (!label)
				break;
			vector min = Vector(point[0] - label.Size[0] * 0.5 - 6, point[1] - label.Size[1] * 0.5 - 6, 0);
			vector max = Vector(point[0] + label.Size[0] * 0.5 + 6, point[1] + label.Size[1] * 0.5 + 6, 0);
			if (min[0] < 0 || min[1] < 0 || max[0] > view.CanvasWidth || max[1] > view.CanvasHeight || Overlaps(min, max))
				continue;
			FrameSlot.SetPos(label.m_wText, point[0], point[1]);
			label.m_wText.SetVisible(true);
			m_LabelMins.Insert(min);
			m_LabelMaxs.Insert(max);
			visible++;
			if (visible >= MAX_LABELS)
				break;
		}
		for (int i = visible; i < m_Labels.Count(); i++)
			m_Labels[i].m_wText.SetVisible(false);
	}

	void Stop()
	{
		m_bActive = false;
		if (m_Canvas)
			m_Canvas.SetVisible(false);
		foreach (ATAK_MapLabelWidget label : m_Labels)
			label.m_wText.SetVisible(false);
	}

	void Destroy()
	{
		Stop();
		if (m_Canvas)
			m_Canvas.RemoveFromHierarchy();
		foreach (ATAK_MapLabelWidget label : m_Labels)
			label.m_wText.RemoveFromHierarchy();
		m_Canvas = null;
		m_Parent = null;
		m_Labels.Clear();
		m_Commands.Clear();
		m_LinePool.Clear();
	}
}
