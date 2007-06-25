/* $Id$ */

/** @file group_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "command.h"
#include "engine.h"
#include "vehicle_gui.h"
#include "depot.h"
#include "train.h"
#include "date.h"
#include "group.h"
#include "helpers.hpp"
#include "viewport.h"
#include "strings.h"
#include "debug.h"


struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
};

static Sorting _sorting;


static void BuildGroupList(grouplist_d* gl, PlayerID owner, VehicleType vehicle_type)
{
	const Group** list;
	const Group *g;
	uint n = 0;

	if (!(gl->l.flags & VL_REBUILD)) return;

	list = MallocT<const Group*>(GetGroupArraySize());
	if (list == NULL) {
		error("Could not allocate memory for the group-sorting-list");
	}

	FOR_ALL_GROUPS(g) {
		if (g->owner == owner && g->vehicle_type == vehicle_type) list[n++] = g;
	}

	free((void*)gl->sort_list);
	gl->sort_list = MallocT<const Group *>(n);
	if (n != 0 && gl->sort_list == NULL) {
		error("Could not allocate memory for the group-sorting-list");
	}
	gl->l.list_length = n;

	for (uint i = 0; i < n; ++i) gl->sort_list[i] = list[i];
	free((void*)list);

	gl->l.flags &= ~VL_REBUILD;
	gl->l.flags |= VL_RESORT;
}


static int CDECL GroupNameSorter(const void *a, const void *b)
{
	static const Group *last_group[2] = { NULL, NULL };
	static char         last_name[2][64] = { "", "" };

	const Group *ga = *(const Group**)a;
	const Group *gb = *(const Group**)b;
	int r;

	if (ga != last_group[0]) {
		last_group[0] = ga;
		SetDParam(0, ga->index);
		GetString(last_name[0], STR_GROUP_NAME, lastof(last_name[0]));
	}

	if (gb != last_group[1]) {
		last_group[1] = gb;
		SetDParam(0, gb->index);
		GetString(last_name[1], STR_GROUP_NAME, lastof(last_name[1]));
	}

	r = strcmp(last_name[0], last_name[1]); // sort by name

	if (r == 0) return ga->index - gb->index;

	return r;
}


static void SortGroupList(grouplist_d *gl)
{
	if (!(gl->l.flags & VL_RESORT)) return;

	qsort((void*)gl->sort_list, gl->l.list_length, sizeof(gl->sort_list[0]), GroupNameSorter);

	gl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	gl->l.flags &= ~VL_RESORT;
}


enum GroupListWidgets {
	GRP_WIDGET_CLOSEBOX = 0,
	GRP_WIDGET_CAPTION,
	GRP_WIDGET_STICKY,
	GRP_WIDGET_EMPTY_TOP_LEFT,
	GRP_WIDGET_ALL_VEHICLES,
	GRP_WIDGET_LIST_GROUP,
	GRP_WIDGET_LIST_GROUP_SCROLLBAR,
	GRP_WIDGET_SORT_BY_ORDER,
	GRP_WIDGET_SORT_BY_TEXT,
	GRP_WIDGET_SORT_BY_DROPDOWN,
	GRP_WIDGET_EMPTY_TOP_RIGHT,
	GRP_WIDGET_LIST_VEHICLE,
	GRP_WIDGET_LIST_VEHICLE_SCROLLBAR,
	GRP_WIDGET_CREATE_GROUP,
	GRP_WIDGET_DELETE_GROUP,
	GRP_WIDGET_RENAME_GROUP,
	GRP_WIDGET_EMPTY1,
	GRP_WIDGET_REPLACE_PROTECTION,
	GRP_WIDGET_EMPTY2,
	GRP_WIDGET_AVAILABLE_VEHICLES,
	GRP_WIDGET_MANAGE_VEHICLES,
	GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	GRP_WIDGET_STOP_ALL,
	GRP_WIDGET_START_ALL,
	GRP_WIDGET_EMPTY_BOTTOM_RIGHT,
	GRP_WIDGET_RESIZE,
};


static const Widget _group_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,             STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   513,     0,    13, 0x0,                  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   514,   525,     0,    13, 0x0,                  STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   200,    14,    25, 0x0,                  STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   200,    26,    39, 0x0,                  STR_NULL},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   188,    39,   220, 0x701,                STR_GROUPS_CLICK_ON_GROUP_FOR_TIP},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   189,   200,    26,   220, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   201,   281,    14,    25, STR_SORT_BY,          STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,   282,   435,    14,    25, 0x0,                  STR_SORT_CRITERIA_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   436,   447,    14,    25, STR_0225,             STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   448,   525,    14,    25, 0x0,                  STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,   201,   513,    26,   233, 0x701,                STR_NULL},
{ WWT_SCROLL2BAR,    RESIZE_LRB,    14,   514,   525,    26,   233, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,     0,    23,   221,   245, 0x0,                  STR_GROUP_CREATE_TIP},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,    24,    47,   221,   245, 0x0,                  STR_GROUP_DELETE_TIP},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,    48,    71,   221,   245, 0x0,                  STR_GROUP_RENAME_TIP},
{      WWT_PANEL,     RESIZE_TB,    14,    72,   164,   221,   245, 0x0,                  STR_NULL},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,   165,   188,   221,   245, 0x0,                  STR_GROUP_REPLACE_PROTECTION_TIP},
{      WWT_PANEL,     RESIZE_TB,    14,   189,   200,   221,   245, 0x0,                  STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   201,   306,   234,   245, 0x0,                  STR_AVAILABLE_ENGINES_TIP},
{    WWT_TEXTBTN,     RESIZE_TB,    14,   307,   411,   234,   245, STR_MANAGE_LIST,      STR_MANAGE_LIST_TIP},
{    WWT_TEXTBTN,     RESIZE_TB,    14,   412,   423,   234,   245, STR_0225,             STR_MANAGE_LIST_TIP},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,   424,   435,   234,   245, SPR_FLAG_VEH_STOPPED, STR_MASS_STOP_LIST_TIP},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,   436,   447,   234,   245, SPR_FLAG_VEH_RUNNING, STR_MASS_START_LIST_TIP},
{      WWT_PANEL,    RESIZE_RTB,    14,   448,   513,   234,   245, 0x0,                  STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   514,   525,   234,   245, 0x0,                  STR_RESIZE_BUTTON},
{   WIDGETS_END},
};


static void CreateVehicleGroupWindow(Window *w)
{
	const PlayerID owner = (PlayerID)GB(w->window_number, 0, 8);
	groupveh_d *gv = &WP(w, groupveh_d);
	grouplist_d *gl = &WP(w, groupveh_d).gl;

	w->caption_color = owner;
	w->hscroll.cap = 10 * 29;
	w->resize.step_width = 1;

	switch (gv->vehicle_type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
		case VEH_ROAD:
			w->vscroll.cap = 14;
			w->vscroll2.cap = 8;
			w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_SMALL;
			break;
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			w->vscroll.cap = 10;
			w->vscroll2.cap = 4;
			w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_BIG2;
			break;
	}

	w->widget[GRP_WIDGET_LIST_GROUP].data = (w->vscroll.cap << 8) + 1;
	w->widget[GRP_WIDGET_LIST_VEHICLE].data = (w->vscroll2.cap << 8) + 1;

	switch (gv->vehicle_type) {
		default: NOT_REACHED(); break;
		case VEH_TRAIN:    gv->_sorting = &_sorting.train;    break;
		case VEH_ROAD:     gv->_sorting = &_sorting.roadveh;  break;
		case VEH_SHIP:     gv->_sorting = &_sorting.ship;     break;
		case VEH_AIRCRAFT: gv->_sorting = &_sorting.aircraft; break;
	}

	gv->sort_list = NULL;
	gv->vehicle_type = (VehicleType)GB(w->window_number, 11, 5);
	gv->l.sort_type = gv->_sorting->criteria;
	gv->l.flags = VL_REBUILD | (gv->_sorting->order ? VL_DESC : VL_NONE);
	gv->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;	// Set up resort timer

	gl->sort_list = NULL;
	gl->l.flags = VL_REBUILD | VL_NONE;
	gl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;	// Set up resort timer

	gv->group_sel = DEFAULT_GROUP;

	switch (gv->vehicle_type) {
		case VEH_TRAIN:
			w->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_883D_TRAINS_CLICK_ON_TRAIN_FOR;
			w->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_TRAINS;

			w->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_TRAIN;
			w->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_TRAIN;
			w->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_TRAIN;
			break;

		case VEH_ROAD:
			w->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_901A_ROAD_VEHICLES_CLICK_ON;
			w->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_ROAD_VEHICLES;

			w->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_ROADVEH;
			w->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_ROADVEH;
			w->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_ROADVEH;
			break;

		case VEH_SHIP:
			w->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_9823_SHIPS_CLICK_ON_SHIP_FOR;
			w->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_SHIPS;

			w->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_SHIP;
			w->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_SHIP;
			w->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_SHIP;
			break;

		case VEH_AIRCRAFT:
			w->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT;
			w->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_AIRCRAFT;

			w->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_AIRCRAFT;
			w->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_AIRCRAFT;
			w->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_AIRCRAFT;
			break;

		default: NOT_REACHED();
	}
}

/**
 * Update/redraw the group action dropdown
 * @param w   the window the dropdown belongs to
 * @param gid the currently selected group in the window
 */
static void UpdateGroupActionDropdown(Window *w, GroupID gid, bool refresh = true)
{
	if (refresh && !IsWindowWidgetLowered(w, GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN)) return;

	static StringID action_str[] = {
		STR_REPLACE_VEHICLES,
		STR_SEND_FOR_SERVICING,
		STR_SEND_TRAIN_TO_DEPOT,
		STR_NULL,
		STR_NULL,
		INVALID_STRING_ID
	};

	action_str[3] = IsDefaultGroupID(gid) ? INVALID_STRING_ID : STR_GROUP_ADD_SHARED_VEHICLE;
	action_str[4] = IsDefaultGroupID(gid) ? INVALID_STRING_ID : STR_GROUP_REMOVE_ALL_VEHICLES;

	ShowDropDownMenu(w, action_str, 0, GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN, 0, 0);
}

/**
 * bitmask for w->window_number
 * 0-7   PlayerID (owner)
 * 11-15 vehicle type
 **/
static void GroupWndProc(Window *w, WindowEvent *e)
{
	const PlayerID owner = (PlayerID)GB(w->window_number, 0, 8);
	const Player *p = GetPlayer(owner);
	groupveh_d *gv = &WP(w, groupveh_d);
	grouplist_d *gl = &WP(w, groupveh_d).gl;

	gv->vehicle_type = (VehicleType)GB(w->window_number, 11, 5);

	switch(e->event) {
		case WE_INVALIDATE_DATA:
			gv->l.flags |= VL_REBUILD;
			gl->l.flags |= VL_REBUILD;
			UpdateGroupActionDropdown(w, gv->group_sel);
			SetWindowDirty(w);
			break;

		case WE_CREATE:
			CreateVehicleGroupWindow(w);
			break;

		case WE_PAINT: {
			int x = 203;
			int y2 = PLY_WND_PRC__OFFSET_TOP_WIDGET;
			int y1 = PLY_WND_PRC__OFFSET_TOP_WIDGET + 2;
			int max;
			int i;

			/* If we select the default group, gv->list will contain all vehicles of the player
			 * else gv->list will contain all vehicles which belong to the selected group */
			BuildVehicleList(gv, owner, gv->group_sel, IsDefaultGroupID(gv->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST);
			SortVehicleList(gv);


			BuildGroupList(gl, owner, gv->vehicle_type);
			SortGroupList(gl);

			SetVScrollCount(w, gl->l.list_length);
			SetVScroll2Count(w, gv->l.list_length);

			/* Disable all lists management button when the list is empty */
			SetWindowWidgetsDisabledState(w, gv->l.list_length == 0,
					GRP_WIDGET_STOP_ALL,
					GRP_WIDGET_START_ALL,
					GRP_WIDGET_MANAGE_VEHICLES,
					GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN,
					WIDGET_LIST_END);

			/* Disable the group specific function when we select the default group */
			SetWindowWidgetsDisabledState(w, IsDefaultGroupID(gv->group_sel),
					GRP_WIDGET_DELETE_GROUP,
					GRP_WIDGET_RENAME_GROUP,
					GRP_WIDGET_REPLACE_PROTECTION,
					WIDGET_LIST_END);

			/* If selected_group == DEFAULT_GROUP, draw the standard caption
			   We list all vehicles */
			if (IsDefaultGroupID(gv->group_sel)) {
				SetDParam(0, p->name_1);
				SetDParam(1, p->name_2);
				SetDParam(2, gv->l.list_length);

				switch (gv->vehicle_type) {
					case VEH_TRAIN:
						w->widget[GRP_WIDGET_CAPTION].data = STR_881B_TRAINS;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_TRAIN;
						break;
					case VEH_ROAD:
						w->widget[GRP_WIDGET_CAPTION].data = STR_9001_ROAD_VEHICLES;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_ROADVEH;
						break;
					case VEH_SHIP:
						w->widget[GRP_WIDGET_CAPTION].data = STR_9805_SHIPS;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_SHIP;
						break;
					case VEH_AIRCRAFT:
						w->widget[GRP_WIDGET_CAPTION].data =  STR_A009_AIRCRAFT;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_AIRCRAFT;
						break;
					default: NOT_REACHED(); break;
				}
			} else {
				const Group *g = GetGroup(gv->group_sel);

				SetDParam(0, g->index);
				SetDParam(1, g->num_vehicle);

				switch (gv->vehicle_type) {
					case VEH_TRAIN:
						w->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_TRAINS_CAPTION;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_TRAIN : SPR_GROUP_REPLACE_OFF_TRAIN;
						break;
					case VEH_ROAD:
						w->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_ROADVEH_CAPTION;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_ROADVEH : SPR_GROUP_REPLACE_OFF_ROADVEH;
						break;
					case VEH_SHIP:
						w->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_SHIPS_CAPTION;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_SHIP : SPR_GROUP_REPLACE_OFF_SHIP;
						break;
					case VEH_AIRCRAFT:
						w->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_AIRCRAFTS_CAPTION;
						w->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_AIRCRAFT : SPR_GROUP_REPLACE_OFF_AIRCRAFT;
						break;
					default: NOT_REACHED(); break;
				}
			}


			DrawWindowWidgets(w);

			/* Draw Matrix Group
			 * The selected group is drawn in white */
			StringID str;

			switch (gv->vehicle_type) {
				case VEH_TRAIN:    str = STR_GROUP_ALL_TRAINS;    break;
				case VEH_ROAD:     str = STR_GROUP_ALL_ROADS;     break;
				case VEH_SHIP:     str = STR_GROUP_ALL_SHIPS;     break;
				case VEH_AIRCRAFT: str = STR_GROUP_ALL_AIRCRAFTS; break;
				default: NOT_REACHED(); break;
			}
			DrawString(10, y1, str, IsDefaultGroupID(gv->group_sel) ? 12 : 16);

			max = min(w->vscroll.pos + w->vscroll.cap, gl->l.list_length);
			for (i = w->vscroll.pos ; i < max ; ++i) {
				const Group *g = gl->sort_list[i];

				assert(g->owner == owner);

				y1 += PLY_WND_PRC__SIZE_OF_ROW_TINY;

				/* draw the selected group in white, else we draw it in black */
				SetDParam(0, g->index);
				DrawString(10, y1, STR_GROUP_NAME, (gv->group_sel == g->index) ? 12 : 16);

				/* draw the number of vehicles of the group */
				SetDParam(0, g->num_vehicle);
				DrawStringRightAligned(187, y1 + 1, STR_GROUP_TINY_NUM, (gv->group_sel == g->index) ? 12 : 16);
			}

			/* Draw Matrix Vehicle according to the vehicle list built before */
			DrawString(285, 15, _vehicle_sort_listing[gv->l.sort_type], 0x10);
			DoDrawString(gv->l.flags & VL_DESC ? DOWNARROW : UPARROW, 269, 15, 0x10);

			max = min(w->vscroll2.pos + w->vscroll2.cap, gv->l.list_length);
			for (i = w->vscroll2.pos ; i < max ; ++i) {
				const Vehicle* v = gv->sort_list[i];
				StringID str;

				assert(v->type == gv->vehicle_type && v->owner == owner);

				DrawVehicleImage(v, x + 19, y2 + 6, w->hscroll.cap, 0, gv->vehicle_sel);
				DrawVehicleProfitButton(v, x, y2 + 13);

				if (IsVehicleInDepot(v)) {
					str = STR_021F;
				} else {
					str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
				}
				SetDParam(0, v->unitnumber);
				DrawString(x, y2 + 2, str, 0);

				if (w->resize.step_height == PLY_WND_PRC__SIZE_OF_ROW_BIG2) DrawSmallOrderList(v, x + 138, y2);

				if (v->profit_this_year < 0) {
					str = v->profit_last_year < 0 ?
							STR_PROFIT_BAD_THIS_YEAR_BAD_LAST_YEAR :
							STR_PROFIT_BAD_THIS_YEAR_GOOD_LAST_YEAR;
				} else {
					str = v->profit_last_year < 0 ?
							STR_PROFIT_GOOD_THIS_YEAR_BAD_LAST_YEAR :
							STR_PROFIT_GOOD_THIS_YEAR_GOOD_LAST_YEAR;
				}

				SetDParam(0, v->profit_this_year);
				SetDParam(1, v->profit_last_year);
				DrawString(x + 19, y2 + w->resize.step_height - 8, str, 0);

				if (IsValidGroupID(v->group_id)) {
					SetDParam(0, v->group_id);
					DrawString(x + 19, y2, STR_GROUP_TINY_NAME, 16);
				}

				y2 += w->resize.step_height;
			}

			break;
		}

		case WE_CLICK:
			switch(e->we.click.widget) {
				case GRP_WIDGET_SORT_BY_ORDER: // Flip sorting method ascending/descending
					gv->l.flags ^= VL_DESC;
					gv->l.flags |= VL_RESORT;

					gv->_sorting->order = !!(gv->l.flags & VL_DESC);
					SetWindowDirty(w);
					break;

				case GRP_WIDGET_SORT_BY_TEXT:
				case GRP_WIDGET_SORT_BY_DROPDOWN: // Select sorting criteria dropdown menu
					ShowDropDownMenu(w, _vehicle_sort_listing, gv->l.sort_type,  GRP_WIDGET_SORT_BY_DROPDOWN, 0, 0);
					return;

				case GRP_WIDGET_ALL_VEHICLES: // All vehicles button
					if (!IsDefaultGroupID(gv->group_sel)) {
						gv->group_sel = DEFAULT_GROUP;
						gv->l.flags |= VL_REBUILD;
						UpdateGroupActionDropdown(w, gv->group_sel);
						SetWindowDirty(w);
					}
					break;

				case GRP_WIDGET_LIST_GROUP: { // Matrix Group
					uint16 id_g = (e->we.click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET - 13) / PLY_WND_PRC__SIZE_OF_ROW_TINY;

					if (id_g >= w->vscroll.cap) return;

					id_g += w->vscroll.pos;

					if (id_g >= gl->l.list_length) return;

					gv->group_sel = gl->sort_list[id_g]->index;;

					gv->l.flags |= VL_REBUILD;
					UpdateGroupActionDropdown(w, gv->group_sel);
					SetWindowDirty(w);
					break;
				}

				case GRP_WIDGET_LIST_VEHICLE: { // Matrix Vehicle
					uint32 id_v = (e->we.click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / (int)w->resize.step_height;
					const Vehicle *v;

					if (id_v >= w->vscroll2.cap) return; // click out of bounds

					id_v += w->vscroll2.pos;

					if (id_v >= gv->l.list_length) return; // click out of list bound

					v = gv->sort_list[id_v];

					gv->vehicle_sel = v->index;

					if (IsValidVehicle(v)) {
						CursorID image;

						switch (gv->vehicle_type) {
							case VEH_TRAIN:    image = GetTrainImage(v, DIR_W);    break;
							case VEH_ROAD:     image = GetRoadVehImage(v, DIR_W);  break;
							case VEH_SHIP:     image = GetShipImage(v, DIR_W);     break;
							case VEH_AIRCRAFT: image = GetAircraftImage(v, DIR_W); break;
							default: NOT_REACHED(); break;
						}

						SetObjectToPlaceWnd(image, GetVehiclePalette(v), 4, w);
					}

					SetWindowDirty(w);
					break;
				}

				case GRP_WIDGET_CREATE_GROUP: // Create a new group
					DoCommandP(0, gv->vehicle_type, 0, NULL, CMD_CREATE_GROUP | CMD_MSG(STR_GROUP_CAN_T_CREATE));
					break;

				case GRP_WIDGET_DELETE_GROUP: { // Delete the selected group
					GroupID group = gv->group_sel;
					gv->group_sel = DEFAULT_GROUP;

					DoCommandP(0, group, 0, NULL, CMD_DELETE_GROUP | CMD_MSG(STR_GROUP_CAN_T_DELETE));
					break;
				}

				case GRP_WIDGET_RENAME_GROUP: { // Rename the selected roup
					assert(!IsDefaultGroupID(gv->group_sel));

					const Group *g = GetGroup(gv->group_sel);

					SetDParam(0, g->index);
					ShowQueryString(STR_GROUP_NAME, STR_GROUP_RENAME_CAPTION, 31, 150, w, CS_ALPHANUMERAL);
				}	break;


				case GRP_WIDGET_AVAILABLE_VEHICLES:
					ShowBuildVehicleWindow(0, gv->vehicle_type);
					break;

				case GRP_WIDGET_MANAGE_VEHICLES:
				case GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN:  {
					UpdateGroupActionDropdown(w, gv->group_sel, false);
					break;
				}


				case GRP_WIDGET_START_ALL:
				case GRP_WIDGET_STOP_ALL: { // Start/stop all vehicles of the list
					DoCommandP(0, gv->group_sel, ((IsDefaultGroupID(gv->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST) & VLW_MASK)
														| (1 << 6)
														| (e->we.click.widget == GRP_WIDGET_START_ALL ? (1 << 5) : 0)
														| gv->vehicle_type, NULL, CMD_MASS_START_STOP);

					break;
				}

				case GRP_WIDGET_REPLACE_PROTECTION:
					if (!IsDefaultGroupID(gv->group_sel)) {
						const Group *g = GetGroup(gv->group_sel);

						DoCommandP(0, gv->group_sel, !g->replace_protection, NULL, CMD_SET_GROUP_REPLACE_PROTECTION);
					}
					break;
			}

			break;

		case WE_DRAGDROP: {
			switch (e->we.click.widget) {
				case GRP_WIDGET_ALL_VEHICLES: // All trains
					DoCommandP(0, DEFAULT_GROUP, gv->vehicle_sel, NULL, CMD_ADD_VEHICLE_GROUP | CMD_MSG(STR_GROUP_CAN_T_ADD_VEHICLE));

					gv->vehicle_sel = INVALID_VEHICLE;

					SetWindowDirty(w);

					break;

				case GRP_WIDGET_LIST_GROUP: { // Maxtrix group
					uint16 id_g = (e->we.click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET - 13) / PLY_WND_PRC__SIZE_OF_ROW_TINY;
					const VehicleID vindex = gv->vehicle_sel;

					gv->vehicle_sel = INVALID_VEHICLE;

					SetWindowDirty(w);

					if (id_g >= w->vscroll.cap) return;

					id_g += w->vscroll.pos;

					if (id_g >= gl->l.list_length) return;

					DoCommandP(0, gl->sort_list[id_g]->index, vindex, NULL, CMD_ADD_VEHICLE_GROUP | CMD_MSG(STR_GROUP_CAN_T_ADD_VEHICLE));

					break;
				}

				case GRP_WIDGET_LIST_VEHICLE: { // Maxtrix vehicle
					uint32 id_v = (e->we.click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / (int)w->resize.step_height;
					const Vehicle *v;
					const VehicleID vindex = gv->vehicle_sel;

					gv->vehicle_sel = INVALID_VEHICLE;

					SetWindowDirty(w);

					if (id_v >= w->vscroll2.cap) return; // click out of bounds

					id_v += w->vscroll2.pos;

					if (id_v >= gv->l.list_length) return; // click out of list bound

					v = gv->sort_list[id_v];

					if (vindex == v->index) {
						switch (gv->vehicle_type) {
							default: NOT_REACHED(); break;
							case VEH_TRAIN:    ShowTrainViewWindow(v);    break;
							case VEH_ROAD:     ShowRoadVehViewWindow(v);  break;
							case VEH_SHIP:     ShowShipViewWindow(v);     break;
							case VEH_AIRCRAFT: ShowAircraftViewWindow(v); break;
						}
					}

					break;
				}
			}
			break;
		}

		case WE_ON_EDIT_TEXT:
			if (!StrEmpty(e->we.edittext.str)) {
				_cmd_text = e->we.edittext.str;

				DoCommandP(0, gv->group_sel, 0, NULL, CMD_RENAME_GROUP | CMD_MSG(STR_GROUP_CAN_T_RENAME));
			}
			break;

		case WE_RESIZE:
			w->hscroll.cap += e->we.sizing.diff.x;
			w->vscroll.cap += e->we.sizing.diff.y / PLY_WND_PRC__SIZE_OF_ROW_TINY;
			w->vscroll2.cap += e->we.sizing.diff.y / (int)w->resize.step_height;

			w->widget[GRP_WIDGET_LIST_GROUP].data = (w->vscroll.cap << 8) + 1;
			w->widget[GRP_WIDGET_LIST_VEHICLE].data = (w->vscroll2.cap << 8) + 1;
			break;


		case WE_DROPDOWN_SELECT: // we have selected a dropdown item in the list
			switch (e->we.dropdown.button) {
				case GRP_WIDGET_SORT_BY_DROPDOWN:
					if (gv->l.sort_type != e->we.dropdown.index) {
						gv->l.flags |= VL_RESORT;
						gv->l.sort_type = e->we.dropdown.index;
						gv->_sorting->criteria = gv->l.sort_type;
					}
					break;

				case GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN:
					assert(gv->l.list_length != 0);

					switch (e->we.dropdown.index) {
						case 0: // Replace window
							ShowReplaceGroupVehicleWindow(gv->group_sel, gv->vehicle_type);
							break;
						case 1: // Send for servicing
							DoCommandP(0, gv->group_sel, ((IsDefaultGroupID(gv->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST) & VLW_MASK)
										| DEPOT_MASS_SEND
										| DEPOT_SERVICE, NULL, GetCmdSendToDepot(gv->vehicle_type));
							break;
						case 2: // Send to Depots
							DoCommandP(0, gv->group_sel, ((IsDefaultGroupID(gv->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST) & VLW_MASK)
										| DEPOT_MASS_SEND, NULL, GetCmdSendToDepot(gv->vehicle_type));
							break;
						case 3: // Add shared Vehicles
							assert(!IsDefaultGroupID(gv->group_sel));

							DoCommandP(0, gv->group_sel, gv->vehicle_type, NULL, CMD_ADD_SHARED_VEHICLE_GROUP | CMD_MSG(STR_GROUP_CAN_T_ADD_SHARED_VEHICLE));
							break;
						case 4: // Remove all Vehicles from the selected group
							assert(!IsDefaultGroupID(gv->group_sel));

							DoCommandP(0, gv->group_sel, gv->vehicle_type, NULL, CMD_REMOVE_ALL_VEHICLES_GROUP | CMD_MSG(STR_GROUP_CAN_T_REMOVE_ALL_VEHICLES));
							break;
						default: NOT_REACHED();
					}
					break;

				default: NOT_REACHED();
			}

			SetWindowDirty(w);
			break;


		case WE_DESTROY:
			free((void*)gv->sort_list);
			free((void*)gl->sort_list);
			break;


		case WE_TICK: // resort the lists every 20 seconds orso (10 days)
			if (--gv->l.resort_timer == 0) {
				gv->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
				gv->l.flags |= VL_RESORT;
				SetWindowDirty(w);
			}
			if (--gl->l.resort_timer == 0) {
				gl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
				gl->l.flags |= VL_RESORT;
				SetWindowDirty(w);
			}
			break;
		}
}


static const WindowDesc _group_desc = {
	WDP_AUTO, WDP_AUTO, 526, 246,
	WC_TRAINS_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_group_widgets,
	GroupWndProc
};

void ShowPlayerGroup(PlayerID player, VehicleType vehicle_type)
{
	WindowClass wc;

	switch (vehicle_type) {
		default: NOT_REACHED();
		case VEH_TRAIN:    wc = WC_TRAINS_LIST;   break;
		case VEH_ROAD:     wc = WC_ROADVEH_LIST;  break;
		case VEH_SHIP:     wc = WC_SHIPS_LIST;    break;
		case VEH_AIRCRAFT: wc = WC_AIRCRAFT_LIST; break;
	}

	WindowNumber num = (vehicle_type << 11) | VLW_GROUP_LIST | player;
	DeleteWindowById(wc, num);
	Window *w = AllocateWindowDescFront(&_group_desc, num);
	if (w == NULL) return;

	w->window_class = wc;

	switch (vehicle_type) {
		default: NOT_REACHED();
		case VEH_ROAD:
			ResizeWindow(w, -66,   0);
			/* FALL THROUGH */
		case VEH_TRAIN:
			w->resize.height = w->height - (PLY_WND_PRC__SIZE_OF_ROW_SMALL * 4); // Minimum of 4 vehicles
			break;

		case VEH_SHIP:
		case VEH_AIRCRAFT:
			ResizeWindow(w, -66, -52);
			w->resize.height = w->height;  // Minimum of 4 vehicles
			break;
	}

	/* Set the minimum window size to the current window size */
	w->resize.width = w->width;
}
