// Shared phone/tablet lifecycle using the stock gadget manager and SY183-capable
// hand animation path. Physical screen configuration belongs to the device.
[ComponentEditorProps(category: "GameScripted/Gadgets", description: "ATAK device gadget base", insertable: false)]
class ATAK_DeviceGadgetComponentClass : SCR_GadgetComponentClass
{
}

class ATAK_DeviceGadgetComponent : SCR_GadgetComponent
{
	[Attribute("", UIWidgets.EditBox, "Original display material slot name inside the device model")]
	protected string m_sScreenSlot;

	[Attribute("", UIWidgets.ResourceNamePicker, "Screen off material", "emat")]
	protected ResourceName m_sScreenOff;

	[Attribute("", UIWidgets.ResourceNamePicker, "Screen on material", "emat")]
	protected ResourceName m_sScreenOn;

	[Attribute("", UIWidgets.ResourceNamePicker, "Live screen material", "emat")]
	protected ResourceName m_sScreenLive;

	[Attribute("", UIWidgets.ResourceNamePicker, "Device screen layout", "layout")]
	protected ResourceName m_sDisplayLayout;

	[Attribute("1024", UIWidgets.EditBox, "Render target width in layout units")]
	protected float m_fDisplayWidth;

	[Attribute("1024", UIWidgets.EditBox, "Render target height in layout units")]
	protected float m_fDisplayHeight;

	[Attribute("0", UIWidgets.EditBox, "Physical screen width / height; calibrate against the model display surface")]
	protected float m_fDisplayAspect;

	[Attribute("550", UIWidgets.EditBox, "Initial map half-width in metres")]
	protected float m_fMapInitialRadius;

	[Attribute("50", UIWidgets.EditBox, "Minimum map half-width in metres")]
	protected float m_fMapMinRadius;

	[Attribute("5000", UIWidgets.EditBox, "Maximum map half-width in metres")]
	protected float m_fMapMaxRadius;

	protected bool m_bReportedMissingSlot;
	protected bool m_bReportedDisplaySetupFailure;
	protected bool m_bDisplayUpdatesRunning;
	protected bool m_bInitialPowerPending;
	protected bool m_bInitialPower;
	protected EGadgetMode m_eInitialMode;
	protected ref ATAK_ScreenDisplayController m_DisplayController;
	protected ref ATAK_MapInputController m_MapInput;
	protected float m_fMapRenderElapsed;

	override void OnPostInit(IEntity owner)
	{
		ConfigureDeviceDefaults();
		if (m_fDisplayAspect <= 0) m_fDisplayAspect = 1;
		// Stock gadget activation/RPCs still drive manual screen power.
		m_eUseMask = SCR_EUseContext.FROM_ACTION;
		super.OnPostInit(owner);

		if (!SCR_Global.IsEditMode(owner) && !System.IsConsoleApp())
		{
			ATAK_MinimapSession.Register(this);
			GetGame().GetCallqueue().CallLater(UpdateDeviceDisplay, 0, false);
		}
	}

	override void ActivateAction()
	{
		if (GetMode() != EGadgetMode.IN_HAND)
			return;

		bool powered = !IsToggledOn();
		if (GetCharacterOwner() == SCR_PlayerController.GetLocalControlledEntity())
			ATAK_MinimapSession.SetPower(powered);
		ToggleActive(powered, SCR_EUseContext.FROM_ACTION);
	}

	override void OnToggleActive(bool state)
	{
		// ModeSwitch handles equip; an explicit power-on boots Home.
		if (state && !m_bActivated && m_DisplayController)
			m_DisplayController.BootHome();
		m_bActivated = state && GetMode() == EGadgetMode.IN_HAND;
		UpdateDeviceDisplay();
	}

	// Device subclasses provide defaults without hard-coding a phone into the base.
	protected void ConfigureDeviceDefaults()
	{
	}

	override EGadgetType GetType()
	{
		// Stock generic category: inventory/quickslots, no compass/map shortcut.
		return EGadgetType.NONE;
	}

	override bool CanBeRaised()
	{
		return true;
	}

	override bool IsUsingADSControls()
	{
		return true;
	}

	override protected void ModeSwitch(EGadgetMode mode, IEntity charOwner)
	{
		super.ModeSwitch(mode, charOwner);
		// Keep the established behavior: equip powers on and restores the last page.
		m_bActivated = mode == EGadgetMode.IN_HAND;
		// Initial manager setup can replay the loaded mode after RplLoad. Honor
		// its power snapshot once; later genuine equips still power on normally.
		if (m_bInitialPowerPending)
		{
			if (mode == m_eInitialMode && mode == EGadgetMode.IN_HAND)
				m_bActivated = m_bInitialPower;
			m_bInitialPowerPending = false;
		}
		UpdateDeviceDisplay();
	}

	override protected void ModeClear(EGadgetMode mode)
	{
		super.ModeClear(mode);
		if (mode != EGadgetMode.IN_HAND) return;
		m_bActivated = false;
		UpdateDeviceDisplay();
	}

	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer)) return false;
		writer.WriteBool(m_bActivated);
		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader)) return false;
		if (!reader.ReadBool(m_bActivated)) return false;
		m_bInitialPower = m_bActivated;
		m_eInitialMode = GetMode();
		m_bInitialPowerPending = true;
		UpdateDeviceDisplay();
		return true;
	}

	protected bool IsLocalDisplayActive()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		ChimeraCharacter character = GetCharacterOwner();
		if (!player || character != player || !m_bActivated || GetMode() != EGadgetMode.IN_HAND)
			return false;
		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
	}

	protected void UpdateDeviceDisplay()
	{
		IEntity owner = GetOwner();
		if (!owner || SCR_Global.IsEditMode(owner) || System.IsConsoleApp())
			return;

		bool shouldShowLiveScreen = IsLocalDisplayActive();
		if (shouldShowLiveScreen)
		{
			// Select the $rendertarget material before binding the RT to this entity.
			ApplyScreenMaterial(m_sScreenLive);

			if (!m_DisplayController)
			{
				m_DisplayController = new ATAK_ScreenDisplayController();
				m_DisplayController.ConfigureMap(m_fDisplayWidth, m_fDisplayHeight, m_fDisplayAspect,
					m_fMapInitialRadius, m_fMapMinRadius, m_fMapMaxRadius);
			}

			if (m_DisplayController.Start(owner, m_sDisplayLayout))
			{
				if (UpdateDeviceDisplayData())
				{
					m_DisplayController.UpdateMapDisplay();
					StartDisplayUpdates();
					m_bReportedDisplaySetupFailure = false;
					return;
				}

				m_DisplayController.Stop();
			}

			StopDisplayUpdates();
			if (!m_bReportedDisplaySetupFailure)
			{
				PrintFormat("[ATAK] Could not initialize device display/UI from layout %1.", m_sDisplayLayout);
				m_bReportedDisplaySetupFailure = true;
			}

			m_DisplayController.Stop();
			ApplyScreenMaterial(m_sScreenOn);
			return;
		}

		StopDisplayUpdates();
		if (m_DisplayController)
			m_DisplayController.Stop();

		ApplyScreenMaterial(m_sScreenOff);
	}

	protected void StartDisplayUpdates()
	{
		if (m_bDisplayUpdatesRunning)
			return;

		if (!m_MapInput)
			m_MapInput = new ATAK_MapInputController();
		m_MapInput.Start(m_DisplayController);
		m_fMapRenderElapsed = 0;
		ConnectToGadgetsSystem();
		GetGame().GetCallqueue().CallLater(UpdateDeviceDisplayData, 500, true);
		m_bDisplayUpdatesRunning = true;
	}

	protected void StopDisplayUpdates()
	{
		if (!m_bDisplayUpdatesRunning)
			return;

		if (m_MapInput)
			m_MapInput.Stop();
		DisconnectFromGadgetsSystem();
		if (GetGame())
			GetGame().GetCallqueue().Remove(UpdateDeviceDisplayData);

		m_bDisplayUpdatesRunning = false;
	}

	// The gadget scheduler also updates inactive inventory entities. Register only
	// while this local display runs; leave item activity/physics to inventory.
	override void Update(float timeSlice)
	{
		if (!m_bDisplayUpdatesRunning)
			return;
		if (!IsLocalDisplayActive())
		{
			UpdateDeviceDisplay();
			return;
		}

		if (m_MapInput)
			m_MapInput.Update(timeSlice);
		m_fMapRenderElapsed += timeSlice;
		if (m_fMapRenderElapsed >= 0.05)
		{
			m_DisplayController.UpdateMapDisplay(m_fMapRenderElapsed);
			m_fMapRenderElapsed = 0;
		}
	}

	protected bool UpdateDeviceDisplayData()
	{
		if (!m_DisplayController || !IsLocalDisplayActive())
		{
			UpdateDeviceDisplay();
			return false;
		}

		return UpdateDeviceClock();
	}

	protected bool UpdateDeviceClock()
	{
		if (!m_DisplayController)
			return false;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return false;

		TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
		if (!timeManager)
			return false;

		float timeOfDay = timeManager.GetTimeOfTheDay();
		int hours = timeOfDay;
		int minutes = (timeOfDay - hours) * 60;
		if (hours >= 24)
			hours = 0;
		if (minutes >= 60)
			minutes = 59;
		string clockText = string.Format("%1%2:%3%4", hours / 10, hours % 10, minutes / 10, minutes % 10);
		return m_DisplayController.SetClock(clockText);
	}

	protected void ApplyScreenMaterial(ResourceName material)
	{
		IEntity owner = GetOwner();
		if (!owner || SCR_Global.IsEditMode(owner) || System.IsConsoleApp())
			return;

		VObject mesh = owner.GetVObject();
		if (!mesh)
			return;

		string slots[256];
		int count = mesh.GetMaterials(slots);
		bool found;
		for (int i = 0; i < count; i++)
		{
			if (slots[i] == m_sScreenSlot)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			if (!m_bReportedMissingSlot)
			{
				PrintFormat("[ATAK] Screen slot '%1' not found. XOB slots follow:", m_sScreenSlot);
				for (int j = 0; j < count; j++)
					PrintFormat("[ATAK] Slot: %1", slots[j]);
				m_bReportedMissingSlot = true;
			}
			return;
		}

		owner.SetObject(mesh, string.Format("$remap '%1' '%2';", m_sScreenSlot, material));
	}

	override void OnDelete(IEntity owner)
	{
		InventoryItemComponent item = InventoryItemComponent.Cast(owner.FindComponent(InventoryItemComponent));
		if (item) item.m_OnParentSlotChangedInvoker.Remove(OnParentSlotChanged);
		ATAK_MinimapSession.Unregister(this);
		if (GetGame())
			GetGame().GetCallqueue().Remove(UpdateDeviceDisplay);

		StopDisplayUpdates();

		if (m_DisplayController)
			m_DisplayController.Destroy();

		super.OnDelete(owner);
	}
}
