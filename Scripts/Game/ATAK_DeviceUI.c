enum ATAK_EDevicePage
{
	ATAK_HOME,
	ATAK_MAP,
	ATAK_SETTINGS,
	ATAK_SQUAD
}

// Session state belongs to this device/controller; no shared material mutation.
class ATAK_DeviceUIState
{
	ATAK_EDevicePage m_ePage = ATAK_EDevicePage.ATAK_HOME;
	int m_iAppSelection;
	int m_iSquadOffset;
	int m_iSquadCount;
	int m_iSettingsSelection;
	int m_iBrightnessPercent = 100;
	int m_iRevision;

	bool ShowsMap()
	{
		return m_ePage == ATAK_EDevicePage.ATAK_MAP || m_ePage == ATAK_EDevicePage.ATAK_SETTINGS;
	}

	void BootHome()
	{
		m_ePage = ATAK_EDevicePage.ATAK_HOME;
		m_iAppSelection = 0;
		m_iSettingsSelection = 0;
		m_iRevision++;
	}

	void Navigate(int east, int north)
	{
		if (m_ePage == ATAK_EDevicePage.ATAK_HOME)
		{
			int row = m_iAppSelection / 2;
			int column = m_iAppSelection % 2;
			row = Math.ClampInt(row - north, 0, 2);
			column = Math.ClampInt(column + east, 0, 1);
			int selected = row * 2 + column;
			if (selected != m_iAppSelection)
			{
				m_iAppSelection = selected;
				m_iRevision++;
			}
		}
		else if (m_ePage == ATAK_EDevicePage.ATAK_SQUAD)
		{
			m_iSquadOffset = Math.ClampInt(m_iSquadOffset - north + east * 8, 0, Math.Max(0, m_iSquadCount - 8));
			m_iRevision++;
		}
		else if (m_ePage == ATAK_EDevicePage.ATAK_SETTINGS)
		{
			int setting = Math.ClampInt(m_iSettingsSelection - north, 0, 6);
			if (setting != m_iSettingsSelection)
			{
				m_iSettingsSelection = setting;
				m_iRevision++;
			}
			if (east != 0)
			{
				if (m_iSettingsSelection == 0) AdjustBrightness(east * 10);
				else if (m_iSettingsSelection >= 3)
				{
					ATAK_MinimapPreferences.Get().Adjust(m_iSettingsSelection, east);
					m_iRevision++;
				}
			}
		}
	}

	void AdjustBrightness(int amount)
	{
		int value = Math.ClampInt(m_iBrightnessPercent + amount, 40, 100);
		if (value == m_iBrightnessPercent)
			return;
		m_iBrightnessPercent = value;
		ATAK_MinimapPreferences.Get().Brightness = value;
		m_iRevision++;
	}

	void Select()
	{
		if (m_ePage == ATAK_EDevicePage.ATAK_HOME)
		{
			if (m_iAppSelection == 0)
				m_ePage = ATAK_EDevicePage.ATAK_MAP;
			else if (m_iAppSelection == 1)
				m_ePage = ATAK_EDevicePage.ATAK_SQUAD;
			else if (m_iAppSelection == 5)
				m_ePage = ATAK_EDevicePage.ATAK_SETTINGS;
			else
				return; // The remaining apps are visibly unavailable.
			m_iRevision++;
		}
		else if (m_ePage == ATAK_EDevicePage.ATAK_SETTINGS)
		{
			if (m_iSettingsSelection >= 3)
			{
				ATAK_MinimapPreferences.Get().Adjust(m_iSettingsSelection, 1);
				m_iRevision++;
			}
			else if (m_iSettingsSelection == 1)
				AdjustBrightness(100);
			else if (m_iSettingsSelection == 2)
			{
				m_ePage = ATAK_EDevicePage.ATAK_MAP;
				m_iRevision++;
			}
		}
	}
}

// Draws only the phone/tablet UI. Map data/rendering remain in their controllers.
class ATAK_DeviceUI
{
	protected Widget m_Parent;
	protected CanvasWidget m_Canvas;
	protected ATAK_MapViewState m_View;
	protected ref array<ref CanvasWidgetCommand> m_Commands = {};
	protected ref array<ref PolygonDrawCommand> m_RectPool = {};
	protected ref array<TextWidget> m_TextWidgets = {};
	protected ref array<TextWidget> m_AppTitles = {};
	protected ref array<TextWidget> m_AppDetails = {};
	protected ref array<TextWidget> m_SettingsRows = {};
	protected ref array<TextWidget> m_SquadNames = {};
	protected ref array<TextWidget> m_SquadBadges = {};
	protected ATAK_SquadData m_Squad;
	protected TextWidget m_wSquadIdentity;
	protected TextWidget m_wTitle;
	protected TextWidget m_wClock;
	protected TextWidget m_wSubtitle;
	protected TextWidget m_wFooterMain;
	protected TextWidget m_wFooterInfo;
	protected TextWidget m_wHint;
	protected TextWidget m_wBackHint;
	protected TextWidget m_wSettingsNote;
	protected TextWidget m_wPreview;
	protected int m_iRenderedRevision = -1;
	protected float m_fLastPixelScale;
	protected bool m_bDirty = true;
	protected string m_sClock = "--:--";
	protected string m_sMapSubtitle;
	protected string m_sMapGrid;
	protected string m_sMapInfo;

	static float HeaderHeight(ATAK_MapViewState view)
	{
		if (view.Compact) return 52;
		return 168 * view.CanvasHeight / 1024;
	}

	static float FooterHeight(ATAK_MapViewState view)
	{
		if (view.Compact) return 32;
		return 144 * view.CanvasHeight / 1024;
	}

	bool Start(Widget parent, ATAK_MapViewState view)
	{
		m_Parent = parent;
		m_View = view;
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
		m_Canvas.SetZOrder(30);
		m_wTitle = CreateUIText(48, 18, 580, 60, 44);
		m_wClock = CreateUIText(794, 24, 190, 52, 36);
		m_wSubtitle = CreateUIText(48, 82, 928, 36, 25);
		m_wSquadIdentity = CreateUIText(48, 124, 928, 30, 23);
		for (int squadRow = 0; squadRow < 8; squadRow++)
		{
			m_SquadNames.Insert(CreateUIText(72, 208 + squadRow * 80, 608, 48, 34));
			m_SquadBadges.Insert(CreateUIText(696, 214 + squadRow * 80, 264, 40, 24));
		}
		m_wFooterMain = CreateUIText(48, 894, 928, 36, 30);
		m_wFooterInfo = CreateUIText(48, 930, 928, 28, 23);
		m_wHint = CreateUIText(48, 959, 928, 26, 22);
		m_wBackHint = CreateUIText(48, 989, 928, 26, 22);
		for (int i = 0; i < 6; i++)
		{
			int row = i / 2;
			int column = i % 2;
			m_AppTitles.Insert(CreateUIText(72 + column * 476, 206 + row * 208, 400, 64, 44));
			m_AppDetails.Insert(CreateUIText(72 + column * 476, 278 + row * 208, 400, 48, 24));
		}
		for (int setting = 0; setting < 7; setting++)
			m_SettingsRows.Insert(CreateUIText(72, 184 + setting * 76, 880, 52, 30));
		m_wSettingsNote = CreateUIText(64, 744, 896, 60, 23);
		m_wPreview = CreateUIText(64, 816, 896, 40, 25);
		// Fail cleanly if native widget creation failed partway through setup.
		if (m_TextWidgets.Count() != 45)
		{
			Destroy();
			return false;
		}
		m_wTitle.SetBold(true);
		m_wPreview.SetOutline(2, 0xFF07121B);
		Stop();
		return true;
	}

	protected TextWidget CreateUIText(float x, float y, float width, float height, int fontSize)
	{
		TextWidget textWidget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.BLEND | WidgetFlags.INHERIT_CLIPPING,
			new Color(1, 1, 1, 1), 0, m_Parent));
		if (!textWidget)
			return null;
		FrameSlot.SetAnchorMin(textWidget, 0, 0);
		FrameSlot.SetAnchorMax(textWidget, 0, 0);
		FrameSlot.SetAlignment(textWidget, 0, 0);
		FrameSlot.SetPos(textWidget, x * m_View.CanvasWidth / 1024, y * m_View.CanvasHeight / 1024);
		FrameSlot.SetSize(textWidget, width * m_View.CanvasWidth / 1024, height * m_View.CanvasHeight / 1024);
		textWidget.SetFont("{D3E75BCBECEB273E}UI/Fonts/NotoSans/NotoSans-Enhanced-Regular-JP-KOR-SC-LAT.fnt");
		textWidget.SetExactFontSize(fontSize * m_View.CanvasHeight / 1024);
		textWidget.SetTextWrapping(false);
		textWidget.SetIsColorInherited(false);
		textWidget.SetColorInt(0xFFEAF3F9);
		textWidget.SetZOrder(31);
		m_TextWidgets.Insert(textWidget);
		return textWidget;
	}

	void Invalidate()
	{
		m_bDirty = true;
	}

	void SetClock(string clockText)
	{
		m_sClock = clockText;
		if (m_wClock)
			m_wClock.SetText(clockText);
	}

	protected string KeyName(string action, string preset, string fallback)
	{
		InputManager input = GetGame().GetInputManager();
		array<string> keys = {};
		array<BaseContainer> filters = {};
		if (!input || !input.GetActionKeybinding(action, keys, filters, EInputDeviceType.KEYBOARD, preset))
			return fallback;
		string result;
		foreach (string key : keys)
		{
			key.Replace("keyboard:", "");
			key.Replace("KC_", "");
			if (key == "RETURN") key = "ENTER";
			if (key == "BACK") key = "BACKSPACE";
			if (key == "EQUALS") key = "+";
			if (key == "MINUS") key = "-";
			if (key == "ADD") key = "NUM+";
			if (key == "SUBTRACT") key = "NUM-";
			if (!result.IsEmpty()) result += "+";
			result += key;
		}
		if (result.IsEmpty())
			return fallback;
		return result;
	}

	protected void AddRect(float x, float y, float width, float height, int color)
	{
		int index = m_Commands.Count();
		PolygonDrawCommand polygon;
		if (index < m_RectPool.Count())
			polygon = m_RectPool[index];
		else
		{
			polygon = new PolygonDrawCommand();
			polygon.m_Vertices = new array<float>();
			polygon.m_Vertices.Resize(8);
			m_RectPool.Insert(polygon);
		}
		polygon.m_iColor = color;
		for (int i = 0; i < 4; i++)
		{
			float px = x;
			float py = y;
			if (i == 1 || i == 2) px += width;
			if (i >= 2) py += height;
			vector pixel = m_Canvas.PosToPixels(Vector(px * m_View.CanvasWidth / 1024, py * m_View.CanvasHeight / 1024, 0));
			polygon.m_Vertices[i * 2] = pixel[0];
			polygon.m_Vertices[i * 2 + 1] = pixel[1];
		}
		m_Commands.Insert(polygon);
	}

	void SetSquad(ATAK_SquadData squad)
	{
		m_Squad = squad;
		Invalidate();
	}

	// Measure with the native font; reserve a separate column for badges.
	static void FitText(TextWidget label, string value, float width)
	{
		label.SetText(value);
		float measured, height;
		label.GetTextSize(measured, height);
		int length = value.Length();
		while (measured > width && length > 0)
		{
			length--;
			label.SetText(value.Substring(0, length) + "...");
			label.GetTextSize(measured, height);
		}
	}

	void Draw(ATAK_DeviceUIState state)
	{
		if (!m_Canvas)
			return;
		m_Canvas.SetVisible(true);
		float pixelScale = m_Canvas.PixelPerUnit();
		if (pixelScale <= 0)
			return;
		if (!m_bDirty && m_iRenderedRevision == state.m_iRevision && m_fLastPixelScale == pixelScale)
			return;
		foreach (TextWidget textWidget : m_TextWidgets)
			textWidget.SetVisible(false);
		m_Commands.Clear();
		if (state.m_ePage == ATAK_EDevicePage.ATAK_HOME || state.m_ePage == ATAK_EDevicePage.ATAK_SQUAD)
			AddRect(0, 0, 1024, 1024, 0xFF0B121A);
		AddRect(0, 0, 1024, 168, 0xF20B121A);
		AddRect(0, 880, 1024, 144, 0xF20B121A);
		m_wTitle.SetText("ATAK");
		m_wClock.SetText(m_sClock);
		m_wTitle.SetVisible(true);
		m_wClock.SetVisible(true);
		m_wSubtitle.SetVisible(true);
		m_wSquadIdentity.SetVisible(true);
		string identity = "Squad information unavailable";
		if (m_Squad) identity = m_Squad.Identity();
		FitText(m_wSquadIdentity, identity, 928 * m_View.CanvasWidth / 1024);
		m_wFooterMain.SetVisible(true);
		m_wFooterInfo.SetVisible(true);
		m_wHint.SetVisible(true);
		m_wBackHint.SetVisible(true);
		string select = KeyName("ATAK_DeviceSelect", "click", "ENTER");
		string back = KeyName("ATAK_DeviceBack", "click", "BACKSPACE");
		string up = KeyName("ATAK_MapUp", "pressed", "UP");
		string down = KeyName("ATAK_MapDown", "pressed", "DOWN");
		string left = KeyName("ATAK_MapLeft", "pressed", "LEFT");
		string right = KeyName("ATAK_MapRight", "pressed", "RIGHT");
		m_wFooterMain.SetText(string.Empty);
		m_wFooterInfo.SetText(string.Empty);
		m_wHint.SetText(string.Empty);
		m_wBackHint.SetText(string.Empty);
		if (state.m_ePage == ATAK_EDevicePage.ATAK_HOME)
		{
			m_wSubtitle.SetText("HOME");
			array<string> titles = {"MAP", "SQUAD", "MARKERS", "ROUTES", "MESSAGES", "SETTINGS"};
			for (int i = 0; i < 6; i++)
			{
				int row = i / 2;
				int column = i % 2;
				bool focused = i == state.m_iAppSelection;
				int border = 0xFF223342;
				if (focused) border = 0xFF37C5F3;
				AddRect(48 + column * 476, 172 + row * 208, 452, 184, border);
				AddRect(52 + column * 476, 176 + row * 208, 444, 176, 0xFF12212E);
				m_AppTitles[i].SetText(titles[i]);
				m_AppTitles[i].SetColorInt(0xFF748899);
				m_AppDetails[i].SetText("Coming later");
				if (i == 0 || i == 1 || i == 5)
				{
					m_AppTitles[i].SetColorInt(0xFFEAF3F9);
					m_AppDetails[i].SetText("Display controls");
					if (i == 0) m_AppDetails[i].SetText("Live terrain map");
					if (i == 1) m_AppDetails[i].SetText("Own-squad roster");
				}
				m_AppTitles[i].SetVisible(true);
				m_AppDetails[i].SetVisible(true);
			}
			m_wFooterMain.SetText(string.Format("%1/%2/%3/%4  SELECT", up, down, left, right));
			m_wFooterInfo.SetText(string.Format("%1  OPEN APP", select));
			m_wHint.SetText(string.Format("%1  SCREEN POWER", KeyName("GadgetActivate", "click", "USE ITEM")));
		}
		else if (state.m_ePage == ATAK_EDevicePage.ATAK_SQUAD)
		{
			m_wSubtitle.SetText("SQUAD | PLAYER ROSTER");
			if (m_Squad && m_Squad.m_eStatus == ATAK_ESquadStatus.READY)
			{
				for (int rosterRow = 0; rosterRow < 8; rosterRow++)
				{
					int memberIndex = state.m_iSquadOffset + rosterRow;
					if (memberIndex >= m_Squad.m_Members.Count()) break;
					ATAK_SquadMember member = m_Squad.m_Members[memberIndex];
					AddRect(48, 196 + rosterRow * 80, 928, 72, 0xFF12212E);
					FitText(m_SquadNames[rosterRow], member.m_sDisplayName, 608 * m_View.CanvasWidth / 1024);
					string badges = string.Empty;
					if (member.m_bLeader) badges = "LEADER";
					if (member.m_bLocal) badges += " YOU";
					m_SquadBadges[rosterRow].SetText(badges);
					m_SquadNames[rosterRow].SetVisible(true);
					m_SquadBadges[rosterRow].SetVisible(true);
				}
				int first = 0;
				if (state.m_iSquadCount > 0) first = state.m_iSquadOffset + 1;
				m_wFooterMain.SetText(string.Format("MEMBERS %1 | %2-%3", state.m_iSquadCount, first, Math.Min(state.m_iSquadOffset + 8, state.m_iSquadCount)));
				bool leaderPresent;
				foreach (ATAK_SquadMember squadMember : m_Squad.m_Members)
					if (squadMember.m_bLeader) leaderPresent = true;
				if (!leaderPresent) m_wFooterInfo.SetText("Leader unavailable");
			}
			else
				m_wFooterMain.SetText(identity);
			m_wHint.SetText(string.Format("%1/%2 SCROLL | %3/%4 PAGE", up, down, left, right));
			m_wBackHint.SetText(string.Format("%1 HOME", back));
		}
		else if (state.m_ePage == ATAK_EDevicePage.ATAK_SETTINGS)
		{
			m_wSubtitle.SetText("SETTINGS | DISPLAY");
			for (int j = 0; j < 7; j++)
			{
				float top = 172 + j * 76;
				float height = 72;
				int color = 0xEB0B121A;
				if (j == state.m_iSettingsSelection) color = 0xFF216184;
				AddRect(48, top, 928, height, color);
				m_SettingsRows[j].SetVisible(true);
			}
			m_SettingsRows[0].SetText(string.Format("MAP BRIGHTNESS   %1", state.m_iBrightnessPercent) + "%");
			m_SettingsRows[1].SetText("RESET DISPLAY");
			m_SettingsRows[2].SetText("OPEN MAP");
			ATAK_MinimapPreferences preferences = ATAK_MinimapPreferences.Get();
			string minimapState = "OFF";
			if (preferences.Enabled) minimapState = "ON";
			m_SettingsRows[3].SetText("HUD MINIMAP   " + minimapState);
			m_SettingsRows[4].SetText("HUD CORNER   " + preferences.CornerName());
			m_SettingsRows[5].SetText("HUD SIZE   " + preferences.SizeName());
			int hudRadius = preferences.Radius;
			m_SettingsRows[6].SetText(string.Format("HUD ZOOM   +/- %1 m", hudRadius));
			m_wSettingsNote.SetText("HUD follows while stowed. Power-off also hides HUD.");
			m_wSettingsNote.SetOutline(2, 0xFF07121B);
			m_wSettingsNote.SetVisible(true);
			m_wPreview.SetText("LIVE MAP PREVIEW");
			m_wPreview.SetVisible(true);
			m_wFooterMain.SetText(string.Format("%1 / %2  SELECT", up, down));
			m_wFooterInfo.SetText(string.Format("%1 / %2  ADJUST VALUE", left, right));
			m_wHint.SetText(string.Format("%1  CONFIRM", select));
			m_wBackHint.SetText(string.Format("%1  HOME", back));
		}
		else
		{
			m_wSubtitle.SetText(m_sMapSubtitle);
			m_wFooterMain.SetText(m_sMapGrid);
			m_wFooterInfo.SetText(m_sMapInfo);
			m_wHint.SetText(string.Format("PAN %1/%2/%3/%4 | ZOOM %5/%6", up, down, left, right,
				KeyName("ATAK_MapZoomIn", "click", "+"), KeyName("ATAK_MapZoomOut", "click", "-")));
			m_wBackHint.SetText(string.Format("%1  FOLLOW | %2  HOME", KeyName("ATAK_MapCenter", "click", "HOME"), back));
		}
		m_Canvas.SetDrawCommands(m_Commands);
		m_iRenderedRevision = state.m_iRevision;
		m_fLastPixelScale = pixelScale;
		m_bDirty = false;
	}

	// Avoid re-laying out native text widgets when rounded values are unchanged.
	protected void SetTextIfChanged(TextWidget textWidget, string value)
	{
		if (textWidget.GetText() != value)
			textWidget.SetText(value);
	}

	void SetMapInfo(ATAK_DeviceUIState state, vector player, int heading, string mode, int halfWidth, int spacing, string warning)
	{
		m_sMapSubtitle = string.Format("MAP | %1 | N UP | HDG %2", mode, heading);
		m_sMapGrid = string.Format("GRID %1 (100 m)", ATAK_MapGrid.FormatPosition(player));
		m_sMapInfo = string.Format("LINES %1 m | VIEW +/- %2 m", spacing, halfWidth);
		if (!warning.IsEmpty()) m_sMapInfo = warning;
		if (state.m_ePage == ATAK_EDevicePage.ATAK_MAP)
		{
			SetTextIfChanged(m_wSubtitle, m_sMapSubtitle);
			SetTextIfChanged(m_wFooterMain, m_sMapGrid);
			SetTextIfChanged(m_wFooterInfo, m_sMapInfo);
		}
		else if (state.m_ePage == ATAK_EDevicePage.ATAK_SETTINGS)
			SetTextIfChanged(m_wPreview, "LIVE MAP PREVIEW");
	}

	void SetMapUnavailable(ATAK_DeviceUIState state, string reason)
	{
		m_sMapSubtitle = "MAP | UNAVAILABLE";
		m_sMapGrid = "MAP UNAVAILABLE";
		m_sMapInfo = reason;
		if (state.m_ePage == ATAK_EDevicePage.ATAK_MAP)
		{
			SetTextIfChanged(m_wSubtitle, m_sMapSubtitle);
			SetTextIfChanged(m_wFooterMain, m_sMapGrid);
			SetTextIfChanged(m_wFooterInfo, reason);
		}
		else if (state.m_ePage == ATAK_EDevicePage.ATAK_SETTINGS)
			SetTextIfChanged(m_wPreview, "MAP PREVIEW UNAVAILABLE");
	}

	void Stop()
	{
		m_bDirty = true;
		if (m_Canvas) m_Canvas.SetVisible(false);
		foreach (TextWidget textWidget : m_TextWidgets) textWidget.SetVisible(false);
	}

	void Destroy()
	{
		Stop();
		if (m_Canvas) m_Canvas.RemoveFromHierarchy();
		foreach (TextWidget textWidget : m_TextWidgets) textWidget.RemoveFromHierarchy();
		m_TextWidgets.Clear();
		m_AppTitles.Clear();
		m_AppDetails.Clear();
		m_SettingsRows.Clear();
		m_SquadNames.Clear();
		m_SquadBadges.Clear();
		m_Squad = null;
		m_Commands.Clear();
		m_RectPool.Clear();
		m_Canvas = null;
		m_Parent = null;
	}
}
