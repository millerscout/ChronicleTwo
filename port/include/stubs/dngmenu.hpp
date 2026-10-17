#pragma once
#include <dngmenu.hpp>
extern CDngFreeMap  *MenuDngMap asm("MenuMouse_DngMap");
extern CMenuTreeMap *CMenuTreePt asm("MenuMouse_Tree");
extern short         DngTreeMode asm("MenuMouse_TreeMode");

// The floor map's cull edges and passage bands; see header_fixups.UNIT_FIXUPS.
float PortDngMapVisibleRight();
float PortDngMapVisibleBottom();
void  mgPortPrimLineBands(mgCDrawPrim &prim, int lines);
// The floor information's state, which the monster list (port/src/floor_monsters.cpp) reads and changes.
extern u8    dngfloor_infoview asm("MenuMouse_FloorInfoView");
extern s8    DngAskMessageDrawFlag asm("MenuMouse_FloorAsk");
extern int   DngInfoDrawAlpha asm("MenuMouse_FloorDrawAlpha");
extern u8    GeoramaMateriaInfoDrawFlag asm("MenuMouse_FloorMateriaShown");
extern s8    GeoramaMateriaInfoDrawPage asm("MenuMouse_FloorMateriaPage");
extern short GeoramaMateriaNum asm("MenuMouse_FloorMateriaNum");
