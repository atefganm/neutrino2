/*
	Neutrino-GUI  -   DBoxII-Project
	
	$Id: menue.h 2013/10/12 mohousch Exp $

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __MENU__
#define __MENU__

#include <string>
#include <vector>

#include <driver/gfx/framebuffer.h>
#include <driver/gfx/color.h>

#include <driver/rcinput.h>

#include <system/localize.h>
#include <system/helpers.h>

#include <gui/widget/textbox.h>
#include <gui/widget/widget_helpers.h>

#include <gui/plugins.h>

#include <system/settings.h>
#include <gui/widget/listbox.h>

	
// CMenuWidget
class CMenuWidget : public CMenuTarget
{	
	protected:
		//
		CFrameBuffer *frameBuffer;
		
		//
		int width;
		int height;
		int x;
		int y;
		int offx, offy;
		int wanted_height;
		int wanted_width;

		//
		std::vector<CMenuItem*> items;
		
		void Init(const std::string & Icon, const int mwidth, const int mheight);
		virtual void paintItems();
		
		//
		std::vector<unsigned int> page_start;
		unsigned int item_start_y;
		unsigned int current_page;
		unsigned int total_pages;
		int sb_width;
		int items_height;
		int items_width;
		CCScrollBar scrollBar;
		
		//
		fb_pixel_t * background;
		int full_width;
		int full_height;
		bool savescreen;
		void saveScreen();
		void restoreScreen();
		
		//
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;
		int selected;
		bool exit_pressed;

		struct keyAction { 
			std::string action; 
			CMenuTarget *menue; 
		};
		std::map<neutrino_msg_t, keyAction> keyActionMap;

		uint32_t sec_timer_id;
		uint64_t timeout;

		//// head
		int hheight;
		std::string l_name;
		std::string iconfile;
		int hbutton_count;
		button_label_list_t hbutton_labels;
		int thalign;
		bool paintDate;
		std::string format;
		CCTime* timer;
		
		//// foot
		int fheight;
		int fbutton_count;
		int fbutton_width;
		button_label_list_t fbutton_labels;

		//// itemInfo
		bool paintFootInfo;
		int cFrameFootInfoHeight;
		int footInfoHeight;
		int footInfoMode;
		CCItemInfo itemsLine;
		
		// extended
		CTextBox * textBox;
		
		// frame
		int itemsPerX;
		int itemsPerY;
		int maxItemsPerPage;
		int item_height;
		int item_width;

		//
		int widgetType;

		//
		bool shrinkMenu;
		int widgetMode;
		int menu_position;

		// for lua
		std::string actionKey;
		
		//// dirty Hack: FIXME
		ClistBox *listBox;
			
	public:
		CMenuWidget();
		CMenuWidget(const char * const Name, const std::string& Icon = "", const int mwidth = MENU_WIDTH, const int mheight = MENU_HEIGHT);
		
		virtual ~CMenuWidget();

		////
		void addItem(CMenuItem * menuItem, const bool defaultselected = false);
		void integratePlugins(CPlugins::i_type_t integration = CPlugins::I_TYPE_DISABLED, const unsigned int shortcut = CRCInput::RC_nokey, bool enabled = true);
		void removeItem(long pos);
		bool hasItem();
		int getItemsCount()const{return items.size();};
		void clearItems(void){items.clear();};
		void clear(void){items.clear(); hbutton_labels.clear(); fbutton_labels.clear(); current_page = 0;};

		////
		void initFrames();
		void paintHead();
		void paintFoot();
		void paint();
		void paintItemInfo(int pos);
		void hideItemInfo();
		void hide();
		void refresh();
		void enableSaveScreen();

		////
		int exec(CMenuTarget * parent, const std::string &actionKey);

		////
		void setSelected(unsigned int _new) { if(_new <= items.size()) selected = _new; if (selected < 0) selected = 0;};
		void setTimeOut(unsigned long long int to = 0){timeout = to;};
		void move(int xoff, int yoff);
		void addKey(neutrino_msg_t key, CMenuTarget *menue = NULL, const std::string &action = "");

		//// head
		void setTitle(const char* title = "", const char* icon = NULL){l_name = title; if(icon != NULL) iconfile = icon;};
		void setTitleHAlign(const int m){thalign = m;};
		void setHeadButtons(const struct button_label* _hbutton_label, const int _hbutton_count = 1);
		void enablePaintDate(void){paintDate = true;};
		void setFormat(const char* f){if (f) format.clear(); format = f;};
		
		//// foot
		void setFootButtons(const struct button_label *_fbutton_label, const int _fbutton_count = 1, const int _fbutton_width = 0);

		//// itemInfo
		void enablePaintItemInfo(int fh = 70){paintFootInfo = true; footInfoHeight = fh; /*initFrames();*/};
		void setItemInfoMode(int mode = CCItemInfo::ITEMINFO_INFO){footInfoMode = mode;};

		//// type/mode/pos
		void setWidgetType(int type){widgetType = type;};
		void setWidgetMode(int mode){widgetMode = mode;};
		void setMenuPosition(int p){menu_position = p;};
		void enableShrinkMenu(){shrinkMenu = true;};
		//
		void setItemsPerPage(int itemsX = 6, int itemsY = 3){itemsPerX = itemsX; itemsPerY = itemsY; maxItemsPerPage = itemsPerX*itemsPerY;};

		//// get methods
		bool getExitPressed(){return exit_pressed;};
		int getSelected(){return selected;};
		int getWidgetType(){return widgetType;};
		int getHeight(void) const {return height;}
		int getWidth(void) const {return width;};
		int getX(void) const {return x;};
		int getY(void) const {return y;};
		std::string getActionKey(){return actionKey;}; // lua
		neutrino_msg_t getKey(){return msg;};
};

#endif

