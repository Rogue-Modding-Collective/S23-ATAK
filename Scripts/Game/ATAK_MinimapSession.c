// One client-local preference set and HUD, independent of physical device pages.
class ATAK_MinimapPreferences
{
	protected static ref ATAK_MinimapPreferences s_Instance;
	bool Enabled;
	int Corner = 1; // top-left, top-right, bottom-left, bottom-right
	int SizeIndex = 1;
	float Radius = 550;
	int Brightness = 100;

	static ATAK_MinimapPreferences Get()
	{
		if (!s_Instance) s_Instance = new ATAK_MinimapPreferences();
		return s_Instance;
	}

	float GetSize()
	{
		return 240 + SizeIndex * 80;
	}

	string CornerName()
	{
		array<string> names = {"TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT"};
		return names[Corner];
	}

	string SizeName()
	{
		array<string> names = {"SMALL", "MEDIUM", "LARGE"};
		return names[SizeIndex];
	}

	void Adjust(int setting, int direction)
	{
		if (setting == 3) Enabled = !Enabled;
		else if (setting == 4) Corner = (Corner + direction + 4) % 4;
		else if (setting == 5) SizeIndex = Math.ClampInt(SizeIndex + direction, 0, 2);
		else if (setting == 6)
		{
			float factor = 1.5;
			if (direction > 0) factor = 1 / factor;
			Radius = Math.Clamp(Radius * factor, 50, 5000);
		}
	}
}

// Compatible gadgets register themselves; no player prefab or vanilla override.
// References to engine components are weak. OnDelete removes the last timer/HUD.
class ATAK_MinimapSession
{
	protected static ref ATAK_MinimapSession s_Instance;
	protected ref array<ATAK_DeviceGadgetComponent> m_Devices = {};
	protected ref ATAK_ScreenDisplayController m_Display;
	protected IEntity m_Player;
	protected bool m_bMonitoring;
	protected bool m_bVisible;
	protected bool m_bPowered = true;
	protected float m_fSize;
	protected int m_iCorner = -1;
	protected float m_fSetupRetry;

	static void Register(ATAK_DeviceGadgetComponent device)
	{
		if (!s_Instance) s_Instance = new ATAK_MinimapSession();
		if (s_Instance.m_Devices.Find(device) == -1)
			s_Instance.m_Devices.Insert(device);
		if (!s_Instance.m_bMonitoring)
		{
			s_Instance.m_bMonitoring = true;
			GetGame().GetCallqueue().CallLater(s_Instance.Tick, 100, false);
		}
	}

	static void Unregister(ATAK_DeviceGadgetComponent device)
	{
		if (!s_Instance) return;
		s_Instance.m_Devices.RemoveItem(device);
		// Immediately handle deletion of the last carried device.
		s_Instance.Refresh();
	}

	static void SetPower(bool powered)
	{
		if (!s_Instance) return;
		s_Instance.m_bPowered = powered;
		s_Instance.Refresh();
	}

	protected void Refresh()
	{
		if (!GetGame()) return;
		GetGame().GetCallqueue().Remove(Tick);
		if (m_Devices.IsEmpty())
		{
			m_bMonitoring = false;
			DestroyDisplay();
			return;
		}
		Tick();
	}

	protected void Tick()
	{
		if (!GetGame() || System.IsConsoleApp()) return;
		bool visible = UpdateDisplay();
		int delay = 500;
		if (visible) delay = 100;
		// Only ownership/menu monitoring while hidden; no map queries/rendering.
		GetGame().GetCallqueue().CallLater(Tick, delay, false);
	}

	protected bool UpdateDisplay()
	{
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (m_Player != player)
		{
			DestroyDisplay();
			m_Player = player;
			m_bPowered = true; // A new controlled life starts with session power enabled.
		}
		bool carried;
		bool held;
		foreach (ATAK_DeviceGadgetComponent device : m_Devices)
		{
			if (!device || !device.GetOwner() || device.GetOwner().IsDeleted()) continue;
			if (!player || device.GetCharacterOwner() != player) continue;
			if (device.GetMode() == EGadgetMode.ON_GROUND) continue;
			carried = true;
			if (device.GetMode() == EGadgetMode.IN_HAND) held = true;
		}
		if (!carried)
		{
			DestroyDisplay();
			return false;
		}
		CharacterControllerComponent controller = player.GetCharacterController();
		MenuManager menus = GetGame().GetMenuManager();
		ATAK_MinimapPreferences preferences = ATAK_MinimapPreferences.Get();
		if (!preferences.Enabled || !m_bPowered || held || !controller || controller.IsDead()
			|| (menus && menus.IsAnyMenuOpen()))
		{
			if (m_bVisible && m_Display) m_Display.Stop();
			m_bVisible = false;
			return false;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		// Bound the HUD on low-resolution/high-DPI windows as well as full screen.
		float size = Math.Min(preferences.GetSize(), workspace.DPIUnscale(workspace.GetHeight()) * 0.45);
		size = Math.Min(size, workspace.DPIUnscale(workspace.GetWidth()) - 48);
		if (size <= 0) return false;
		if (m_Display && (m_fSize != size || m_iCorner != preferences.Corner))
			DestroyDisplay();
		if (!m_Display)
		{
			// Rate-limit a native widget setup failure rather than retrying at 10 Hz.
			m_fSetupRetry -= 0.5;
			if (m_fSetupRetry > 0) return false;
			m_Display = new ATAK_ScreenDisplayController();
			m_fSize = size;
			m_iCorner = preferences.Corner;
		}
		if (!m_Display.StartHUD(size, preferences.Corner))
		{
			DestroyDisplay();
			m_fSetupRetry = 5;
			Print("[ATAK] Could not create HUD minimap widgets; retrying in five seconds.");
			return false;
		}
		m_Display.UpdateHUD(preferences.Radius, preferences.Brightness, 0.1);
		m_bVisible = true;
		return true;
	}

	protected void DestroyDisplay()
	{
		if (m_Display) m_Display.Destroy();
		m_Display = null;
		m_bVisible = false;
		m_fSetupRetry = 0;
	}
}
