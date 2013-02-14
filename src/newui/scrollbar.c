/* scrollbar.c */

#include <assert.h>
#include <stdlib.h>
#include "../os/math.h"

#include "scrollbar.h"

#include "../gui/gui.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../shape.h"
#include "../string.h"
#include "../table/strings.h"
#include "../video/video.h"

static ScrollbarItem *s_scrollbar_item;
static int s_scrollbar_max_items;
static int s_selectedHelpSubject;

static Widget *ScrollListArea_Allocate(Widget *scrollbar, enum WindowID parentID);

/*--------------------------------------------------------------*/

static void
GUI_Widget_Scrollbar_CalculateSize(WidgetScrollbar *scrollbar)
{
	Widget *w;
	uint16 size;

	w = scrollbar->parent;
	if (w == NULL) return;

	if (scrollbar->scrollMax <= 0) {
		size = (max(w->width, w->height) - 2);
	}
	else {
		size = scrollbar->scrollPageSize * (max(w->width, w->height) - 2) / scrollbar->scrollMax;
	}

	if (scrollbar->size != size) {
		scrollbar->size = size;
		scrollbar->dirty = 1;
	}
}

static void
GUI_Widget_Scrollbar_CalculatePosition(WidgetScrollbar *scrollbar)
{
	Widget *w;
	uint16 position;

	w = scrollbar->parent;
	if (w == NULL) return;

	position = scrollbar->scrollMax - scrollbar->scrollPageSize;

	if (position != 0) position = scrollbar->scrollPosition * (max(w->width, w->height) - 2 - scrollbar->size) / position;

	if (scrollbar->position != position) {
		scrollbar->position = position;
		scrollbar->dirty = 1;
	}
}

static void
GUI_Widget_Scrollbar_CalculateScrollPosition(WidgetScrollbar *scrollbar)
{
	Widget *w;

	w = scrollbar->parent;
	if (w == NULL) return;

	if (scrollbar->scrollMax - scrollbar->scrollPageSize <= 0) {
		scrollbar->scrollPosition = 0;
	}
	else {
		scrollbar->scrollPosition = scrollbar->position * (scrollbar->scrollMax - scrollbar->scrollPageSize) / (max(w->width, w->height) - 2 - scrollbar->size);
	}
}

static Widget *
GUI_Widget_Allocate_WithScrollbar(uint16 index, enum WindowID parentID,
		uint16 offsetX, uint16 offsetY, int16 width, int16 height, ScrollbarDrawProc *drawProc)
{
	Widget *w;
	WidgetScrollbar *ws;

	w = (Widget *)calloc(1, sizeof(Widget));

	w->index    = index;
	w->parentID = parentID;
	w->offsetX  = offsetX;
	w->offsetY  = offsetY;
	w->width    = width;
	w->height   = height;

	w->fgColourSelected = 10;
	w->bgColourSelected = 12;

	w->fgColourNormal = 15;
	w->bgColourNormal = 12;

	w->flags.all = 0;
	w->flags.s.buttonFilterLeft = 7;
	w->flags.s.loseSelect = true;

	w->state.all = 0;
	w->state.s.hover2Last = true;

	w->drawModeNormal   = DRAW_MODE_CUSTOM_PROC;
	w->drawModeSelected = DRAW_MODE_CUSTOM_PROC;
	w->drawModeDown     = DRAW_MODE_CUSTOM_PROC;
	w->drawParameterNormal.proc   = &GUI_Widget_Scrollbar_Draw;
	w->drawParameterSelected.proc = &GUI_Widget_Scrollbar_Draw;
	w->drawParameterDown.proc     = &GUI_Widget_Scrollbar_Draw;
	w->clickProc                  = &Scrollbar_Click;

	ws = (WidgetScrollbar *)calloc(1, sizeof(WidgetScrollbar));

	w->data = ws;

	ws->parent = w;

	ws->scrollMax      = 1;
	ws->scrollPageSize = 1;
	ws->scrollPosition = 0;
	ws->pressed        = 0;
	ws->dirty          = 0;

	ws->drawProc = drawProc;

	GUI_Widget_Scrollbar_CalculateSize(ws);
	GUI_Widget_Scrollbar_CalculatePosition(ws);

	return w;
}

static Widget *
GUI_Widget_Allocate3(uint16 index, enum WindowID parentID, uint16 offsetX, uint16 offsetY,
		uint16 sprite1, uint16 sprite2, Widget *widget2, uint16 unknown1A)
{
	Widget *w;

	w = (Widget *)calloc(1, sizeof(Widget));

	w->index    = index;
	w->parentID = parentID;
	w->offsetX  = offsetX;
	w->offsetY  = offsetY;

	w->drawModeNormal   = DRAW_MODE_SPRITE;
	w->drawModeDown     = DRAW_MODE_SPRITE;
	w->drawModeSelected = DRAW_MODE_SPRITE;

	w->width  = Shape_Width(sprite1);
	w->height = Shape_Height(sprite1);

	w->flags.all = 0;
	w->flags.s.requiresClick     = true;
	w->flags.s.clickAsHover      = true;
	w->flags.s.loseSelect        = true;
	w->flags.s.buttonFilterLeft  = 1;
	w->flags.s.buttonFilterRight = 1;

	w->drawParameterNormal.sprite   = sprite1;
	w->drawParameterSelected.sprite = sprite1;
	w->drawParameterDown.sprite     = sprite2;

	if (unknown1A != 0x0) {
		w->clickProc = &Scrollbar_ArrowDown_Click;
	} else {
		w->clickProc = &Scrollbar_ArrowUp_Click;
	}

	w->data = widget2->data;
	return w;
}

void
GUI_Widget_Scrollbar_Init(Widget *w, int16 scrollMax, int16 scrollPageSize, int16 scrollPosition)
{
	WidgetScrollbar *scrollbar;

	if (w == NULL) return;

	scrollbar = w->data;

	if (scrollMax > 0) scrollbar->scrollMax = scrollMax;
	if (scrollPageSize >= 0) scrollbar->scrollPageSize = min(scrollPageSize, scrollbar->scrollMax);
	if (scrollPosition >= 0) scrollbar->scrollPosition = min(scrollPosition, scrollbar->scrollMax - scrollbar->scrollPageSize);

	GUI_Widget_Scrollbar_CalculateSize(scrollbar);
	GUI_Widget_Scrollbar_CalculatePosition(scrollbar);
}

void
GUI_Widget_Free_WithScrollbar(Widget *w)
{
	if (w == NULL) return;

	free(w->data);
	free(w);
}

/*--------------------------------------------------------------*/

Widget *
Scrollbar_Allocate(Widget *list, enum WindowID parentID, bool set_mentat_widgets)
{
	Widget *scrollbar = GUI_Widget_Allocate_WithScrollbar(15, parentID, 168, 24, 8, 72, NULL);

	Widget *listarea = ScrollListArea_Allocate(scrollbar, parentID);
	list = GUI_Widget_Link(list, listarea);
	list = GUI_Widget_Link(list, scrollbar);

	Widget *scrolldown = GUI_Widget_Allocate3(16, parentID, 168, 96,
			SHAPE_SCROLLBAR_DOWN, SHAPE_SCROLLBAR_DOWN_PRESSED, scrollbar, 1);
	list = GUI_Widget_Link(list, scrolldown);

	Widget *scrollup = GUI_Widget_Allocate3(17, parentID, 168, 16,
			SHAPE_SCROLLBAR_UP, SHAPE_SCROLLBAR_UP_PRESSED, scrollbar, 0);
	list = GUI_Widget_Link(list, scrollup);

	if (set_mentat_widgets) {
		g_widgetMentatScrollbar = scrollbar;
		g_widgetMentatScrollDown = scrolldown;
		g_widgetMentatScrollUp = scrollup;
	}

	return list;
}

ScrollbarItem *
Scrollbar_AllocItem(Widget *w, enum ScrollbarItemType type)
{
	WidgetScrollbar *ws = w->data;
	const int i = ws->scrollMax;

	if (s_scrollbar_max_items <= i + 1) {
		const int new_max = (s_scrollbar_max_items <= 0) ? 16 : 2 * s_scrollbar_max_items;

		s_scrollbar_item = realloc(s_scrollbar_item, new_max * sizeof(s_scrollbar_item[0]));
		s_scrollbar_max_items = new_max;
	}

	ws->scrollMax++;

	ScrollbarItem *si = &s_scrollbar_item[i];
	si->type = type;
	return si;
}

void
Scrollbar_FreeItems(void)
{
	free(s_scrollbar_item);
	s_scrollbar_item = NULL;

	s_scrollbar_max_items = 0;
}

ScrollbarItem *
Scrollbar_GetSelectedItem(const Widget *w)
{
	const WidgetScrollbar *ws = w->data;
	assert(0 <= s_selectedHelpSubject && s_selectedHelpSubject < ws->scrollMax);
	VARIABLE_NOT_USED(ws);

	return &s_scrollbar_item[s_selectedHelpSubject];
}

static void
Scrollbar_Scroll(WidgetScrollbar *scrollbar, uint16 scroll)
{
	scrollbar->scrollPosition += scroll;

	if ((int16)scrollbar->scrollPosition >= scrollbar->scrollMax - scrollbar->scrollPageSize) {
		scrollbar->scrollPosition = scrollbar->scrollMax - scrollbar->scrollPageSize;
	}

	if ((int16)scrollbar->scrollPosition <= 0) scrollbar->scrollPosition = 0;

	GUI_Widget_Scrollbar_CalculatePosition(scrollbar);
}

static void
Scrollbar_Clamp(const WidgetScrollbar *ws)
{
	if (s_selectedHelpSubject < ws->scrollPosition)
		s_selectedHelpSubject = ws->scrollPosition;

	if (s_selectedHelpSubject > ws->scrollPosition + ws->scrollPageSize - 1)
		s_selectedHelpSubject = ws->scrollPosition + ws->scrollPageSize - 1;
}

static void
Scrollbar_SelectUp(Widget *w)
{
	WidgetScrollbar *ws = w->data;

	if (s_selectedHelpSubject <= ws->scrollPosition)
		Scrollbar_Scroll(ws, -1);

	s_selectedHelpSubject--;
	Scrollbar_Clamp(ws);
}

static void
Scrollbar_SelectDown(Widget *w)
{
	WidgetScrollbar *ws = w->data;

	if (s_selectedHelpSubject >= ws->scrollPosition + ws->scrollPageSize - 1)
		Scrollbar_Scroll(ws, 1);

	s_selectedHelpSubject++;
	Scrollbar_Clamp(ws);
}

bool
Scrollbar_ArrowUp_Click(Widget *w)
{
	WidgetScrollbar *ws = w->data;

	Scrollbar_Scroll(ws, -1);
	Scrollbar_Clamp(ws);
	return false;
}

bool
Scrollbar_ArrowDown_Click(Widget *w)
{
	WidgetScrollbar *ws = w->data;

	Scrollbar_Scroll(ws, 1);
	Scrollbar_Clamp(ws);
	return false;
}

void
Scrollbar_HandleEvent(Widget *w, int key)
{
	WidgetScrollbar *ws = w->data;

	switch (key) {
		case 0x80 | MOUSE_ZAXIS:
			if (g_mouseDZ > 0) {
				Scrollbar_ArrowUp_Click(w);
			}
			else if (g_mouseDZ < 0) {
				Scrollbar_ArrowDown_Click(w);
			}
			break;

		case SCANCODE_KEYPAD_8: /* NUMPAD 8 / ARROW UP */
			Scrollbar_SelectUp(w);
			break;

		case SCANCODE_KEYPAD_2: /* NUMPAD 2 / ARROW DOWN */
			Scrollbar_SelectDown(w);
			break;

		case SCANCODE_KEYPAD_9: /* NUMPAD 9 / PAGE UP */
			for (int i = 0; i < ws->scrollPageSize; i++)
				Scrollbar_SelectUp(w);
			break;

		case SCANCODE_KEYPAD_3: /* NUMPAD 3 / PAGE DOWN */
			for (int i = 0; i < ws->scrollPageSize; i++)
				Scrollbar_SelectDown(w);
			break;
	}
}

bool
Scrollbar_Click(Widget *w)
{
	WidgetScrollbar *scrollbar;
	uint16 positionX, positionY;

	scrollbar = w->data;

	positionX = w->offsetX;
	if (w->offsetX < 0) positionX += g_widgetProperties[w->parentID].width;
	positionX += g_widgetProperties[w->parentID].xBase;

	positionY = w->offsetY;
	if (w->offsetY < 0) positionY += g_widgetProperties[w->parentID].height;
	positionY += g_widgetProperties[w->parentID].yBase;

	if ((w->state.s.buttonState & 0x44) != 0) {
		scrollbar->pressed = 0;
	}

	if ((w->state.s.buttonState & 0x11) != 0) {
		int16 positionCurrent;
		int16 positionBegin;
		int16 positionEnd;

		scrollbar->pressed = 0;

		if (w->width > w->height) {
			positionCurrent = g_mouseX;
			positionBegin = positionX + scrollbar->position + 1;
		} else {
			positionCurrent = g_mouseY;
			positionBegin = positionY + scrollbar->position + 1;
		}

		positionEnd = positionBegin + scrollbar->size;

		if (positionCurrent <= positionEnd && positionCurrent >= positionBegin) {
			scrollbar->pressed = 1;
			scrollbar->pressedPosition = positionCurrent - positionBegin;
		} else {
			Scrollbar_Scroll(scrollbar, (positionCurrent < positionBegin ? -scrollbar->scrollPageSize : scrollbar->scrollPageSize));
			Scrollbar_Clamp(scrollbar);
		}
	}

	if ((w->state.s.buttonState & 0x22) != 0 && scrollbar->pressed != 0) {
		int16 position, size;

		if (w->width > w->height) {
			size = w->width - 2 - scrollbar->size;
			position = g_mouseX - scrollbar->pressedPosition - positionX - 1;
		} else {
			size = w->height - 2 - scrollbar->size;
			position = g_mouseY - scrollbar->pressedPosition - positionY - 1;
		}

		if (position < 0) {
			position = 0;
		} else if (position > size) {
			position = size;
		}

		if (scrollbar->position != position) {
			scrollbar->position = position;
			scrollbar->dirty = 1;
		}

		GUI_Widget_Scrollbar_CalculateScrollPosition(scrollbar);
		Scrollbar_Clamp(scrollbar);
	}

	return false;
}

static void
ScrollListArea_Draw(Widget *w)
{
	const ScreenDiv *div = &g_screenDiv[SCREENDIV_MENU];
	const WidgetProperties *wi = &g_widgetProperties[w->parentID];
	const Widget *scrollbar = GUI_Widget_Get_ByIndex(w, 15);
	const WidgetScrollbar *ws = w->data;

	Video_SetClippingArea(div->scalex * wi->xBase + div->x, div->scaley * wi->yBase + div->y,
			div->scalex * scrollbar->offsetX, div->scaley * wi->height);

	for (int i = 0; i < ws->scrollPageSize; i++) {
		const int n = ws->scrollPosition + i;

		if (!(0 <= n && n < ws->scrollMax))
			break;

		const ScrollbarItem *si = &s_scrollbar_item[n];
		const int y = wi->yBase + w->offsetY + 8 * i;
		int x = wi->xBase + w->offsetX;
		uint8 colour;

		if (si->type == SCROLLBAR_CATEGORY) {
			x -= 8;
			colour = 11;
		}
		else {
			colour = (n == s_selectedHelpSubject) ? 8 : 15;
		}

		GUI_DrawText_Wrapper(si->text, x, y, colour, 0, 0x11);
	}

	Video_SetClippingArea(0, 0, TRUE_DISPLAY_WIDTH, TRUE_DISPLAY_HEIGHT);
}

static bool
ScrollListArea_Click(Widget *w)
{
	const WidgetProperties *wi = &g_widgetProperties[w->parentID];
	WidgetScrollbar *ws = w->data;

	if (wi->yBase + w->offsetY <= g_mouseY && g_mouseY < wi->yBase + w->offsetY + w->height) {
		const int y = (g_mouseY - w->offsetY - wi->yBase) / 8;

		if (ws->scrollPosition + y < ws->scrollMax)
			s_selectedHelpSubject = ws->scrollPosition + y;
	}

	if ((w->state.s.buttonState & 0x11) == 0) return true;

	return false;
}

static Widget *
ScrollListArea_Allocate(Widget *scrollbar, enum WindowID parentID)
{
	Widget *w = calloc(1, sizeof(Widget));

	w->index = 3;

	w->flags.all = 0;
	w->flags.s.buttonFilterLeft = 9;
	w->flags.s.buttonFilterRight = 1;

	w->clickProc = &ScrollListArea_Click;

	w->drawParameterNormal.proc = ScrollListArea_Draw;
	w->drawParameterSelected.proc = w->drawParameterNormal.proc;
	w->drawParameterDown.proc = w->drawParameterNormal.proc;
	w->drawModeNormal = DRAW_MODE_CUSTOM_PROC;
	w->drawModeSelected = DRAW_MODE_CUSTOM_PROC;
	w->drawModeDown = DRAW_MODE_CUSTOM_PROC;

	w->state.all = 0;

	w->offsetX = 24;
	w->offsetY = 16;
	w->width = 0x88;
	w->height = 8 * 11;
	w->parentID = parentID;

	w->data = scrollbar->data;

	return w;
}
