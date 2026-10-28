// Phone configuration only. Shared gadget lifecycle is reusable by the tablet.
[ComponentEditorProps(category: "GameScripted/Gadgets", description: "ATAK phone screen toggle", insertable: true)]
class ATAK_PhoneGadgetComponentClass : ATAK_DeviceGadgetComponentClass
{
}

class ATAK_PhoneGadgetComponent : ATAK_DeviceGadgetComponent
{
	override protected void ConfigureDeviceDefaults()
	{
		super.ConfigureDeviceDefaults();
		// Retain the proven hand animation; a custom hand pose is a later milestone.
		m_eAnimVariable = EGadgetAnimVariable.SY183;
		if (m_sScreenSlot.IsEmpty()) m_sScreenSlot = "ATAK_Display_Material";
		if (m_sScreenOff.IsEmpty()) m_sScreenOff = "{A63CE15D19A99FC3}Assets/Props/Data/ATAK_Screen_Off.emat";
		if (m_sScreenOn.IsEmpty()) m_sScreenOn = "{3F4E127A062AF544}Assets/Props/Data/ATAK_Screen_On.emat";
		if (m_sScreenLive.IsEmpty()) m_sScreenLive = "{6D2913EB827C8BFF}Assets/Props/Data/ATAK_Screen_Live.emat";
		if (m_sDisplayLayout.IsEmpty()) m_sDisplayLayout = "{7213168B8D092797}UI/layouts/Gadgets/BallisticTable/ATAK_PhoneDisplay.layout";
		if (m_fDisplayAspect <= 0) m_fDisplayAspect = 0.461538;
	}
}
