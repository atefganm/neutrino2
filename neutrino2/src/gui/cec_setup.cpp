/*
	cec settings menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gui/cec_setup.h>

#include <global.h>
#include <neutrino2.h>

#include <driver/hdmi_cec.h>

#include <system/debug.h>


CCECSetup::CCECSetup()
{
	cec1 = NULL;
	cec2 = NULL;
	cec3 = NULL;
}

int CCECSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_NORMAL, "CCECSetup::exec:\n");
	
	int   res = CMenuTarget::RETURN_REPAINT;

	if (parent)
		parent->hide();

	res = showMenu();

	return res;
}

#define OPTIONS_OFF0_ON1_OPTION_COUNT 2
const keyval OPTIONS_OFF0_ON1_OPTIONS[OPTIONS_OFF0_ON1_OPTION_COUNT] =
{
        { 0, _("off") },
        { 1, _("on") }
};

#define VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT 3
const keyval VIDEOMENU_HDMI_CEC_MODE_OPTIONS[VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_MODE_OFF	, _("CEC mode off")      },
	{ VIDEO_HDMI_CEC_MODE_TUNER	, _("CEC mode tuner")   },
	{ VIDEO_HDMI_CEC_MODE_RECORDER	, _("CEC mode recorder") }
};

#define VIDEOMENU_HDMI_CEC_VOL_OPTION_COUNT 3
const keyval VIDEOMENU_HDMI_CEC_VOL_OPTIONS[VIDEOMENU_HDMI_CEC_VOL_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_VOL_OFF		, _("CEC volume off") },
	{ VIDEO_HDMI_CEC_VOL_AUDIOSYSTEM, _("CEC volume audiosystem") },
	{ VIDEO_HDMI_CEC_VOL_TV			, _("CEC volume TV") }
};

int CCECSetup::showMenu()
{
	//menue init
	CWidget *widget = NULL;
	ClistBox *cec = NULL;
	
	widget = CNeutrinoApp::getInstance()->getWidget("cecsetup");
	
	if (widget)
	{
		cec = (ClistBox*)widget->getCCItem(CComponent::CC_LISTBOX);
	}
	else
	{
		//
		widget = new CWidget(0, 0, MENU_WIDTH, MENU_HEIGHT);
		widget->name = "cecsetup";
		widget->setMenuPosition(CWidget::MENU_POSITION_CENTER);
		
		//
		cec = new ClistBox(widget->getWindowsPos().iX, widget->getWindowsPos().iY, widget->getWindowsPos().iWidth, widget->getWindowsPos().iHeight);

		cec->setWidgetMode(ClistBox::MODE_SETUP);
		cec->enableShrinkMenu();
		
		//
		cec->enablePaintHead();
		cec->setTitle(_("CEC Setup"), NEUTRINO_ICON_SETTINGS);

		//
		cec->enablePaintFoot();
			
		const struct button_label btn = { NEUTRINO_ICON_INFO, " ", 0 };
			
		cec->setFootButtons(&btn);
		
		//
		widget->addCCItem(cec);
	}
	
	//
	oldLcdMode = CLCD::getInstance()->getMode();
	oldLcdMenutitle = CLCD::getInstance()->getMenutitle();
	CLCD::getInstance()->setMode(CLCD::MODE_MENU_UTF8, _("CEC Setup"));
	
	// intros
	cec->addItem(new CMenuForwarder(_("back")));
	cec->addItem( new CMenuSeparator(CMenuSeparator::LINE) );
	
	// save settings
	cec->addItem(new CMenuForwarder(_("Save settings now"), true, NULL, CNeutrinoApp::getInstance(), "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	cec->addItem(new CMenuSeparator(CMenuSeparator::LINE));

	//cec
	CMenuOptionChooser *cec_ch = new CMenuOptionChooser(_("CEC mode"), &g_settings.hdmi_cec_mode, VIDEOMENU_HDMI_CEC_MODE_OPTIONS, VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT, true, this);
	
	cec1 = new CMenuOptionChooser(_("CEC view on"), &g_settings.hdmi_cec_view_on, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	
	cec2 = new CMenuOptionChooser(_("CEC standby"), &g_settings.hdmi_cec_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	
	cec3 = new CMenuOptionChooser(_("CEC volume"), &g_settings.hdmi_cec_volume, VIDEOMENU_HDMI_CEC_VOL_OPTIONS, VIDEOMENU_HDMI_CEC_VOL_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);

	cec->addItem(cec_ch);
	cec->addItem(cec1);
	cec->addItem(cec2);
	cec->addItem(cec3);

	widget->setTimeOut(g_settings.timing_menu);
	
	int res = widget->exec(NULL, "");
	
	if (widget)
	{
		delete widget;
		widget = NULL;
	}
	
	//
        CLCD::getInstance()->setMode(oldLcdMode, oldLcdMenutitle.c_str());

	return res;
}

void CCECSetup::setCECSettings(bool b)
{	
	dprintf(DEBUG_NORMAL, "CCECSetup::setCECSettings\n");
	
	hdmi_cec::getInstance()->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
	hdmi_cec::getInstance()->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
	hdmi_cec::getInstance()->GetAudioDestination();
	hdmi_cec::getInstance()->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);	
}

bool CCECSetup::changeNotify(const std::string& OptionName, void * /*data*/)
{
	dprintf(DEBUG_NORMAL, "CCECSetup::changeNotify\n");

	if (OptionName == _("CEC mode"))
	{
		cec1->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
		cec2->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
		cec3->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
		
		hdmi_cec::getInstance()->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);
	}
	else if (OptionName == _("CEC standby"))
	{
		hdmi_cec::getInstance()->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
	}
	else if (OptionName == _("CEC view on"))
	{
		hdmi_cec::getInstance()->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
	}
	else if (OptionName == _("CEC volume"))
	{
		if (g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF)
		{
			g_settings.current_volume = 100;
			
			hdmi_cec::getInstance()->GetAudioDestination();
		}
	}

	return false;
}

