// Prototype: replace SCR_CompassComponent on the PHONE prefab only.
// Uses the existing gadget input/RPC path and compass save/load state.
[ComponentEditorProps(category: "GameScripted/Gadgets", description: "ATAK phone screen toggle", insertable: true)]
class ATAK_PhoneGadgetComponentClass : SCR_CompassComponentClass
{
}

class ATAK_PhoneGadgetComponent : SCR_CompassComponent
{
	[Attribute("ATAK_Display_Material", UIWidgets.EditBox, "Original material slot name inside ATAK_Phone.xob")]
	protected string m_sScreenSlot;

	[Attribute("{A63CE15D19A99FC3}Assets/Props/Data/ATAK_Screen_Off.emat", UIWidgets.ResourceNamePicker, "Screen off material", "emat")]
	protected ResourceName m_sScreenOff;

	[Attribute("{3F4E127A062AF544}Assets/Props/Data/ATAK_Screen_On.emat", UIWidgets.ResourceNamePicker, "Screen on material", "emat")]
	protected ResourceName m_sScreenOn;

	protected bool m_bReportedMissingSlot;

	override void OnPostInit(IEntity owner)
	{
		// Retain the existing SY183 hand animation and enable its activate input.
		m_eAnimVariable = EGadgetAnimVariable.SY183;
		m_eUseMask = SCR_EUseContext.FROM_ACTION;
		super.OnPostInit(owner);

		if (!SCR_Global.IsEditMode(owner) && !System.IsConsoleApp())
			GetGame().GetCallqueue().CallLater(ApplyScreenMaterial, 0, false);
	}

	override void ActivateAction()
	{
		if (GetMode() != EGadgetMode.IN_HAND)
			return;

		ToggleActive(!IsToggledOn(), SCR_EUseContext.FROM_ACTION);
	}

	override void OnToggleActive(bool state)
	{
		m_bActivated = state && GetMode() == EGadgetMode.IN_HAND;
		ApplyScreenMaterial();
	}

	// The compass calls this on equip, unequip, init and replicated state load.
	override protected void UpdateCompassState()
	{
		ApplyScreenMaterial();
	}

	protected void ApplyScreenMaterial()
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

		ResourceName material = m_sScreenOff;
		if (m_bActivated && GetMode() == EGadgetMode.IN_HAND)
			material = m_sScreenOn;

		owner.SetObject(mesh, string.Format("$remap '%1' '%2';", m_sScreenSlot, material));
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(ApplyScreenMaterial);
		super.OnDelete(owner);
	}
}
