# S23 phone screen toggle — Workbench prototype

This is source for a first compile/play test, not a compiled or in-game-verified addon.
It uses your uploaded screen material GUIDs. Your original prefab and materials
are not replaced by this package.

## Install

1. Copy the included Scripts folder into your S23-ATAK addon root (beside Assets
   and Prefabs), giving Scripts/Game/ATAK/ATAK_PhoneGadgetComponent.c.
2. Reload/recompile game scripts in Workbench. Resolve any compile error before
   changing the prefab. Workbench will generate resource metadata where needed.
3. Open YOUR phone prefab in Prefab Edit Mode. Its GUID is C3958C367D545554;
   its current filename is Compass_SY183.et. Do not edit the vanilla parent.
4. Replace its inherited SCR_CompassComponent with ATAK_PhoneGadgetComponent.
   Use a component class replacement option if your Workbench exposes one;
   otherwise remove the inherited compass component on the child prefab and add
   ATAK_PhoneGadgetComponent. The final phone must have ONE gadget component,
   not both. Keep the rest of the components, including replication, inventory,
   mesh and physics. The new class still derives from SCR_CompassComponent.
5. Confirm the new component's Screen Slot is ATAK_Display_Material (taken from
   your earlier model screenshot). This is the XOB's ORIGINAL slot name, not the
   On/Off emat filename. The script prints the actual slots if it cannot match.
6. Confirm Screen Off and Screen On resolve to your two materials. Defaults are:
   - Off: {A63CE15D19A99FC3}Assets/Props/Data/ATAK_Screen_Off.emat
   - On:  {3F4E127A062AF544}Assets/Props/Data/ATAK_Screen_On.emat
7. Keep InventoryItemComponent hand rotation 90 0 0 and translation 0 0 0.05.
   Save the phone prefab, then spawn a fresh instance for testing.

The script selects SY183 and enables FROM_ACTION itself. No custom input file
is needed. It uses GadgetActivate, the game's existing activate-gadget action;
check your current key binding rather than assuming a particular physical key.

## Expected behavior and test

- Ground/inventory: off.
- Equip in hand: on, following the compass's existing activation-on-equip behavior.
- Press the activate-gadget binding: off; press again: on.
- Put away or drop: off. Equip again: on.
- Only the display slot changes; the body and lenses retain their materials.
- Test a normal compass too: it should be unaffected because this is a subclass,
  not a global modded override.

The implementation uses the vanilla gadget toggle RPC and the inherited compass
RplSave/RplLoad state. Multiplayer, late joining, dropping/picking up, and switching
owners still need testing on your installed game version. No battery, map or UI
is included.

If it fails to compile, copy the first compiler error and line number. If it
compiles but the screen does not change, check the console for [ATAK] slot names,
confirm the new component replaced the old one, and check the gadget binding.
If a material looks wrong, select its BCR/NMO textures again in Workbench:
your uploaded Off material has Assets/Props paths while On has Assets/Props/Data
paths for the same GUIDs. The package leaves those working files untouched.

## Implementation references

- https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/main/scripts/Game/Components/Gadgets/SCR_GadgetComponent.c
- https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/main/scripts/Game/Components/Gadgets/SCR_GadgetManagerComponent.c
- https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/main/scripts/Game/Components/Gadgets/SCR_CompassComponent.c
- https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceIEntity.html

Reference source reviewed on 2026-10-27. Current public source can differ from
the version of Workbench installed locally. No Enfusion compiler is available
in the environment where this package was prepared.
