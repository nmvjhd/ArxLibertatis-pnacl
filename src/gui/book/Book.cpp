/*
 * Copyright 2015 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/book/Book.h"

#include <iomanip>

#include "animation/AnimationRender.h"
#include "core/Application.h"
#include "core/Core.h"
#include "core/GameTime.h"
#include "core/Localisation.h"
#include "game/EntityManager.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Levels.h"
#include "game/Player.h"
#include "graphics/Draw.h"
#include "graphics/Renderer.h"
#include "graphics/effects/Halo.h"
#include "graphics/particle/ParticleEffects.h"
#include "graphics/texture/TextureStage.h"
#include "gui/Menu.h"
#include "gui/MiniMap.h"
#include "gui/Speech.h"
#include "gui/TextManager.h"
#include "scene/GameSound.h"
#include "scene/Interactive.h"
#include "script/Script.h"

ARX_INTERFACE_BOOK_MODE Book_Mode = BOOKMODE_STATS;

long Book_MapPage = 0;
long Book_SpellPage = 0;

long BOOKZOOM = 0;
long IN_BOOK_DRAW = 0;

long FLYING_OVER = 0;
long OLD_FLYING_OVER = 0;

//used to redist points - attributes and skill
long lCursorRedistValue = 0;

static void onBookClosePage() {
	
	if(Book_Mode == BOOKMODE_SPELLS) {
		// Closing spell page - clean up any rune flares
		ARX_SPELLS_ClearAllSymbolDraw();
	}
	
}

static bool canOpenBookPage(ARX_INTERFACE_BOOK_MODE page) {
	switch(page) {
		case BOOKMODE_SPELLS:  return !!player.rune_flags;
		default:               return true;
	}
}

void openBookPage(ARX_INTERFACE_BOOK_MODE newPage, bool toggle) {
	
	if((player.Interface & INTER_MAP) && Book_Mode == newPage) {
		
		if(toggle) {
			// Close the book
			ARX_INTERFACE_BookClose();
		}
		
		return; // nothing to do
	}
	
	if(!canOpenBookPage(newPage)) {
		return;
	}
	
	if(player.Interface & INTER_MAP) {
		
		onBookClosePage();
		
		// If the book is already open, play the page turn sound
		ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
		
	} else {
		// Otherwise open the book
		ARX_INTERFACE_BookToggle();
	}
	
	Book_Mode = newPage;
}

ARX_INTERFACE_BOOK_MODE nextBookPage() {
	ARX_INTERFACE_BOOK_MODE nextPage = Book_Mode, oldPage;
	do {
		oldPage = nextPage;
		
		switch(oldPage) {
			case BOOKMODE_STATS:   nextPage = BOOKMODE_SPELLS;  break;
			case BOOKMODE_SPELLS:  nextPage = BOOKMODE_MINIMAP; break;
			case BOOKMODE_MINIMAP: nextPage = BOOKMODE_QUESTS;  break;
			case BOOKMODE_QUESTS:  nextPage = BOOKMODE_QUESTS;  break;
		}
		
		if(canOpenBookPage(nextPage)) {
			return nextPage;
		}
		
	} while(nextPage != oldPage);
	return Book_Mode;
}

ARX_INTERFACE_BOOK_MODE prevBookPage() {
	ARX_INTERFACE_BOOK_MODE prevPage = Book_Mode, oldPage;
	do {
		oldPage = prevPage;
		
		switch(oldPage) {
			case BOOKMODE_STATS:   prevPage = BOOKMODE_STATS;   break;
			case BOOKMODE_SPELLS:  prevPage = BOOKMODE_STATS;   break;
			case BOOKMODE_MINIMAP: prevPage = BOOKMODE_SPELLS;  break;
			case BOOKMODE_QUESTS:  prevPage = BOOKMODE_MINIMAP; break;
		}
		
		if(canOpenBookPage(prevPage)) {
			return prevPage;
		}
		
	} while(prevPage != oldPage);
	return Book_Mode;
}

void ARX_INTERFACE_BookOpen() {
	if((player.Interface & INTER_MAP))
		return;
	
	ARX_INTERFACE_BookToggle();
}

void ARX_INTERFACE_BookClose() {
	if(!(player.Interface & INTER_MAP))
		return;
	
	ARX_INTERFACE_BookToggle();
}

void ARX_INTERFACE_BookToggle() {
	
	if(player.Interface & INTER_MAP) {
		ARX_SOUND_PlayInterface(SND_BOOK_CLOSE, 0.9F + 0.2F * rnd());
		SendIOScriptEvent(entities.player(),SM_BOOK_CLOSE);
		player.Interface &=~ INTER_MAP;
		g_miniMap.purgeTexContainer();

		if(ARXmenu.mda) {
			for(long i = 0; i < MAX_FLYOVER; i++) {
				ARXmenu.mda->flyover[i].clear();
			}
			free(ARXmenu.mda);
			ARXmenu.mda=NULL;
		}
		
		onBookClosePage();
	} else {
		SendIOScriptEvent(entities.player(),SM_NULL,"","book_open");

		ARX_SOUND_PlayInterface(SND_BOOK_OPEN, 0.9F + 0.2F * rnd());
		SendIOScriptEvent(entities.player(),SM_BOOK_OPEN);
		ARX_INTERFACE_NoteClose();
		player.Interface |= INTER_MAP;
		Book_MapPage = ARX_LEVELS_GetRealNum(CURRENTLEVEL);
		Book_MapPage = glm::clamp(Book_MapPage, 0l, 7l);
		
		if(!ARXmenu.mda) {
//			ARXmenu.mda = (MENU_DYNAMIC_DATA *)malloc(sizeof(MENU_DYNAMIC_DATA));
//			memset(ARXmenu.mda,0,sizeof(MENU_DYNAMIC_DATA));
			ARXmenu.mda = new MENU_DYNAMIC_DATA();
			
			ARXmenu.mda->flyover[BOOK_STRENGTH] = getLocalised("system_charsheet_strength");
			ARXmenu.mda->flyover[BOOK_MIND] = getLocalised("system_charsheet_intel");
			ARXmenu.mda->flyover[BOOK_DEXTERITY] = getLocalised("system_charsheet_dex");
			ARXmenu.mda->flyover[BOOK_CONSTITUTION] = getLocalised("system_charsheet_consti");
			ARXmenu.mda->flyover[BOOK_STEALTH] = getLocalised("system_charsheet_stealth");
			ARXmenu.mda->flyover[BOOK_MECANISM] = getLocalised("system_charsheet_mecanism");
			ARXmenu.mda->flyover[BOOK_INTUITION] = getLocalised("system_charsheet_intuition");
			ARXmenu.mda->flyover[BOOK_ETHERAL_LINK] = getLocalised("system_charsheet_etheral_link");
			ARXmenu.mda->flyover[BOOK_OBJECT_KNOWLEDGE] = getLocalised("system_charsheet_objknoledge");
			ARXmenu.mda->flyover[BOOK_CASTING] = getLocalised("system_charsheet_casting");
			ARXmenu.mda->flyover[BOOK_PROJECTILE] = getLocalised("system_charsheet_projectile");
			ARXmenu.mda->flyover[BOOK_CLOSE_COMBAT] = getLocalised("system_charsheet_closecombat");
			ARXmenu.mda->flyover[BOOK_DEFENSE] = getLocalised("system_charsheet_defense");
			ARXmenu.mda->flyover[BUTTON_QUICK_GENERATION] = getLocalised("system_charsheet_quickgenerate");
			ARXmenu.mda->flyover[BUTTON_DONE] = getLocalised("system_charsheet_done");
			ARXmenu.mda->flyover[BUTTON_SKIN] = getLocalised("system_charsheet_skin");
			ARXmenu.mda->flyover[WND_ATTRIBUTES] = getLocalised("system_charsheet_atributes");
			ARXmenu.mda->flyover[WND_SKILLS] = getLocalised("system_charsheet_skills");
			ARXmenu.mda->flyover[WND_STATUS] = getLocalised("system_charsheet_status");
			ARXmenu.mda->flyover[WND_LEVEL] = getLocalised("system_charsheet_level");
			ARXmenu.mda->flyover[WND_XP] = getLocalised("system_charsheet_xpoints");
			ARXmenu.mda->flyover[WND_HP] = getLocalised("system_charsheet_hp");
			ARXmenu.mda->flyover[WND_MANA] = getLocalised("system_charsheet_mana");
			ARXmenu.mda->flyover[WND_AC] = getLocalised("system_charsheet_ac");
			ARXmenu.mda->flyover[WND_RESIST_MAGIC] = getLocalised("system_charsheet_res_magic");
			ARXmenu.mda->flyover[WND_RESIST_POISON] = getLocalised("system_charsheet_res_poison");
			ARXmenu.mda->flyover[WND_DAMAGE] = getLocalised("system_charsheet_damage");
		}
	}

	if(player.Interface & INTER_COMBATMODE) {
		player.Interface&=~INTER_COMBATMODE;
		ARX_EQUIPMENT_LaunchPlayerUnReadyWeapon();
	}

	if(player.Interface & INTER_INVENTORYALL) {
		ARX_SOUND_PlayInterface(SND_BACKPACK, 0.9F + 0.2F * rnd());
		bInventoryClosing = true;
	}

	BOOKZOOM = 0;
	pTextManage->Clear();

	TRUE_PLAYER_MOUSELOOK_ON = false;
}




static bool MouseInBookRect(const Vec2f pos, const Vec2f size) {
	return DANAEMouse.x >= (pos.x + BOOKDEC.x) * g_sizeRatio.x
		&& DANAEMouse.x <= (pos.x + size.x + BOOKDEC.x) * g_sizeRatio.x
		&& DANAEMouse.y >= (pos.y + BOOKDEC.y) * g_sizeRatio.y
		&& DANAEMouse.y <= (pos.y + size.y + BOOKDEC.y) * g_sizeRatio.y;
}

bool ARX_INTERFACE_MouseInBook() {
	if((player.Interface & INTER_MAP) && !(player.Interface & INTER_COMBATMODE)) {
		return MouseInBookRect(Vec2f(99, 65), Vec2f(500, 307));
	} else {
		return false;
	}
}

static void DrawBookInterfaceItem(TextureContainer * tc, Vec2f pos, Color color = Color::white, float z = 0.000001f) {
	if(tc) {
		EERIEDrawBitmap2(Rectf(
			(pos + BOOKDEC) * g_sizeRatio,
			tc->m_dwWidth * g_sizeRatio.x,
			tc->m_dwHeight * g_sizeRatio.y),
			z,
			tc,
			color
		);
	}
}

static void RenderBookPlayerCharacter() {
	
	// TODO use assert ?
	if(!entities.player()->obj)
		return;
	
	GRenderer->SetRenderState(Renderer::DepthWrite, true);
	
	Rect rec;
	if (BOOKZOOM) {
		
		rec = Rect(s32((120.f + BOOKDEC.x) * g_sizeRatio.x), s32((69.f + BOOKDEC.y) * g_sizeRatio.y),
				   s32((330.f + BOOKDEC.x) * g_sizeRatio.x), s32((300.f + BOOKDEC.y) * g_sizeRatio.y));
		GRenderer->Clear(Renderer::DepthBuffer, Color::none, 1.f, 1, &rec);
		
		if(ARXmenu.currentmode != AMCM_OFF) {
			Rect vp = Rect(Vec2i(s32(139.f * g_sizeRatio.x), 0), s32(139.f * g_sizeRatio.x), s32(310.f * g_sizeRatio.y));
			GRenderer->SetScissor(vp);
		}
	} else {
		
		rec = Rect(s32((118.f + BOOKDEC.x) * g_sizeRatio.x), s32((69.f + BOOKDEC.y) * g_sizeRatio.y),
				  s32((350.f + BOOKDEC.x) * g_sizeRatio.x), s32((338.f + BOOKDEC.y) * g_sizeRatio.y));
		GRenderer->Clear(Renderer::DepthBuffer, Color::none, 1.f, 1, &rec);

		rec.right -= 50;
	}
	
	if(ARXmenu.currentmode == AMCM_OFF)
		BOOKZOOM = 0;
	
	Vec3f pos;
	EERIE_LIGHT eLight1;
	EERIE_LIGHT eLight2;
	
	eLight1.pos = Vec3f(50.f, 50.f, 200.f);
	eLight1.exist = 1;
	eLight1.rgb = Color3f(0.15f, 0.06f, 0.003f);
	eLight1.intensity = 8.8f;
	eLight1.fallstart = 2020;
	eLight1.fallend = eLight1.fallstart + 60;
	RecalcLight(&eLight1);
	
	eLight2.exist = 1;
	eLight2.pos = Vec3f(-50.f, -50.f, -200.f);
	eLight2.rgb = Color3f::gray(0.6f);
	eLight2.intensity = 3.8f;
	eLight2.fallstart = 0;
	eLight2.fallend = eLight2.fallstart + 3460.f;
	RecalcLight(&eLight2);
	
	EERIE_LIGHT * SavePDL[2];
	SavePDL[0] = PDL[0];
	SavePDL[1] = PDL[1];
	int iSavePDL = TOTPDL;
	
	PDL[0] = &eLight1;
	PDL[1] = &eLight2;
	TOTPDL = 2;
	
	EERIE_CAMERA * oldcam = ACTIVECAM;
	bookcam.center = rec.center();
	SetActiveCamera(&bookcam);
	PrepareCamera(&bookcam, g_size);
	
	Anglef ePlayerAngle = Anglef::ZERO;
	
	if(BOOKZOOM) {
		Rect vp;
		vp.left = static_cast<int>(rec.left + 52.f * g_sizeRatio.x);
		vp.top = rec.top;
		vp.right = static_cast<int>(rec.right - 21.f * g_sizeRatio.x);
		vp.bottom = static_cast<int>(rec.bottom - 17.f * g_sizeRatio.y);
		GRenderer->SetScissor(vp);
		
		switch(player.skin) {
			case 0:
				ePlayerAngle.setPitch(-25.f);
				break;
			case 1:
				ePlayerAngle.setPitch(-10.f);
				break;
			case 2:
				ePlayerAngle.setPitch(20.f);
				break;
			case 3:
				ePlayerAngle.setPitch(35.f);
				break;
		}
		
		pos = Vec3f(8, 162, 75);
		eLight1.pos.z = -90.f;
	} else {
		GRenderer->SetScissor(rec);
		
		ePlayerAngle.setPitch(-20.f);
		pos = Vec3f(20.f, 96.f, 260.f);
		
		ARX_EQUIPMENT_AttachPlayerWeaponToHand();
	}
	
	bool ti = player.m_improve;
	player.m_improve = false;
	
	
	float invisibility = entities.player()->invisibility;
	
	if(invisibility > 0.5f)
		invisibility = 0.5f;
	
	IN_BOOK_DRAW = 1;
	std::vector<EERIE_VERTEX> vertexlist = entities.player()->obj->vertexlist3;
	
	arx_assert(player.bookAnimation[0].cur_anim);
	
	EERIEDrawAnimQuat(entities.player()->obj, player.bookAnimation, ePlayerAngle, pos,
					  checked_range_cast<unsigned long>(Original_framedelay), NULL, true, invisibility);
	
	IN_BOOK_DRAW = 0;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		GRenderer->SetRenderState(Renderer::DepthTest, true);
		GRenderer->GetTextureStage(0)->setMipFilter(TextureStage::FilterNone);
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		PopAllTriangleList();
		GRenderer->SetRenderState(Renderer::AlphaBlending, true);
		PopAllTriangleListTransparency();
		GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		GRenderer->GetTextureStage(0)->setMipFilter(TextureStage::FilterLinear);
		GRenderer->SetRenderState(Renderer::DepthTest, false);
	}
	
	PDL[0] = SavePDL[0];
	PDL[1] = SavePDL[1];
	TOTPDL = iSavePDL;
	
	entities.player()->obj->vertexlist3 = vertexlist;
	vertexlist.clear();
	
	player.m_improve = ti;
	
	GRenderer->SetScissor(Rect::ZERO);
	
	GRenderer->SetRenderState(Renderer::AlphaBlending, false);
	GRenderer->SetCulling(Renderer::CullNone);
	SetActiveCamera(oldcam);
	PrepareCamera(oldcam, g_size);
	
	player.bookAnimation[0].cur_anim = herowaitbook;
	
	if(ValidIONum(player.equiped[EQUIP_SLOT_WEAPON])) {
		if(entities[player.equiped[EQUIP_SLOT_WEAPON]]->type_flags & OBJECT_TYPE_2H) {
			player.bookAnimation[0].cur_anim = herowait_2h;
		}
	}
	
	GRenderer->SetCulling(Renderer::CullNone);
	
	if(ValidIONum(player.equiped[EQUIP_SLOT_ARMOR])) {
		Entity *tod = entities[player.equiped[EQUIP_SLOT_ARMOR]];
		if(tod) {
			tod->bbox2D.min = Vec2f(195.f, 116.f);
			tod->bbox2D.max = Vec2f(284.f, 182.f);
			
			tod->bbox2D.min = (tod->bbox2D.min + BOOKDEC) * g_sizeRatio;
			tod->bbox2D.max = (tod->bbox2D.max + BOOKDEC) * g_sizeRatio;
			
			tod->ioflags |= IO_ICONIC;
		}
	}
	
	if(ValidIONum(player.equiped[EQUIP_SLOT_LEGGINGS])) {
		Entity *tod = entities[player.equiped[EQUIP_SLOT_LEGGINGS]];
		if(tod) {
			tod->bbox2D.min = Vec2f(218.f, 183.f);
			tod->bbox2D.max = Vec2f(277.f, 322.f);
			
			tod->bbox2D.min = (tod->bbox2D.min + BOOKDEC) * g_sizeRatio;
			tod->bbox2D.max = (tod->bbox2D.max + BOOKDEC) * g_sizeRatio;
			
			tod->ioflags |= IO_ICONIC;
		}
	}
	
	if(ValidIONum(player.equiped[EQUIP_SLOT_HELMET])) {
		Entity *tod = entities[player.equiped[EQUIP_SLOT_HELMET]];
		if(tod) {
			tod->bbox2D.min = Vec2f(218.f, 75.f);
			tod->bbox2D.max = Vec2f(260.f, 115.f);
			
			tod->bbox2D.min = (tod->bbox2D.min + BOOKDEC) * g_sizeRatio;
			tod->bbox2D.max = (tod->bbox2D.max + BOOKDEC) * g_sizeRatio;
			
			tod->ioflags |= IO_ICONIC;
		}
	}
	
	TextureContainer * tc;
	TextureContainer * tc2=NULL;
	GRenderer->SetCulling(Renderer::CullNone);
	
	if(ValidIONum(player.equiped[EQUIP_SLOT_RING_LEFT])) {
		Entity *todraw = entities[player.equiped[EQUIP_SLOT_RING_LEFT]];
		
		tc = todraw->inv;
		
		if(NeedHalo(todraw))
			tc2 = todraw->inv->getHalo();
		
		if(tc) {
			todraw->bbox2D.min = Vec2f(146.f, 312.f);
			
			Color color = (todraw->poisonous && todraw->poisonous_count != 0) ? Color::green : Color::white;
			DrawBookInterfaceItem(tc, todraw->bbox2D.min, color, 0);
			
			if(tc2) {
				ARX_INTERFACE_HALO_Draw(todraw, tc, tc2, (todraw->bbox2D.min + BOOKDEC) * g_sizeRatio, g_sizeRatio);
			}
			
			todraw->bbox2D.max = todraw->bbox2D.min + Vec2f(tc->size());
			
			todraw->bbox2D.min = (todraw->bbox2D.min + BOOKDEC) * g_sizeRatio;
			todraw->bbox2D.max = (todraw->bbox2D.max + BOOKDEC) * g_sizeRatio;
			
			todraw->ioflags |= IO_ICONIC;
		}
	}
	
	tc2=NULL;
	
	if(ValidIONum(player.equiped[EQUIP_SLOT_RING_RIGHT])) {
		Entity *todraw = entities[player.equiped[EQUIP_SLOT_RING_RIGHT]];
		
		tc = todraw->inv;
		
		if(NeedHalo(todraw))
			tc2 = todraw->inv->getHalo();
		
		if(tc) {
			todraw->bbox2D.min = Vec2f(296.f, 312.f);
			
			Color color = (todraw->poisonous && todraw->poisonous_count != 0) ? Color::green : Color::white;
			DrawBookInterfaceItem(tc, todraw->bbox2D.min, color, 0);
			
			if(tc2) {
				ARX_INTERFACE_HALO_Draw(todraw, tc, tc2, (todraw->bbox2D.min + BOOKDEC) * g_sizeRatio, g_sizeRatio);
			}
			
			todraw->bbox2D.max = todraw->bbox2D.min + Vec2f(tc->size());
			
			todraw->bbox2D.min = (todraw->bbox2D.min + BOOKDEC) * g_sizeRatio;
			todraw->bbox2D.max = (todraw->bbox2D.max + BOOKDEC) * g_sizeRatio;
			
			todraw->ioflags |= IO_ICONIC;
		}
	}
	
	if(!BOOKZOOM)
		ARX_EQUIPMENT_AttachPlayerWeaponToBack();
	
	Halo_Render();
}

static void ARX_INTERFACE_RELEASESOUND() {
	ARX_SOUND_PlayInterface(SND_MENU_RELEASE);
}

static void ARX_INTERFACE_ERRORSOUND() {
	ARX_SOUND_PlayInterface(SND_MENU_CLICK);
}

extern long BOOKBUTTON;
extern long LASTBOOKBUTTON;

static bool CheckAttributeClick(Vec2f pos, float * val, TextureContainer * tc) {
	
	bool rval=false;
	float t = *val;

	if(MouseInBookRect(pos, Vec2f(32, 32))) {
		rval = true;

		if(((BOOKBUTTON & 1) || (BOOKBUTTON & 2)) && tc)
			DrawBookInterfaceItem(tc, pos);

		if(!(BOOKBUTTON & 1) && (LASTBOOKBUTTON & 1)) {
			if(player.Attribute_Redistribute > 0) {
				player.Attribute_Redistribute--;
				t++;
				*val=t;
				ARX_INTERFACE_RELEASESOUND();
			}
			else
				ARX_INTERFACE_ERRORSOUND();
		}

		if(!(BOOKBUTTON & 2) && (LASTBOOKBUTTON & 2)) {
			if(ARXmenu.currentmode == AMCM_NEWQUEST) {
				if(t > 6 && player.level == 0) {
					player.Attribute_Redistribute++;
					t --;
					*val=t;
					ARX_INTERFACE_RELEASESOUND();
				}
				else
					ARX_INTERFACE_ERRORSOUND();
			}
			else
				ARX_INTERFACE_ERRORSOUND();
		}
	}

	return rval;
}

static bool CheckSkillClick(Vec2f pos, float * val, TextureContainer * tc,
                            float * oldval) {
	
	bool rval=false;

	float t = *val;
	float ot = *oldval;

	if(MouseInBookRect(pos, Vec2f(32, 32))) {
		rval=true;

		if(((BOOKBUTTON & 1) || (BOOKBUTTON & 2)) && tc)
			DrawBookInterfaceItem(tc, pos);

		if(!(BOOKBUTTON & 1) && (LASTBOOKBUTTON & 1)) {
			if(player.Skill_Redistribute > 0) {
				player.Skill_Redistribute--;
				t++;
				*val=t;
				ARX_INTERFACE_RELEASESOUND();
			}
			else
				ARX_INTERFACE_ERRORSOUND();
		}

		if(!(BOOKBUTTON & 2) && (LASTBOOKBUTTON & 2)) {
			if(ARXmenu.currentmode == AMCM_NEWQUEST) {
				if(t > ot && player.level == 0) {
					player.Skill_Redistribute++;
					t --;
					*val=t;
					ARX_INTERFACE_RELEASESOUND();
				}
				else
					ARX_INTERFACE_ERRORSOUND();
			}
			else
				ARX_INTERFACE_ERRORSOUND();
		}
	}

	return rval;
}


namespace gui {

/*!
 * Manage forward and backward buttons on notes and the quest book.
 * \return true if the note was clicked
 */
bool manageNoteActions(Note & note) {
	
	if(note.prevPageButton().contains(Vec2f(DANAEMouse))) {
		SpecialCursor = CURSOR_INTERACTION_ON;
		if(eeMouseUp1()) {
			ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9f + 0.2f * rnd());
			arx_assert(note.page() >= 2);
			note.setPage(note.page() - 2);
		}
		
	} else if(note.nextPageButton().contains(Vec2f(DANAEMouse))) {
		SpecialCursor = CURSOR_INTERACTION_ON;
		if(eeMouseUp1()) {
			ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9f + 0.2f * rnd());
			note.setPage(note.page() + 2);
		}
		
	} else if(note.area().contains(Vec2f(DANAEMouse))) {
		if((eeMouseDown1() && TRUE_PLAYER_MOUSELOOK_ON)
		   || (eeMouseDown2())) {
			EERIEMouseButton &= ~2;
			return true;
		}
	}
	
	return false;
}

static gui::Note questBook;

//! Update and render the quest book.
static void manageQuestBook() {
	
	// Cache the questbook data
	if(questBook.text().empty() && !PlayerQuest.empty()) {
		std::string text;
		for(size_t i = 0; i < PlayerQuest.size(); ++i) {
			std::string quest = getLocalised(PlayerQuest[i].ident);
			if(!quest.empty()) {
				text += quest;
				text += "\n\n";
			}
		}
		questBook.setData(Note::QuestBook, text);
		questBook.setPage(questBook.pageCount() - 1);
	}
	
	manageNoteActions(questBook);
	
	questBook.render();
}

void updateQuestBook() {
	// Clear the quest book cache - it will be re-created when needed
	questBook.clear();
}

} // namespace gui

//-----------------------------------------------------------------------------


static void ARX_INTERFACE_ManageOpenedBook_TopTabs() {
	
	static const Vec2f BOOKMARKS_POS = Vec2f(216.f, 60.f);
	
	if(Book_Mode != BOOKMODE_STATS) {
		Vec2f pos = BOOKMARKS_POS;
		
		TextureContainer* tcBookmarkChar = ITC.bookmark_char;
		DrawBookInterfaceItem(tcBookmarkChar, pos);
		
		// Check for cursor on charcter sheet bookmark
		if(MouseInBookRect(pos, Vec2f(tcBookmarkChar->m_dwWidth, tcBookmarkChar->m_dwHeight))) {
			// Draw highlighted Character sheet icon
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			DrawBookInterfaceItem(tcBookmarkChar, pos, Color::grayb(0x55));
			GRenderer->SetRenderState(Renderer::AlphaBlending, false);
			
			// Set cursor to interacting
			SpecialCursor=CURSOR_INTERACTION_ON;
			
			// Check for click
			if(bookclick) {
				ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
				openBookPage(BOOKMODE_STATS);
				pTextManage->Clear();
			}
		}
	}
	
	if(Book_Mode != BOOKMODE_SPELLS) {
		if(player.rune_flags) {
			Vec2f pos = BOOKMARKS_POS + Vec2f(32, 0);
			
			DrawBookInterfaceItem(ITC.bookmark_magic, pos);

			if(NewSpell == 1) {
				NewSpell = 2;
				for(long nk = 0; nk < 2; nk++) {
					MagFX(Vec3f(BOOKDEC.x + 220.f, BOOKDEC.y + 49.f, 0.000001f));
				}
			}
			
			if(MouseInBookRect(pos, Vec2f(ITC.bookmark_magic->m_dwWidth, ITC.bookmark_magic->m_dwHeight))) {
				// Draw highlighted Magic sheet icon
				GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
				GRenderer->SetRenderState(Renderer::AlphaBlending, true);
				DrawBookInterfaceItem(ITC.bookmark_magic, pos, Color::grayb(0x55));
				GRenderer->SetRenderState(Renderer::AlphaBlending, false);
				
				// Set cursor to interacting
				SpecialCursor=CURSOR_INTERACTION_ON;
				
				// Check for click
				if(bookclick) {
					ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					openBookPage(BOOKMODE_SPELLS);
					pTextManage->Clear();
				}
			}
		}
	}
	
	if(Book_Mode != BOOKMODE_MINIMAP) {
		Vec2f pos = BOOKMARKS_POS + Vec2f(64, 0);
		
		DrawBookInterfaceItem(ITC.bookmark_map, pos);
		
		if(MouseInBookRect(pos, Vec2f(ITC.bookmark_map->m_dwWidth, ITC.bookmark_map->m_dwHeight))) {
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			DrawBookInterfaceItem(ITC.bookmark_map, pos, Color::grayb(0x55));
			GRenderer->SetRenderState(Renderer::AlphaBlending, false);
			
			// Set cursor to interacting
			SpecialCursor=CURSOR_INTERACTION_ON;
			
			// Check for click
			if(bookclick) {
				ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
				openBookPage(BOOKMODE_MINIMAP);
				pTextManage->Clear();
			}
		}
	}
	
	if(Book_Mode != BOOKMODE_QUESTS) {
		Vec2f pos = BOOKMARKS_POS + Vec2f(96, 0);
		
		DrawBookInterfaceItem(ITC.bookmark_quest, pos);
		
		if(MouseInBookRect(pos, Vec2f(ITC.bookmark_quest->m_dwWidth, ITC.bookmark_quest->m_dwHeight))) {
			GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			DrawBookInterfaceItem(ITC.bookmark_quest, pos, Color::grayb(0x55));
			GRenderer->SetRenderState(Renderer::AlphaBlending, false);
			
			// Set cursor to interacting
			SpecialCursor=CURSOR_INTERACTION_ON;
			
			// Check for click
			if(bookclick) {
				ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
				openBookPage(BOOKMODE_QUESTS);
				pTextManage->Clear();
			}
		}
	}
}

static void ARX_INTERFACE_ManageOpenedBook_LeftTabs(bool tabVisibility[10], long & activeTab) {
	
		if(tabVisibility[0]) {
			if(activeTab!=0) {
				Vec2f pos = Vec2f(100.f, 82.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[0], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[0], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=0;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[0], Vec2f(102.f, 82.f));
		}

		if(tabVisibility[1]) {
			if(activeTab!=1) {
				Vec2f pos = Vec2f(98.f, 112.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[1], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[1], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=1;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[1], Vec2f(100.f, 114.f));
		}

		if(tabVisibility[2]) {
			if(activeTab!=2) {
				Vec2f pos = Vec2f(97.f, 143.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[2], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[2], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=2;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[2], Vec2f(101.f, 141.f));
		}

		if(tabVisibility[3]) {
			if(activeTab!=3) {
				Vec2f pos = Vec2f(95.f, 170.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[3], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[3], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=3;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[3], Vec2f(100.f, 170.f));
		}

		if(tabVisibility[4]) {
			if(activeTab!=4) {
				Vec2f pos = Vec2f(95.f, 200.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[4], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[4], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=4;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[4], Vec2f(97.f, 199.f));
		}

		if(tabVisibility[5]) {
			if(activeTab!=5) {
				Vec2f pos = Vec2f(94.f, 229.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[5], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[5], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=5;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[5], Vec2f(103.f, 226.f));
		}

		if(tabVisibility[6]) {
			if(activeTab!=6) {
				Vec2f pos = Vec2f(94.f, 259.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[6], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[6], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=6;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[6], Vec2f(101.f, 255.f));
		}

		if(tabVisibility[7]) {
			if(activeTab!=7) {
				Vec2f pos = Vec2f(92.f, 282.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[7], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[7], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=7;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[7], Vec2f(99.f, 283.f));
		}

		if(tabVisibility[8]) {
			if(activeTab!=8) {
				Vec2f pos = Vec2f(90.f, 308.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[8], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[8], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=8;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[8], Vec2f(99.f, 307.f));
		}

		if(tabVisibility[9]) {
			if (activeTab!=9) {
				Vec2f pos = Vec2f(97.f, 331.f);
				
				DrawBookInterfaceItem(ITC.accessibleTab[9], pos);

				if(MouseInBookRect(pos, Vec2f(32, 32))) {
					GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
					GRenderer->SetRenderState(Renderer::AlphaBlending, true);
					DrawBookInterfaceItem(ITC.accessibleTab[9], pos, Color::grayb(0x55));
					GRenderer->SetRenderState(Renderer::AlphaBlending, false);
					SpecialCursor=CURSOR_INTERACTION_ON;
					if(bookclick) {
						activeTab=9;
						ARX_SOUND_PlayInterface(SND_BOOK_PAGE_TURN, 0.9F + 0.2F * rnd());
					}
				}
			}
			else DrawBookInterfaceItem(ITC.currentTab[9], Vec2f(104.f, 331.f));
		}
}

static void ARX_INTERFACE_ManageOpenedBook_LeftTabs_Spells() {
	
	bool tabVisibility[10] = {false};

	for(size_t i = 0; i < SPELL_TYPES_COUNT; ++i) {
		if(spellicons[i].bSecret == false) {
			bool bOk = true;

			for(long j = 0; j < 4 && spellicons[i].symbols[j] != RUNE_NONE; ++j) {
				if(!player.hasRune(spellicons[i].symbols[j]))
					bOk = false;
			}

			if(bOk)
				tabVisibility[spellicons[i].level - 1] = true;
		}
	}
	
	ARX_INTERFACE_ManageOpenedBook_LeftTabs(tabVisibility, Book_SpellPage);
}

static void ARX_INTERFACE_ManageOpenedBook_LeftTabs_Map() {
	
	bool tabVisibility[10] = {false};
	
	long max_onglet = 7;
	memset(tabVisibility, true, (max_onglet + 1) * sizeof(*tabVisibility));
	
	ARX_INTERFACE_ManageOpenedBook_LeftTabs(tabVisibility, Book_MapPage);
}

static void ARX_INTERFACE_ManageOpenedBook_Stats()
{
	FLYING_OVER = 0;
	std::string tex;
	Color color(0, 0, 0);

	ARX_PLAYER_ComputePlayerFullStats();

	std::stringstream ss;
	ss << ITC.Level << " " << std::setw(3) << (int)player.level;
	tex = ss.str();
	DrawBookTextCenter(hFontInBook, Vec2f(398, 74), tex, color);

	std::stringstream ss2;
	ss2 << ITC.Xp << " " << std::setw(8) << player.xp;
	tex = ss2.str();
	DrawBookTextCenter(hFontInBook, Vec2f(510, 74), tex, color);

	if (MouseInBookRect(Vec2f(463, 74), Vec2f(87, 20)))
		FLYING_OVER = WND_XP;

	if (MouseInBookRect(Vec2f(97+41,64+62), Vec2f(32, 32)))
		FLYING_OVER = WND_AC;
	else if (MouseInBookRect(Vec2f(97+41,64+120), Vec2f(32, 32)))
		FLYING_OVER = WND_RESIST_MAGIC;
	else if (MouseInBookRect(Vec2f(97+41,64+178), Vec2f(32, 32)))
		FLYING_OVER = WND_RESIST_POISON;
	else if (MouseInBookRect(Vec2f(97+211,64+62), Vec2f(32, 32)))
		FLYING_OVER = WND_HP;
	else if (MouseInBookRect(Vec2f(97+211,64+120), Vec2f(32, 32)))
		FLYING_OVER = WND_MANA;
	else if (MouseInBookRect(Vec2f(97+211,64+178), Vec2f(32, 32)))
		FLYING_OVER = WND_DAMAGE;

	if(!((player.Attribute_Redistribute == 0) && (ARXmenu.currentmode != AMCM_NEWQUEST))) {
		// Main Player Attributes
		if(CheckAttributeClick(Vec2f(379, 95), &player.m_attribute.strength, ITC.ic_strength)) {
			FLYING_OVER = BOOK_STRENGTH;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Attribute_Redistribute;
		}

		if(CheckAttributeClick(Vec2f(428, 95), &player.m_attribute.mind, ITC.ic_mind)) {
			FLYING_OVER = BOOK_MIND;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Attribute_Redistribute;
		}

		if(CheckAttributeClick(Vec2f(477, 95), &player.m_attribute.dexterity, ITC.ic_dexterity)) {
			FLYING_OVER = BOOK_DEXTERITY;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Attribute_Redistribute;
		}

		if(CheckAttributeClick(Vec2f(526, 95), &player.m_attribute.constitution, ITC.ic_constitution)) {
			FLYING_OVER = BOOK_CONSTITUTION;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Attribute_Redistribute;
		}
	}

	if(!((player.Skill_Redistribute == 0) && (ARXmenu.currentmode != AMCM_NEWQUEST))) {
		if (CheckSkillClick(Vec2f(389, 177), &player.m_skill.stealth, ITC.ic_stealth, &player.m_skillOld.stealth)) {
			FLYING_OVER = BOOK_STEALTH;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(453, 177), &player.m_skill.mecanism, ITC.ic_mecanism, &player.m_skillOld.mecanism)) {
			FLYING_OVER = BOOK_MECANISM;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(516, 177), &player.m_skill.intuition, ITC.ic_intuition, &player.m_skillOld.intuition)) {
			FLYING_OVER = BOOK_INTUITION;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(389, 230), &player.m_skill.etheralLink, ITC.ic_etheral_link, &player.m_skillOld.etheralLink)) {
			FLYING_OVER = BOOK_ETHERAL_LINK;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(453, 230), &player.m_skill.objectKnowledge, ITC.ic_object_knowledge, &player.m_skillOld.objectKnowledge)) {
			FLYING_OVER = BOOK_OBJECT_KNOWLEDGE;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;

			if((BOOKBUTTON & 1) && !(LASTBOOKBUTTON & 1)) {
				ARX_INVENTORY_IdentifyAll();
				ARX_EQUIPMENT_IdentifyAll();
			}

			ARX_PLAYER_ComputePlayerFullStats();
		}

		if(CheckSkillClick(Vec2f(516, 230), &player.m_skill.casting, ITC.ic_casting, &player.m_skillOld.casting)) {
			FLYING_OVER = BOOK_CASTING;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(389, 284), &player.m_skill.closeCombat, ITC.ic_close_combat, &player.m_skillOld.closeCombat)) {
			FLYING_OVER = BOOK_CLOSE_COMBAT;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(453, 284), &player.m_skill.projectile, ITC.ic_projectile, &player.m_skillOld.projectile)) {
			FLYING_OVER = BOOK_PROJECTILE;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}

		if(CheckSkillClick(Vec2f(516, 284), &player.m_skill.defense, ITC.ic_defense, &player.m_skillOld.defense)) {
			FLYING_OVER = BOOK_DEFENSE;
			SpecialCursor = CURSOR_REDIST;
			lCursorRedistValue = player.Skill_Redistribute;
		}
	} else {
		//------------------------------------PRIMARY
		if (MouseInBookRect(Vec2f(379,95), Vec2f(32, 32)))
			FLYING_OVER=BOOK_STRENGTH;
		else if (MouseInBookRect(Vec2f(428,95), Vec2f(32, 32)))
			FLYING_OVER=BOOK_MIND;
		else if (MouseInBookRect(Vec2f(477,95), Vec2f(32, 32)))
			FLYING_OVER=BOOK_DEXTERITY;
		else if (MouseInBookRect(Vec2f(526,95), Vec2f(32, 32)))
			FLYING_OVER=BOOK_CONSTITUTION;

		//------------------------------------SECONDARY
		if (MouseInBookRect(Vec2f(389,177), Vec2f(32, 32)))
			FLYING_OVER=BOOK_STEALTH;
		else if (MouseInBookRect(Vec2f(453,177), Vec2f(32, 32)))
			FLYING_OVER=BOOK_MECANISM;
		else if (MouseInBookRect(Vec2f(516,177), Vec2f(32, 32)))
			FLYING_OVER=BOOK_INTUITION;
		else if (MouseInBookRect(Vec2f(389,230), Vec2f(32, 32)))
			FLYING_OVER=BOOK_ETHERAL_LINK;
		else if (MouseInBookRect(Vec2f(453,230), Vec2f(32, 32)))
			FLYING_OVER=BOOK_OBJECT_KNOWLEDGE;
		else if (MouseInBookRect(Vec2f(516,230), Vec2f(32, 32)))
			FLYING_OVER=BOOK_CASTING;
		else if (MouseInBookRect(Vec2f(389,284), Vec2f(32, 32)))
			FLYING_OVER=BOOK_CLOSE_COMBAT;
		else if (MouseInBookRect(Vec2f(453,284), Vec2f(32, 32)))
			FLYING_OVER=BOOK_PROJECTILE;
		else if (MouseInBookRect(Vec2f(516,284), Vec2f(32, 32)))
			FLYING_OVER=BOOK_DEFENSE;
	}

	//------------------------------ SEB 04/12/2001
	if(ARXmenu.mda && !ARXmenu.mda->flyover[FLYING_OVER].empty()) {
		
		float fRandom = rnd() * 2;

		int t = checked_range_cast<int>(fRandom);

		pTextManage->Clear();
		OLD_FLYING_OVER=FLYING_OVER;

		std::string toDisplay;

		// Nuky Note: the text used never scrolls, centered function with wordwrap would be enough
		if(FLYING_OVER == WND_XP) {
			std::stringstream ss;
			ss << ARXmenu.mda->flyover[WND_XP] << " " << std::setw(8) << GetXPforLevel(player.level+1)-player.xp;

			toDisplay = ss.str();
		} else {
			toDisplay = ARXmenu.mda->flyover[FLYING_OVER];
		}

		UNICODE_ARXDrawTextCenteredScroll(hFontInGame,
			(g_size.width()*0.5f),
			4,
			(g_size.center().x)*0.82f,
			toDisplay,
			Color(232+t,204+t,143+t),
			1000,
			0.01f,
			3,
			0);
	} else {
		OLD_FLYING_OVER=-1;
	}

	//------------------------------
	
	std::stringstream ss3;
	ss3 << std::setw(3) << std::setprecision(0) << std::fixed << player.m_attributeFull.strength;
	tex = ss3.str();
	
	if(player.m_attributeMod.strength < 0.f)
		color = Color::red;
	else if(player.m_attributeMod.strength > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_attributeFull.strength == 6)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(391, 129), tex, color);
	
	ss3.str(""); // clear the stream
	ss3 << player.m_attributeFull.mind;
	tex = ss3.str();
	
	if(player.m_attributeMod.mind < 0.f)
		color = Color::red;
	else if(player.m_attributeMod.mind > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_attributeFull.mind == 6)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(440, 129), tex, color);
	
	ss3.str("");
	ss3 << player.m_attributeFull.dexterity;
	tex = ss3.str();

	if(player.m_attributeMod.dexterity < 0.f)
		color = Color::red;
	else if(player.m_attributeMod.dexterity > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_attributeFull.dexterity == 6)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(490, 129), tex, color);
	ss3.str("");
	ss3 << player.m_attributeFull.constitution;
	tex = ss3.str();
	
	if(player.m_attributeMod.constitution < 0.f)
		color = Color::red;
	else if(player.m_attributeMod.constitution > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_attributeFull.constitution == 6)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(538, 129), tex, color);
	
	// Player Skills
	ss3.str("");
	ss3 << player.m_skillFull.stealth;
	tex = ss3.str();
	
	if (player.m_skillMod.stealth < 0.f)
		color = Color::red;
	else if (player.m_skillMod.stealth > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.stealth == 0)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(405, 210), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.mecanism;
	tex = ss3.str();
	
	if (player.m_skillMod.mecanism < 0.f)
		color = Color::red;
	else if (player.m_skillMod.mecanism > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.mecanism == 0)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(469, 210), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.intuition;
	tex = ss3.str();
	
	if (player.m_skillMod.intuition < 0.f)
		color = Color::red;
	else if (player.m_skillMod.intuition > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.intuition == 0)
			color = Color::red;
	}

	DrawBookTextCenter(hFontInBook, Vec2f(533, 210), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.etheralLink;
	tex = ss3.str();

	if(player.m_skillMod.etheralLink < 0.f)
		color = Color::red;
	else if(player.m_skillMod.etheralLink > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.etheralLink == 0)
			color = Color::red;
	}

	DrawBookTextCenter(hFontInBook, Vec2f(405, 265), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.objectKnowledge;
	tex = ss3.str();

	if(player.m_skillMod.objectKnowledge < 0.f)
		color = Color::red;
	else if(player.m_skillMod.objectKnowledge > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.objectKnowledge == 0)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(469, 265), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.casting;
	tex = ss3.str();
	
	if (player.m_skillMod.casting < 0.f)
		color = Color::red;
	else if (player.m_skillMod.casting > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.casting == 0)
			color = Color::red;
	}

	DrawBookTextCenter(hFontInBook, Vec2f(533, 265), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.closeCombat;
	tex = ss3.str();

	if (player.m_skillMod.closeCombat < 0.f)
		color = Color::red;
	else if (player.m_skillMod.closeCombat > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.closeCombat == 0)
			color = Color::red;
	}

	DrawBookTextCenter(hFontInBook, Vec2f(405, 319), tex, color);

	
	ss3.str("");
	ss3 << player.m_skillFull.projectile;
	tex = ss3.str();

	if(player.m_skillMod.projectile < 0.f)
		color = Color::red;
	else if(player.m_skillMod.projectile > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.projectile == 0)
			color = Color::red;
	}

	DrawBookTextCenter(hFontInBook, Vec2f(469, 319), tex, color);
	
	ss3.str("");
	ss3 << player.m_skillFull.defense;
	tex = ss3.str();

	if (player.m_skillMod.defense < 0.f)
		color = Color::red;
	else if (player.m_skillMod.defense > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	if(ARXmenu.currentmode == AMCM_NEWQUEST) {
		if(player.m_skill.defense == 0)
			color = Color::red;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(533, 319), tex, color);
	
	// Secondary Attributes
	std::stringstream ss4;
	ss4.str("");
	ss4 << F2L_RoundUp(player.Full_maxlife);
	tex = ss4.str();
	
	if(player.Full_maxlife < player.lifePool.max) {
		color = Color::red;
	} else if(player.Full_maxlife > player.lifePool.max) {
		color = Color::blue;
	} else {
		color = Color::black;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(324, 158), tex, color);
	
	ss4.str("");
	ss4 << F2L_RoundUp(player.Full_maxmana);
	tex = ss4.str();

	if(player.Full_maxmana < player.manaPool.max) {
		color = Color::red;
	} else if(player.Full_maxmana > player.manaPool.max) {
		color = Color::blue;
	} else {
		color = Color::black;
	}
	
	DrawBookTextCenter(hFontInBook, Vec2f(324, 218), tex, color);
	
	ss4.str("");
	ss4 << F2L_RoundUp(player.m_miscFull.damages);
	tex = ss4.str();
	
	if (player.m_miscMod.damages < 0.f)
		color = Color::red;
	else if (player.m_miscMod.damages > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	DrawBookTextCenter(hFontInBook, Vec2f(324, 278), tex, color);
	
	float ac = player.m_miscFull.armorClass;
	ss4.str("");
	ss4 << F2L_RoundUp(ac);
	tex = ss4.str();
	
	if (player.m_miscMod.armorClass < 0.f)
		color = Color::red;
	else if (player.m_miscMod.armorClass > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	DrawBookTextCenter(hFontInBook, Vec2f(153, 158), tex, color);
	
	ss4.str("");
	ss4 << std::setw(3) << std::setprecision(0) << F2L_RoundUp( player.m_miscFull.resistMagic );
	tex = ss4.str();
	
	if (player.m_miscMod.resistMagic < 0.f)
		color = Color::red;
	else if (player.m_miscMod.resistMagic > 0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	DrawBookTextCenter(hFontInBook, Vec2f(153, 218), tex, color);
	
	ss4.str("");
	ss4 << F2L_RoundUp( player.m_miscFull.resistPoison );
	tex = ss4.str();

	if (player.m_miscMod.resistPoison<0.f)
		color = Color::red;
	else if (player.m_miscMod.resistPoison>0.f)
		color = Color::blue;
	else
		color = Color::black;
	
	DrawBookTextCenter(hFontInBook, Vec2f(153, 278), tex, color);
	
	RenderBookPlayerCharacter();	
}

static void ARX_INTERFACE_ManageOpenedBook_Map()
{
	long SHOWLEVEL = Book_MapPage;

	if(SHOWLEVEL >= 0 && SHOWLEVEL < 32)
		g_miniMap.showBookEntireMap(SHOWLEVEL);

	SHOWLEVEL = ARX_LEVELS_GetRealNum(CURRENTLEVEL);

	if(SHOWLEVEL >= 0 && SHOWLEVEL < 32)
		g_miniMap.showBookMiniMap(SHOWLEVEL);
}

void ARX_INTERFACE_ManageOpenedBook() {
	arx_assert(entities.player());
	
	GRenderer->SetRenderState(Renderer::Fog, false);
	
	BOOKDEC.x = 0;
	BOOKDEC.y = 0;
	
	GRenderer->GetTextureStage(0)->setMinFilter(TextureStage::FilterLinear);
	GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterLinear);
	
	if(ARXmenu.currentmode != AMCM_NEWQUEST) {
		switch(Book_Mode) {
			case BOOKMODE_STATS: {
				DrawBookInterfaceItem(ITC.playerbook, Vec2f(97, 64), Color::white, 0.9999f);
				break;
			}
			case BOOKMODE_SPELLS: {
				DrawBookInterfaceItem(ITC.ptexspellbook, Vec2f(97, 64), Color::white, 0.9999f);
				ARX_INTERFACE_ManageOpenedBook_LeftTabs_Spells();
				break;
			}
			case BOOKMODE_MINIMAP: {
				DrawBookInterfaceItem(ITC.questbook, Vec2f(97, 64), Color::white, 0.9999f);
				ARX_INTERFACE_ManageOpenedBook_LeftTabs_Map();
				break;
			}
			case BOOKMODE_QUESTS: {
				gui::manageQuestBook();
				break;
			}
		}
		
		ARX_INTERFACE_ManageOpenedBook_TopTabs();
		
		bookclick = false;
		
	} else {
		arx_assert(ITC.playerbook);
		float x = (640 - ITC.playerbook->m_dwWidth) / 2.f;
		float y = (480 - ITC.playerbook->m_dwHeight) / 2.f;
		
		DrawBookInterfaceItem(ITC.playerbook, Vec2f(x, y));

		BOOKDEC.x = x - 97;
		// TODO copy paste error ?
		BOOKDEC.y = x - 64 + 19;
	}
	
	GRenderer->GetTextureStage(0)->setMinFilter(TextureStage::FilterNearest);
	GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterNearest);

	if(Book_Mode == BOOKMODE_STATS) {
		ARX_INTERFACE_ManageOpenedBook_Stats();
	} else if (Book_Mode == BOOKMODE_MINIMAP) {
		ARX_INTERFACE_ManageOpenedBook_Map();
	}
}





void ARX_INTERFACE_ManageOpenedBook_SpellsDraw() {
	
	// Now Draws Spells for this level...
	ARX_PLAYER_ComputePlayerFullStats();
	
	Vec2f tmpPos = Vec2f_ZERO;
	bool	bFlyingOver = false;
	
	for(size_t i=0; i < SPELL_TYPES_COUNT; i++) {
		const SPELL_ICON & spellInfo = spellicons[i];
		
		if(spellInfo.level != (Book_SpellPage + 1) || spellInfo.bSecret)
			continue;
		
		// check if player can cast it
		bool bOk = true;
		long j = 0;
		
		while(j < 4 && (spellInfo.symbols[j] != RUNE_NONE)) {
			if(!player.hasRune(spellInfo.symbols[j])) {
				bOk = false;
			}
			
			j++;
		}
		
		if(!bOk)
			continue;
			
		Vec2f fPos = Vec2f(170.f, 135.f) + tmpPos * Vec2f(85.f, 70.f);
		long flyingover = 0;
		
		if(MouseInBookRect(fPos, Vec2f(48, 48))) {
			bFlyingOver = true;
			flyingover = 1;
			
			SpecialCursor=CURSOR_INTERACTION_ON;
			DrawBookTextCenter(hFontInBook, Vec2f(208, 90), spellInfo.name, Color::none);
			
			for(size_t si = 0; si < MAX_SPEECH; si++) {
				if(speech[si].timecreation > 0)
					FLYING_OVER=0;
			}
			
			OLD_FLYING_OVER = FLYING_OVER;
			pTextManage->Clear();
			UNICODE_ARXDrawTextCenteredScroll(hFontInGame,
				static_cast<float>(g_size.center().x),
				12,
				(g_size.center().x)*0.82f,
				spellInfo.description,
				Color(232,204,143),
				1000,
				0.01f,
				2,
				0);
			
			long count = 0;
			
			for(long j = 0; j < 6; ++j)
				if(spellInfo.symbols[j] != RUNE_NONE)
					++count;
			
			GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterLinear);
			for(int j = 0; j < 6; ++j) {
				if(spellInfo.symbols[j] != RUNE_NONE) {
					Vec2f pos;
					pos.x = 240 - (count * 32) * 0.5f + j * 32;
					pos.y = 306;
					DrawBookInterfaceItem(gui::necklace.pTexTab[spellInfo.symbols[j]], Vec2f(pos));
				}
			}
			GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterNearest);
		}
		
		if(spellInfo.tc) {
			GRenderer->SetRenderState(Renderer::AlphaBlending, true);
			GRenderer->SetBlendFunc(Renderer::BlendZero, Renderer::BlendInvSrcColor);
			
			Color color;
			if(flyingover) {
				color = Color::white;
				
				if(eeMouseDown1()) {
					player.SpellToMemorize.bSpell = true;
					
					for(long j = 0; j < 6; j++) {
						player.SpellToMemorize.iSpellSymbols[j] = spellInfo.symbols[j];
					}
					
					player.SpellToMemorize.lTimeCreation = (unsigned long)(arxtime);
				}
			} else {
				color = Color(168, 208, 223, 255);
			}
			
			GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterLinear);
			DrawBookInterfaceItem(spellInfo.tc, fPos, color);
			GRenderer->GetTextureStage(0)->setMagFilter(TextureStage::FilterNearest);
			
			GRenderer->SetRenderState(Renderer::AlphaBlending, false);
		}
		
		tmpPos.x ++;
		
		if(tmpPos.x >= 2) {
			tmpPos.x = 0;
			tmpPos.y ++;
		}
	}
	
	if(!bFlyingOver) {
		OLD_FLYING_OVER = -1;
		FLYING_OVER = -1;
	}
}
