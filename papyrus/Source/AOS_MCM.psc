Scriptname AOS_MCM extends MCM_ConfigBase

; Optional MCM Helper menu. Without SkyUI and MCM Helper this quest does nothing
; and prices come from the TOML files only.
; Both typed values are passed; the DLL uses the one that matches the setting id.

Event OnSettingChange(String a_ID)
	ArtisansOfSkyrim.ApplyMcmSetting(a_ID, GetModSettingFloat(a_ID), GetModSettingInt(a_ID))
EndEvent
