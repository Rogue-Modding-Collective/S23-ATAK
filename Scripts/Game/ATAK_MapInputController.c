// Routes remappable controls to the current held-device page.
class ATAK_MapInputController
{
	protected ATAK_ScreenDisplayController m_Display;
	protected bool m_bListening;
	protected bool m_bContextActive;
	protected float m_fNavigationRepeat;
	protected int m_iLastEast;
	protected int m_iLastNorth;

	void Start(ATAK_ScreenDisplayController display)
	{
		if (m_bListening)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		m_Display = display;
		input.AddActionListener("ATAK_MapZoomIn", EActionTrigger.DOWN, ZoomIn);
		input.AddActionListener("ATAK_MapZoomOut", EActionTrigger.DOWN, ZoomOut);
		input.AddActionListener("ATAK_MapCenter", EActionTrigger.DOWN, Recenter);
		input.AddActionListener("ATAK_DeviceSelect", EActionTrigger.DOWN, Select);
		input.AddActionListener("ATAK_DeviceBack", EActionTrigger.DOWN, Back);
		m_bListening = true;
	}

	protected bool CanHandleInput()
	{
		if (!m_bListening || !m_Display || !m_Display.HasDisplay())
			return false;

		MenuManager menus = GetGame().GetMenuManager();
		if (menus && menus.IsAnyMenuOpen())
			return false;

		// Chat is a HUD edit box rather than necessarily a MenuManager menu.
		Widget focused = GetGame().GetWorkspace().GetFocusedWidget();
		while (focused)
		{
			if (focused.IsVisibleInHierarchy() && focused.IsEnabledInHierarchy())
			{
				if (EditBoxWidget.Cast(focused) || MultilineEditBoxWidget.Cast(focused))
					return false;
			}
			focused = focused.GetParent();
		}
		return true;
	}

	void Update(float timeSlice)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (!CanHandleInput())
		{
			// Clear held values when a menu takes over, even if a key is released
			// while this context is inactive.
			if (m_bContextActive)
				input.ResetContext("ATAK_DeviceContext");
			m_bContextActive = false;
			ResetNavigation();
			return;
		}

		if (!m_bContextActive)
			m_Display.RefreshBindings();
		input.ActivateContext("ATAK_DeviceContext");
		m_bContextActive = true;
		float east = input.GetActionValue("ATAK_MapRight") - input.GetActionValue("ATAK_MapLeft");
		float north = input.GetActionValue("ATAK_MapUp") - input.GetActionValue("ATAK_MapDown");
		if (m_Display.IsMapPage())
		{
			m_Display.Pan(east, north, timeSlice);
			ResetNavigation();
			return;
		}

		int uiEast = 0;
		int uiNorth = 0;
		if (east > 0.5) uiEast = 1;
		else if (east < -0.5) uiEast = -1;
		if (north > 0.5) uiNorth = 1;
		else if (north < -0.5) uiNorth = -1;
		// One immediate focus move, then a bounded repeat while held.
		m_fNavigationRepeat -= Math.Min(timeSlice, 0.1);
		if (uiEast != m_iLastEast || uiNorth != m_iLastNorth)
			m_fNavigationRepeat = 0;
		if ((uiEast != 0 || uiNorth != 0) && m_fNavigationRepeat <= 0)
		{
			m_Display.NavigateUI(uiEast, uiNorth);
			m_fNavigationRepeat = 0.22;
		}
		m_iLastEast = uiEast;
		m_iLastNorth = uiNorth;
	}

	protected void ZoomIn(float value, EActionTrigger reason)
	{
		if (CanHandleInput() && m_Display.IsMapPage())
			m_Display.StepZoom(true);
	}

	protected void ZoomOut(float value, EActionTrigger reason)
	{
		if (CanHandleInput() && m_Display.IsMapPage())
			m_Display.StepZoom(false);
	}

	protected void Recenter(float value, EActionTrigger reason)
	{
		if (CanHandleInput() && m_Display.IsMapPage())
			m_Display.Recenter();
	}

	protected void ResetNavigation()
	{
		m_fNavigationRepeat = 0;
		m_iLastEast = 0;
		m_iLastNorth = 0;
	}

	protected void Select(float value, EActionTrigger reason)
	{
		if (CanHandleInput())
			m_Display.SelectUI();
	}

	protected void Back(float value, EActionTrigger reason)
	{
		if (CanHandleInput())
			m_Display.BootHome();
	}

	void Stop()
	{
		if (m_bListening && GetGame())
		{
			InputManager input = GetGame().GetInputManager();
			if (input)
			{
				input.RemoveActionListener("ATAK_MapZoomIn", EActionTrigger.DOWN, ZoomIn);
				input.RemoveActionListener("ATAK_MapZoomOut", EActionTrigger.DOWN, ZoomOut);
				input.RemoveActionListener("ATAK_MapCenter", EActionTrigger.DOWN, Recenter);
				input.RemoveActionListener("ATAK_DeviceSelect", EActionTrigger.DOWN, Select);
				input.RemoveActionListener("ATAK_DeviceBack", EActionTrigger.DOWN, Back);
				input.ResetContext("ATAK_DeviceContext");
			}
		}
		m_bListening = false;
		m_bContextActive = false;
		ResetNavigation();
		m_Display = null;
	}
}
