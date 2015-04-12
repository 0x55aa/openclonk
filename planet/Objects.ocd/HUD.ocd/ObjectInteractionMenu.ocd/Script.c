/**
	ObjectInteractionMenu
	Handles the inventory exchange and general interaction between the player and buildings, vehicles etc.

	@author Zapper
*/

local Name = "$Name$";
local Description = "$Description$";

static const InteractionMenu_SideBarSize = 80; // in tenth-em

static const InteractionMenu_Contents = 2;
static const InteractionMenu_Custom = 4;

local current_objects;

/*
	current_menus is an array with two fields
	each field contain a proplist with the attributes:
		target: the target object, needs to be in current_objects
		menu_object: target of the menu (usually a dummy object) (the ID is always 1; the menu's sidebar has the ID 2)
		menu_id
		forced: (boolean) Whether the menu was forced-open (f.e. by an extra-slot object) and is not necessarily attached to an outside-world object.
			Such an object might be removed from the list when the menu is closed.
		menus: array with more proplists with the following attributes:
			flag: bitwise flag (needed for content-menus f.e.)
			title
			decoration: ID of a menu decoration definition
			Priority: priority of the menu (Y position)
			BackgroundColor: background color of the menu
			callback: function called when an entry is selected, the function is passed the symbol of an entry and the user data
			callback_hover: function called when hovering over an entry, the function is passed everything "callback" gets plus the target of the description box menu
			callback_target: object to which the callback is made, ususally the target object (except for contents menus)
			menu_object: MenuStyle_Grid object, used to add/remove entries later
			entry_index_count: used to generate unique IDs for the entries
			entries_callback: (callback) function that can be used to retrieve a list of entries for that menu (at any point - it might also be called later)
				This callback should return an array of entries shown in the menu, the entries are proplists with the following attributes:
				symbol: icon of the item
				extra_data: custom user data (internal: in case of inventory menus this is a proplist containing some extra data (f.e. the one object for unstackable objects))
				text: text shown on the object (f.e. count in inventory)
				custom (optional): completely custom menu entry that is passed to the grid menu - allows for custom design
				unique_index: generated from entry_index_count (not set by user)
			entries: last result of the callback function described above
*/
local current_menus;
/*
	current_description_box contains information about the description box at the bottom of the menu:
		target: target object of the description box
		symbol_target: target object of the symbol sub-box
		desc_target: target object of the description sub-box
*/
local current_description_box;
// this is the ID of the root window that contains the other subwindows (i.e. the menus which contain the sidebars and the interaction-menu)
local current_main_menu_id;
// This holds the dummy object that is the target of the center column ("move all left/right").
local current_center_column_target;
// The Clonk who the menu was opened for.
local cursor;

public func Close() { return RemoveObject(); }
public func IsContentMenu() { return true; }
public func Show() { this.Visibility = VIS_Owner; return true; }
public func Hide() { this.Visibility = VIS_None; return true; }

func Construction()
{
	current_objects = [];
	current_menus = [];
	current_description_box = {target=nil};
}

func Destruction()
{
	// we need only to remove the top-level menu target of the open menus, since the submenus close due to a clever use of OnClose!
	for (var menu in current_menus)
	{
		if (menu && menu.menu_object)
			menu.menu_object->RemoveObject();
	}
	// remove all remaining contained dummy objects to prevent script warnings about objects in removed containers
	var i = ContentsCount(), obj = nil;
	while (obj = Contents(--i))
		obj->RemoveObject(false);
}

// used as a static function
func CreateFor(object cursor)
{
	var obj = CreateObject(GUI_ObjectInteractionMenu, AbsX(0), AbsY(0), cursor->GetOwner());
	obj.Visibility = VIS_Owner;
	obj->Init(cursor);
	cursor->SetMenu(obj);
	return obj;
}


func Init(object cursor)
{
	this.cursor = cursor;
	var checking_effect = AddEffect("IntCheckObjects", cursor, 1, 10, this);
	// Notify the Clonk. This can be used to create custom entries in the objects list via helper objects. For example the "Your Environment" tab.
	// Note that the cursor is NOT informed when the menu is closed again. Helper objects can be attached to the livetime of this menu by, f.e., effects.
	cursor->~OnInteractionMenuOpen(this);
	// And then quickly refresh for the very first time. Successive refreshs will be only every 10 frames.
	EffectCall(cursor, checking_effect, "Timer");
}

func FxIntCheckObjectsStart(target, effect, temp)
{
	if (temp) return;
	EffectCall(target, effect, "Timer");
}

func FxIntCheckObjectsTimer(target, effect, timer)
{
	var new_objects = FindObjects(Find_AtPoint(target->GetX(), target->GetY()), Find_NoContainer(),
		// Find only vehicles and structures (plus Clonks ("livings") and helper objects). This makes sure that no C4D_Object-containers (extra slot) are shown.
		Find_Or(Find_Category(C4D_Vehicle), Find_Category(C4D_Structure), Find_OCF(OCF_Alive), Find_ActionTarget(target)),
		// But these objects still need to either be a container or provide an own interaction menu.
		Find_Or(Find_Func("IsContainer"), Find_Func("HasInteractionMenu")));
	var equal = GetLength(new_objects) == GetLength(current_objects);
	
	if (equal)
	{
		for (var i = GetLength(new_objects) - 1; i >= 0; --i)
		{
			if (new_objects[i] == current_objects[i]) continue;
			equal = false;
			break;
		}
	}
	if (!equal)
		UpdateObjects(new_objects);
}

// updates the objects shown in the side bar
// if an object which is in the menu on the left or on the right is not in the side bar anymore, another object is selected
func UpdateObjects(array new_objects)
{
	// need to close a menu?
	for (var i = 0; i < GetLength(current_menus); ++i)
	{
		if (!current_menus[i]) continue; // todo: I don't actually know why this can happen.
		if (current_menus[i].forced) continue;
		
		var target = current_menus[i].target;
		var found = false;
		for (var obj in new_objects)
		{
			if (target != obj) continue;
			found = true;
			break;
		}
		if (found) continue;
		// not found? close!
		// sub menus close automatically (and remove their dummy) due to a clever usage of OnClose
		current_menus[i].menu_object->RemoveObject();
		current_menus[i] = nil;
	}
	
	current_objects = new_objects;
	
	// need to fill an empty menu slot?
	for (var i = 0; i < 2; ++i)
	{
		// If the menu already exists, just update the sidebar.
		if (current_menus[i] != nil)
		{
			RefreshSidebar(i);
			continue;
		}
		// look for next object to fill slot
		for (var obj in current_objects)
		{
			// but only if the object's menu is not already open
			var is_already_open = false;
			for (var menu in current_menus)
			{
				if (!menu) continue; // todo: I don't actually know why that can happen.
				if (menu.target != obj) continue;
				is_already_open = true;
				break;
			}
			if (is_already_open) continue;
			// use object to create a new menu at that slot
			OpenMenuForObject(obj, i);
			break;
		}
	}
}

func FxIntCheckObjectsStop(target, effect, reason, temp)
{
	if (temp) return;
	if (this)
		this->RemoveObject();
}

/*
	This is the entry point.
	Create a menu for an object (usually from current_objects) and also create everything around it if it's the first time a menu is opened.
*/
func OpenMenuForObject(object obj, int slot, bool forced)
{
	forced = forced ?? false;
	// clean up old menu
	var old_menu = current_menus[slot];
	if (old_menu)
		old_menu.menu_object->RemoveObject();
	current_menus[slot] = nil;
	// before creating the sidebar, we have to create a new entry in current_menus, even if it contains not all information
	current_menus[slot] = {target = obj, forced = forced};
	// clean up old inventory-check effects that are not needed anymore
	var effect_index = 0, inv_effect = nil;
	while (inv_effect = GetEffect("IntRefreshContentsMenu", this, effect_index))
	{
		if (inv_effect.obj != current_menus[0].target && inv_effect.obj != current_menus[1].target)
			RemoveEffect(nil, nil, inv_effect);
		else
			++effect_index;
	}
	// Create a menu with all interaction possibilities for an object.
	// Always create the side bar AFTER the main menu, so that the target can be copied.
	var main = CreateMainMenu(obj, slot);
	// To close the part_menu automatically when the main menu is closed. The sidebar will use this target, too.
	current_menus[slot].menu_object = main.Target;
	// Now, the sidebar.
	var sidebar = CreateSideBar(slot);
	
	var sidebar_size_em = ToEmString(InteractionMenu_SideBarSize);
	var part_menu =
	{
		Left = "0%", Right = "50%-2em",
		Bottom = "100%-14em",
		sidebar = sidebar, main = main,
		Target = current_menus[slot].menu_object,
		ID = 1
	};
	
	if (slot == 1)
	{
		part_menu.Left = "50%+2em";
		part_menu.Right = "100%";
	}
	

	// need to open a completely new menu?
	if (!current_main_menu_id)
	{
		if (!current_description_box.target)
		{
			current_description_box.target = CreateDummy();
			current_description_box.symbol_target = CreateDummy();
			current_description_box.desc_target = CreateDummy();
		}
		if (!current_center_column_target)
			current_center_column_target = CreateDummy();
		
		var root_menu =
		{
			_one_part = part_menu,
			Target = this,
			Decoration = GUI_MenuDeco,
			BackgroundColor = RGB(0, 0, 0),
			center_column =
			{
				Left = "50%-2em",
				Right = "50%+2em",
				Top = "1.75em",
				Bottom = "100%-14em",
				Style = GUI_VerticalLayout,
				move_all_left =
				{
					Target = current_center_column_target,
					ID = 10 + 0,
					Right = "4em", Bottom = "6em",
					Style = GUI_TextHCenter | GUI_TextVCenter,
					Symbol = Icon_MoveItems, GraphicsName = "Left",
					Tooltip = "",
					BackgroundColor ={Std = 0, Hover = RGBa(255, 0, 0, 100)},
					OnMouseIn = GuiAction_SetTag("Hover"),
					OnMouseOut = GuiAction_SetTag("Std"),
					OnClick = GuiAction_Call(this, "OnMoveAllToClicked", 0)
				},
				move_all_right =
				{
					Target = current_center_column_target,
					ID = 10 + 1,
					Right = "4em", Bottom = "6em",
					Style = GUI_TextHCenter | GUI_TextVCenter,
					Symbol = Icon_MoveItems,
					Tooltip = "",
					BackgroundColor ={Std = 0, Hover = RGBa(255, 0, 0, 100)},
					OnMouseIn = GuiAction_SetTag("Hover"),
					OnMouseOut = GuiAction_SetTag("Std"),
					OnClick = GuiAction_Call(this, "OnMoveAllToClicked", 1)
				}
			},
			description_box =
			{
				Top = "100%-10em",
				Margin = [sidebar_size_em, "0em"],
				BackgroundColor = RGB(25, 25, 25),
				symbol_part =
				{
					Right = "10em",
					Symbol = nil,
					Margin = "1em",
					ID = 1,
					Target = current_description_box.symbol_target
				},
				desc_part =
				{
					Left = "10em",
					Margin = "1em",
					ID = 1,
					Target = current_description_box.target,
					real_contents = // nested one more time so it can dynamically be replaced without messing up the layout
					{
						ID = 1,
						Target = current_description_box.desc_target
					}
				}
			}
		};
		current_main_menu_id = GuiOpen(root_menu);
	}
	else // menu already exists and only one part has to be added
	{
		GuiUpdate({_update: part_menu}, current_main_menu_id, nil, nil);
	}
	
	// Show "put/take all items" buttons if applicable. Also update tooltip.
	var show_grab_all = current_menus[0] && current_menus[1];
	show_grab_all = show_grab_all 
					&& (current_menus[0].target->~IsContainer() || current_menus[0].target->~IsClonk())
					&& (current_menus[1].target->~IsContainer() || current_menus[1].target->~IsClonk());
	if (show_grab_all)
	{
		current_center_column_target.Visibility = VIS_Owner;
		for (var i = 0; i < 2; ++i)
			GuiUpdate({Tooltip: Format("$MoveAllTo$", current_menus[i].target->GetName())}, current_main_menu_id, 10 + i, current_center_column_target);
	}
	else
	{
		current_center_column_target.Visibility = VIS_None;
	}
}

// Tries to put all items from the other menu's target into the target of menu menu_id. Returns nil.
public func OnMoveAllToClicked(int menu_id)
{
	// Sanity checks..
	for (var i = 0; i < 2; ++i)
	{
		if (!current_menus[i] || !current_menus[i].target)
			return;
		if (!current_menus[i].target->~IsContainer() && !current_menus[i].target->~IsClonk())
			return;
	}
	// Take all from the other object and try to put into the target.
	var other = current_menus[1 - menu_id].target;
	var target = current_menus[menu_id].target;
	var contents = FindObjects(Find_Container(other));
	var transfered = 0;
	for (var obj in contents) 
	{
		// Sanity, can actually happen if an item merges with others during the transfer etc.
		if (!obj) continue;
		
		var collected = target->Collect(obj, true);
		if (collected)
			++transfered;
	}
	
	if (transfered > 0)
	{
		Sound("SoftTouch*", true, nil, GetOwner());
		return;
	}
	else
	{
		Sound("BalloonPop", true, nil, GetOwner());
		return;
	}
}

// generates a proplist that defines a custom GUI that represents a side bar where objects 
// to interact with can be selected
func CreateSideBar(int slot)
{
	var em_size = ToEmString(InteractionMenu_SideBarSize);
	var sidebar =
	{
		Priority = 10,
		Right = em_size,
		Style = GUI_VerticalLayout,
		Target = current_menus[slot].menu_object,
		ID = 2
	};
	if (slot == 1)
	{
		sidebar.Left = Format("100%% %s", ToEmString(-InteractionMenu_SideBarSize));
		sidebar.Right = "100%";
	}
	
	// Now show the current_objects list as entries.
	// If there is a forced-open menu, also add it to bottom of sidebar..
	var sidebar_items = nil;
	if (current_menus[slot].forced)
	{
		sidebar_items = current_objects[:];
		PushBack(sidebar_items, current_menus[slot].target);
	}
	else
		sidebar_items = current_objects;
		
	for (var obj in sidebar_items)
	{
		var background_color = nil;
		var symbol = {Std = Icon_Menu_RectangleRounded, OnHover = Icon_Menu_RectangleBrightRounded};
		// figure out whether the object is already selected
		// if so, highlight the entry
		if (current_menus[slot].target == obj)
		{
			background_color = RGBa(255, 255, 0, 10);
			symbol = Icon_Menu_RectangleBrightRounded;
		}
		var priority = 10000 - obj.Plane;
		if (obj == cursor) priority = 1;
		var entry = 
		{
			Right = em_size, Bottom = em_size,
			Symbol = symbol,
			Priority = priority,
			Style = GUI_TextBottom | GUI_TextHCenter,
			BackgroundColor = background_color,
			OnMouseIn = GuiAction_SetTag("OnHover"),
			OnMouseOut = GuiAction_SetTag("Std"),
			OnClick = GuiAction_Call(this, "OnSidebarEntrySelected", {slot = slot, obj = obj}),
			Text = obj->GetName(),
			obj_symbol = {Symbol = obj, Margin = "0.5em"}
		};
		
		GuiAddSubwindow(entry, sidebar);
	}
	return sidebar;
}

// Updates the sidebar with the current objects (and closes the old one).
func RefreshSidebar(int slot)
{
	if (!current_menus[slot]) return;
	// Close old sidebar? This call will just do nothing if there is no sidebar present.
	GuiClose(current_main_menu_id, 2, current_menus[slot].menu_object);
	
	var sidebar = CreateSideBar(slot);
	GuiUpdate({sidebar = sidebar}, current_main_menu_id, 1, current_menus[slot].menu_object);
}

func OnSidebarEntrySelected(data, int player, int ID, int subwindowID, object target)
{
	if (!data.obj) return;
	
	// can not open object twice!
	for (var menu in current_menus)
		if (menu.target == data.obj) return;
	OpenMenuForObject(data.obj, data.slot);
}

/*
	Generates and creates one side of the menu.
	Returns the proplist that will be put into the main menu on the left or right side.
*/
func CreateMainMenu(object obj, int slot)
{
	var container =
	{
		Target = CreateDummy(),
		Priority = 5,
		Right = Format("100%% %s", ToEmString(-InteractionMenu_SideBarSize)),
		Style = GUI_VerticalLayout,
		BackgroundColor = RGB(25, 25, 25)
	};
	if (slot == 0)
	{
		container.Left = ToEmString(InteractionMenu_SideBarSize);
		container.Right = "100%";
	}
	
	// Do virtually nothing if the building is not ready to be interacted with. This can be caused by several things.
	var error_message = nil;
	if (obj->GetCon() < 100) error_message = Format("$MsgNotFullyConstructed$", obj->GetName());
	else if (Hostile(cursor->GetOwner(), obj->GetOwner())) error_message = Format("$MsgHostile$", obj->GetName(), GetTaggedPlayerName(obj->GetOwner()));
	
	if (error_message)
	{
		container.Style = GUI_TextVCenter | GUI_TextHCenter;
		container.Text = error_message;
		return container;
	}
	
	var menus = obj->~GetInteractionMenus(cursor) ?? [];
	// get all interaction info from the object and put it into a menu
	// contents first
	if (obj->~IsContainer() || obj->~IsClonk())
	{
		var info =
		{
			flag = InteractionMenu_Contents,
			title = "$Contents$",
			entries = [],
			entries_callback = nil,
			callback = "OnContentsSelection",
			callback_target = this,
			decoration = GUI_MenuDecoInventoryHeader,
			Priority = 10
		};
		PushBack(menus, info);
	}
	
	current_menus[slot].menus = menus;
	
	// now generate the actual menus from the information-list
	for (var i = 0; i < GetLength(menus); ++i)
	{
		var menu = menus[i];
		if (!menu.flag)
			menu.flag = InteractionMenu_Custom;
		if (menu.entries_callback)
			menu.entries = obj->Call(menu.entries_callback, obj);
		if (menu.entries == nil)
		{
			FatalError(Format("An interaction menu did not return valid entries. %s -> %v() (object %v)", obj->GetName(), menu.entries_callback, obj));
			continue;
		}
		menu.menu_object = CreateObject(MenuStyle_Grid);
		
		menu.menu_object.Top = "+2em";
		menu.menu_object.Priority = 2;
		menu.menu_object->SetPermanent();
		menu.menu_object->SetFitChildren();
		//menu.menu_object.Bottom = "4em";
		menu.menu_object->SetMouseOverCallback(this, "OnMenuEntryHover");
		for (var e = 0; e < GetLength(menu.entries); ++e)
		{
			var entry = menu.entries[e];
			entry.unique_index = ++menu.entry_index_count;
			// This also allows the interaction-menu user to supply a custom entry with custom layout f.e.
			menu.menu_object->AddItem(entry.symbol, entry.text, entry.unique_index, this, "OnMenuEntrySelected", { slot = slot, index = i }, entry["custom"]);
		}
		
		var all =
		{
			Priority = menu.Priority ?? i,
			Style = GUI_FitChildren,
			title_bar = 
			{
				Priority = 1,
				Style = GUI_TextVCenter | GUI_TextHCenter,
				Bottom = "+1.5em",
				Text = menu.title,
				BackgroundColor = 0xa0000000,
				//Decoration = menu.decoration
				hline = {Bottom = "0.1em", BackgroundColor = RGB(100, 100, 100)}
			},
			Margin = [nil, nil, nil, "0.5em"],
			real_menu = menu.menu_object,
			spacer = {Left = "0em", Right = "0em", Bottom = "6em"} // guarantees a minimum height
		};
		if (menu.flag == InteractionMenu_Contents)
			all.BackgroundColor = RGB(0, 50, 0);
		else if (menu.BackgroundColor)
				all.BackgroundColor = menu.BackgroundColor;
		else if (menu.decoration)
			menu.menu_object.BackgroundColor = menu.decoration->FrameDecorationBackClr();
		GuiAddSubwindow(all, container);
	}
	
	// add refreshing effects for all of the contents menus
	for (var i = 0; i < GetLength(menus); ++i)
	{
		if (!(menus[i].flag & InteractionMenu_Contents)) continue;
		AddEffect("IntRefreshContentsMenu", this, 1, 1, this, nil, obj, slot, i);
	}
	
	return container;
}

func GetEntryInformation(proplist menu_info, int entry_index)
{
	if (!current_menus[menu_info.slot]) return;
	var menu;
	if (!(menu = current_menus[menu_info.slot].menus[menu_info.index])) return;
	var entry;
	for (var possible in menu.entries)
	{
		if (possible.unique_index != entry_index) continue;
		entry = possible;
		break;
	}
	return {menu=menu, entry=entry};
}

func OnMenuEntryHover(proplist menu_info, int entry_index, int player)
{
	var info = GetEntryInformation(menu_info, entry_index);
	if (!info.entry) return;
	// update symbol of description box
	GuiUpdate({Symbol = info.entry.symbol}, current_main_menu_id, 1, current_description_box.symbol_target);
	// and update description itself
	// clean up existing description window in case it has been cluttered by sub-windows
	GuiClose(current_main_menu_id, 1, current_description_box.desc_target);
	// now add new subwindow to replace the recently removed one
	GuiUpdate({new_subwindow = {Target = current_description_box.desc_target, ID = 1}}, current_main_menu_id, 1, current_description_box.target);
	// default to description of object
	if (!info.menu.callback_target || !info.menu.callback_hover)
	{
		var text = Format("%s:|%s", info.entry.symbol.Name, info.entry.symbol.Description);
		var obj = nil;
		if (info.entry.extra_data)
			obj = info.entry.extra_data.one_object;
		// For contents menus, we can sometimes present additional information about objects.
		if (info.menu.flag == InteractionMenu_Contents && obj)
		{
			var additional = nil;
			if (obj->Contents())
			{
				additional = "$Contains$ ";
				var i = 0, count = obj->ContentsCount();
				// This currently justs lists contents one after the other.
				// Items are not stacked, which should be enough for everything we have ingame right now. If this is filed into the bugtracker at some point, fix here.
				for (;i < count;++i)
				{
					if (i > 0)
						additional = Format("%s, ", additional);
					additional = Format("%s%s", additional, obj->Contents(i)->GetName());
				}
			}
			if (additional != nil)
				text = Format("%s||%s", text, additional);
		}
		
		GuiUpdateText(text, current_main_menu_id, 1, current_description_box.desc_target);
	}
	else
	{
		info.menu.callback_target->Call(info.menu.callback_hover, info.entry.symbol, info.entry.extra_data, current_description_box.desc_target, current_main_menu_id);
	}
}

func OnMenuEntrySelected(proplist menu_info, int entry_index, int player)
{
	var info = GetEntryInformation(menu_info, entry_index);
	if (!info.entry) return;
	
	var callback_target;
	if (!(callback_target = info.menu.callback_target)) return;
	if (!info.menu.callback) return; // The menu can actually decide to handle user interaction itself and not provide a callback.
	var result = callback_target->Call(info.menu.callback, info.entry.symbol, info.entry.extra_data, cursor);
	
	// todo: trigger refresh for special value of result?
}

private func OnContentsSelection(symbol, extra_data)
{
	var target = current_menus[extra_data.slot].target;
	if (!target) return;
	// no target to swap to?
	if (!current_menus[1 - extra_data.slot]) return;
	var other_target = current_menus[1 - extra_data.slot].target;
	if (!other_target) return;
	
	var obj = extra_data.one_object ?? FindObject(Find_Container(target), Find_ID(symbol));
	if (!obj) return;
	
	// If stackable, always try to grab a full stack.
	// Imagine armory with 200 arrows, but not 10 stacks with 20 each but 200 stacks with 1 each.
	if (obj->~IsStackable())
	{
		var others = FindObjects(Find_Container(target), Find_ID(symbol), Find_Exclude(obj));
		for (var other in others) 
		{
			if (obj->IsFullStack()) break;
			other->TryAddToStack(obj);
		}
	}
	
	// More special handling for Stackable items..
	var handled = obj->~TryPutInto(other_target);
	if (handled || other_target->Collect(obj, true))
	{
		Sound("SoftTouch*", true, nil, GetOwner());
		return true;
	}
	else
	{
		Sound("BalloonPop", true, nil, GetOwner());
		return false;
	}
}

func FxIntRefreshContentsMenuStart(object target, proplist effect, temp, object obj, int slot, int menu_index)
{
	if (temp) return;
	effect.obj = obj; // the property (with this name) is externally accessed!
	effect.slot = slot;
	effect.menu_index = menu_index;
	effect.last_inventory = [];
}

func FxIntRefreshContentsMenuTimer(target, effect, time)
{
	if (!effect.obj) return -1;
	// Helper object used to track extra-slot objects. When this object is removed, the tracking stops.
	var extra_slot_keep_alive = current_menus[effect.slot].menu_object;
	// The fast interval is only used for the very first check to ensure a fast startup.
	// It can't be just called instantly though, because the menu might not have been opened yet.
	if (effect.Interval == 1) effect.Interval = 5;
	var inventory = [];
	var obj, i = 0;
	while (obj = effect.obj->Contents(i++))
	{
		var symbol = obj->GetID();
		var extra_data = {slot = effect.slot, menu_index = effect.menu_index, one_object = nil /* for unstackable containers */};
		
		// check if already exists (and then stack!)
		var found = false;
		// Never stack containers with (different) contents, though.
		var is_container = obj->~IsContainer();
		var has_contents = obj->ContentsCount() != 0;
		// For extra-slot objects, we should attach a tracking effect to update the UI on changes.
		if (obj->~HasExtraSlot())
		{
			var j = 0, e = nil;
			var found_tracker = false;
			while (e = GetEffect(nil, obj, j++))
			{
				if (e.keep_alive != extra_slot_keep_alive) continue;
				found_tracker = true;
				break;
			}
			if (!found_tracker)
			{
				var e = AddEffect("ExtraSlotTracker", obj, 1, 30 + Random(60), this);
				e.keep_alive = extra_slot_keep_alive;
				e.callback_effect = effect;
			}
		}
		// How many objects are this object?!
		var object_amount = obj->~GetStackCount() ?? 1;
		// Empty containers can be stacked.
		if (!(is_container && has_contents))
		{
			for (var inv in inventory)
			{
				if (inv.symbol != symbol) continue;
				if (inv.has_contents) continue;
				inv.count += object_amount;
				inv.text = Format("%dx", inv.count);
				found = true;
				break;
			}
		}
		else // Can't stack? Remember object..
			extra_data.one_object = obj;
		// Add new!
		if (!found)
		{
			// Do we need a custom entry when the object has contents?
			var custom = nil;
			if (is_container)
			{
				// Use a default grid-menu entry as the base.
				custom = MenuStyle_Grid->MakeEntryProplist(symbol, nil);
				// Pack it into a larger frame to allow for another button below.
				// The entries with contents are sorted to the back of the inventory menu. This makes for a nicer layout.
				custom = {Right = custom.Right, Bottom = "8em", top = custom, Priority = 10000 + obj->GetValue()};
				// Then add a little container-symbol (that can be clicked).
				custom.bottom =
				{
					Top = "4em",
					BackgroundColor = {Std = 0, Selected = RGBa(255, 100, 100, 100)},
					OnMouseIn = GuiAction_SetTag("Selected"),
					OnMouseOut = GuiAction_SetTag("Std"),
					OnClick = GuiAction_Call(this, "OnExtraSlotClicked", {slot = effect.slot, one_object = obj, ID = obj->GetID()}),
					container = 
					{
						Symbol = Chest,
						Priority = 1
					}
				};

				// And if the object has contents, show the first one, too.
				if (has_contents)
				{
					// This icon won't ever be stacked. Remember it for a description.
					extra_data.one_object = obj;
					// Add to GUI.
					custom.bottom.contents = 
					{
						Symbol = obj->Contents(0)->GetID(),
						Margin = "0.25em",
						Priority = 2
					};
					// Possibly add text for stackable items - this is an special exception for the Library_Stackable.
					var count = obj->Contents(0)->~GetStackCount();
					count = count ?? obj->ContentsCount(obj->Contents(0)->GetID());
					if (count > 1)
					{
						custom.bottom.contents.Text = Format("%dx", count);
						custom.bottom.contents.Style = GUI_TextBottom | GUI_TextRight;
					}
					// Also make the chest smaller, so that the contents symbol is not obstructed.
					custom.bottom.container.Bottom = "2em";
					custom.bottom.container.Left = "2em";
				}
			}
			// Add to menu!
			var text = nil;
			if (object_amount > 1)
				text = Format("%dx", object_amount);
			PushBack(inventory,
				{
					symbol = symbol, 
					extra_data = extra_data, 
					has_contents = (is_container && has_contents),
					custom = custom,
					count = object_amount,
					text = text
				});
		}
	}
	

	if (GetLength(inventory) == GetLength(effect.last_inventory))
	{
		var same = true;
		for (var i = GetLength(inventory) - 1; i >= 0; --i)
		{
			if (inventory[i].symbol == effect.last_inventory[i].symbol
				&& inventory[i].text == effect.last_inventory[i].text) continue;
			same = false;
			break;
		}
		if (same) return 1;
	}
	
	effect.last_inventory = inventory[:];
	DoMenuRefresh(effect.slot, effect.menu_index, inventory);
	return 1;
}

func FxExtraSlotTrackerTimer(object target, proplist effect, int time)
{
	if (!effect.keep_alive) return -1;
	return 1;
}

// This is called by the extra-slot library.
func FxExtraSlotTrackerUpdate(object target, proplist effect)
{
	// Simply overwrite the inventory cache of the IntRefreshContentsMenu effect.
	// This will lead to the inventory being upated asap.
	if (effect.callback_effect)
		effect.callback_effect.last_inventory = [];
}

func OnExtraSlotClicked(proplist extra_data)
{
	var menu = current_menus[extra_data.slot];
	if (!menu) return;
	var obj = extra_data.one_object; 
	if (!obj || obj->Contained() != menu.target)
	{
		// Maybe find a similar object? (case: stack of empty bows and one was removed -> user doesn't care which one is displayed)
		for (var o in FindObjects(Find_Container(menu.target), Find_ID(extra_data.ID)))
		{
			obj = o;
			if (!obj->Contents())
				break;
		}
		if (!obj) return;
	}
	
	OpenMenuForObject(obj, extra_data.slot, true);
}

// This function is supposed to be called when the menu already exists (is open) and some sub-menu needs an update.
// Note that the parameter "new_entries" is optional. If not supplied, the /entries_callback/ for the specified menu will be used to fill the menu.
func DoMenuRefresh(int slot, int menu_index, array new_entries)
{
	// go through new_entries and look for differences to currently open menu
	// then try to only adjust the existing menu when possible
	// the assumption is that ususally only few entries change
	var menu = current_menus[slot].menus[menu_index];
	var current_entries = menu.entries;
	if (!new_entries && menu.entries_callback)
		new_entries = current_menus[slot].target->Call(menu.entries_callback, this.cursor);
	
	// step 0.1: update all items where the symbol and extra_data did not change but other things (f.e. the text)
	// this is done to maintain a consistent order that would be shuffled constantly if the entry was removed and re-added at the end
	for (var c = 0; c < GetLength(current_entries); ++c)
	{
		var old_entry = current_entries[c];
		var found = false;
		var symbol_equal_index = -1;
		for (var ni = 0; ni < GetLength(new_entries); ++ni)
		{
			var new_entry = new_entries[ni];
			if (!EntriesEqual(new_entry, old_entry))
			{
				if ((new_entry.symbol == old_entry.symbol) && (new_entry.extra_data == old_entry.extra_data))
					symbol_equal_index = ni;
				continue;
			}
			found = true;
			break;
		}
		// if the entry exist just like that, we do not need to do anything
		// same, if we don't have anything to replace it with, anyway
		if (found || symbol_equal_index == -1) continue;
		// now we can just update the symbol with the new data
		var new_entry = new_entries[symbol_equal_index];
		menu.menu_object->UpdateItem(new_entry.symbol, new_entry.text, old_entry.unique_index, this, "OnMenuEntrySelected", { slot = slot, index = menu_index }, new_entry["custom"], current_main_menu_id);
		new_entry.unique_index = old_entry.unique_index;
		// make sure it's not manipulated later on
		current_entries[c] = nil;
	}
	// step 1: remove (close) all current entries that have been removed
	for (var c = 0; c < GetLength(current_entries); ++c)
	{
		var old_entry = current_entries[c];
		if (!old_entry) continue;
		
		// check for removal
		var removed = true;
		for (var new_entry in new_entries)
		{
			if (!EntriesEqual(new_entry, old_entry)) continue;
			removed = false;
			break;
		}
		if (removed)
		{
			menu.menu_object->RemoveItem(old_entry.unique_index, current_main_menu_id);
			current_entries[c] = nil;
		}
	}
	
	// step 2: add new entries
	for (var c = 0; c < GetLength(new_entries); ++c)
	{
		var new_entry = new_entries[c];
		// the entry was already updated before?
		if (new_entry.unique_index != nil) continue;
		
		var existing = false;
		for (var old_entry in current_entries)
		{
			if (old_entry == nil) // might be nil as a result of step 1
				continue;
			if (!EntriesEqual(new_entry, old_entry)) continue;
			existing = true;
			
			// fix unique indices for the new array
			new_entry.unique_index = old_entry.unique_index;
			break;
		}
		if (existing) continue;
		
		new_entry.unique_index = ++menu.entry_index_count;
		menu.menu_object->AddItem(new_entry.symbol, new_entry.text, new_entry.unique_index, this, "OnMenuEntrySelected", { slot = slot, index = menu_index }, new_entry["custom"], current_main_menu_id);
		
	}
	menu.entries = new_entries;
}

func EntriesEqual(proplist entry_a, proplist entry_b)
{
	return entry_a.symbol == entry_b.symbol
	&& entry_a.text == entry_b.text
	&& entry_a.extra_data == entry_b.extra_data
	&& entry_a.custom == entry_b.custom;
}

func CreateDummy()
{
	var dummy = CreateContents(Dummy);
	dummy.Visibility = VIS_Owner;
	dummy->SetOwner(GetOwner());
	return dummy;
}

func RemoveDummy(object dummy, int player, int ID, int subwindowID, object target)
{
	if (dummy)
		dummy->RemoveObject();
}

// updates the interaction menu for an object iff it is currently shown
func UpdateInteractionMenuFor(object target, callbacks)
{
	for (var slot = 0; slot < GetLength(current_menus); ++slot)
	{
		var current_menu = current_menus[slot];
		if (!current_menu || current_menu.target != target) continue;
		if (!callbacks) // do a full refresh
			OpenMenuForObject(target, slot);
		else // otherwise selectively update the menus for the callbacks
		{
			for (var callback in callbacks)
			{
				for (var menu_index = 0; menu_index < GetLength(current_menu.menus); ++menu_index)
				{
					var menu = current_menu.menus[menu_index];
					if (menu.entries_callback != callback) continue;
					DoMenuRefresh(slot, menu_index);
				}
			}
		}
	}
}

/*
	Updates all interaction menus that are currently attached to an object.
	This function can be called at all times, not only when a menu is open, making it more convenient for users, because there is no need to track open menus.
	If the /callbacks/ parameter is supplied, only menus that use those callbacks are updated. That way, a producer can f.e. only update its "queue" menu.
*/
global func UpdateInteractionMenus(callbacks)
{
	if (!this) return;
	if (callbacks && GetType(callbacks) != C4V_Array) callbacks = [callbacks];
	for (var interaction_menu in FindObjects(Find_ID(GUI_ObjectInteractionMenu)))
		interaction_menu->UpdateInteractionMenuFor(this, callbacks);
}
