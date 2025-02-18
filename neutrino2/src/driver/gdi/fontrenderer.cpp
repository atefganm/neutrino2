//
//	Neutrino-GUI  -   DBoxII-Project
//	
//	$Id: fontrenderer.cpp 30.10.2023 mohousch Exp $
//
//	Copyright (C) 2001 Steffen Hehn 'McClean'
//      Copyright (C) 2003 thegoodguy
//
//	License: GPL
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <vector>

// this method is recommended for FreeType >2.0.x:
#include <ft2build.h>
#include FT_FREETYPE_H

#include <driver/gdi/fontrenderer.h>

#include <system/debug.h>
#include <global.h>

// fribidi
#include <fribidi/fribidi.h>


////
FT_Error FBFontRenderClass::myFTC_Face_Requester(FTC_FaceID face_id, FT_Library library, FT_Pointer request_data, FT_Face *aface)
{
	return ((FBFontRenderClass*)request_data)->FTC_Face_Requester(face_id, aface);
}

FBFontRenderClass::FBFontRenderClass(const int xr, const int yr)
{
	dprintf(DEBUG_DEBUG, "FBFontRenderClass::FBFontRenderClass: initializing core...\n");
	
	if (FT_Init_FreeType(&library))
	{
		dprintf(DEBUG_NORMAL, "FBFontRenderClass::FBFontRenderClass: initializing core failed.\n");
		return;
	}

	font = NULL;
	
	xres = xr;
	yres = yr;

	int maxbytes = 4 *1024*1024;
	dprintf(DEBUG_NORMAL, "FBFontRenderClass::FBFontRenderClass: Intializing font cache, using max. %dMB...\n", maxbytes/1024/1024);
	fflush(stdout);
	
	if (FTC_Manager_New(library, 10, 20, maxbytes, myFTC_Face_Requester, this, &cacheManager))
	{
		dprintf(DEBUG_NORMAL, "FBFontRenderClass::FBFontRenderClass: manager failed!\n");
		return;
	}
	
	if (!cacheManager)
	{
		dprintf(DEBUG_NORMAL, "FBFontRenderClass::FBFontRenderClass: error.\n");
		return;
	}
	
	if (FTC_SBitCache_New(cacheManager, &sbitsCache))
	{
		dprintf(DEBUG_NORMAL, "FBFontRenderClass::FBFontRenderClass: sbit failed!\n");
		return;
	}
	
	pthread_mutex_init( &render_mutex, NULL );
}

FBFontRenderClass::~FBFontRenderClass()
{
	fontListEntry * g;
	
	for (fontListEntry * f = font; f; f = g)
	{
		g = f->next;
		delete f;
	}

	FTC_Manager_Done(cacheManager);
	FT_Done_FreeType(library);
}

FT_Error FBFontRenderClass::FTC_Face_Requester(FTC_FaceID face_id, FT_Face* aface)
{
	fontListEntry *lfont = (fontListEntry *)face_id;
	
	if (!lfont)
		return -1;
	
	dprintf(DEBUG_DEBUG, "FBFontRenderClass::FTC_Face_Requester: FTC_Face_Requester (%s/%s)\n", lfont->family, lfont->style);

	int error;
	if ((error = FT_New_Face(library, lfont->filename, 0, aface)))
	{
		dprintf(DEBUG_NORMAL, "FBFontRenderClass::FTC_Face_Requester: FTC_Face_Requester (%s/%s) failed: %i\n", lfont->family, lfont->style, error);
		return error;
	}

	if (strcmp(lfont->style, (*aface)->style_name) != 0)
	{
		FT_Matrix matrix; // Italics

		matrix.xx = 1 * 0x10000;
		matrix.xy = (0x10000 >> 3);
		matrix.yx = 0 * 0x10000;
		matrix.yy = 1 * 0x10000;

		FT_Set_Transform(*aface, &matrix, NULL);
	}
	
	return 0;
}

FTC_FaceID FBFontRenderClass::getFaceID(const char * const family, const char * const style)
{
	for (fontListEntry *f=font; f; f=f->next)
	{
		if ((!strcmp(f->family, family)) && (!strcmp(f->style, style)))
			return (FTC_FaceID)f;
	}

	if (strncmp(style, "Bold ", 5) == 0)
	{
		for (fontListEntry *f=font; f; f=f->next)
		{
			if ((!strcmp(f->family, family)) && (!strcmp(f->style, &(style[5]))))
				return (FTC_FaceID)f;
		}
	}

	for (fontListEntry *f = font; f; f = f->next)
	{
		if (!strcmp(f->family, family))
		{
			if (f->next) // the first font always seems to be italic, skip if possible
				continue;
			return (FTC_FaceID)f;
		}
	}

	return 0;
}

FT_Error FBFontRenderClass::getGlyphBitmap(FTC_ImageTypeRec * pfont, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBitCache_Lookup(sbitsCache, pfont, glyph_index, sbit, NULL);
}

FT_Error FBFontRenderClass::getGlyphBitmap(FTC_ScalerRec * sc, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBitCache_LookupScaler(sbitsCache, sc, FT_LOAD_DEFAULT, glyph_index, sbit, NULL);
}

const char * FBFontRenderClass::AddFont(const char * const filename, bool make_italics)
{
	int error;
	fontListEntry *n = new fontListEntry;

	FT_Face face;

	if ((error = FT_New_Face(library, filename, 0, &face)))
	{
		dprintf(DEBUG_NORMAL, "FBFontRenderClass::AddFont: adding font %s, failed: %i\n", filename, error);
		//delete n;//Memory leak: n
		return NULL;
	}
	
	n->filename = strdup(filename);
	n->family   = strdup(face->family_name);
	n->style    = strdup(make_italics ? "Italic" : face->style_name);
	
	FT_Done_Face(face);
	n->next = font;
	
	dprintf(DEBUG_DEBUG, "FBFontRenderClass::AddFont: adding font %s... family %s, style %s ok\n", filename, n->family, n->style);
	font = n;
	
	return n->style;
}

FBFontRenderClass::fontListEntry::~fontListEntry()
{	
	free(filename);
	free(family);
	free(style);
}

CFont *FBFontRenderClass::getFont(const char * const family, const char * const style, int size)
{
	FTC_FaceID id = getFaceID(family, style);

	if (!id) 
	{
		dprintf(DEBUG_DEBUG, "FBFontRenderClass::getFont: getFont: family %s, style %s failed!\n", family, style);
		return 0;
	}

	dprintf(DEBUG_DEBUG, "FBFontRenderClass::getFont: getFont: family %s, style %s ok\n", family, style);

	return new CFont(this, id, size, (strcmp(((fontListEntry *)id)->style, style) == 0) ? CFont::Regular : CFont::Embolden);
}

std::string FBFontRenderClass::getFamily(const char * const filename) const
{
	for (fontListEntry *f=font; f; f=f->next)
	{
		if (!strcmp(f->filename, filename))
			return f->family;
	}

  	return "";
}

////
CFont::CFont(FBFontRenderClass * render, FTC_FaceID faceid, const int isize, const fontmodifier _stylemodifier)
{
	stylemodifier   = _stylemodifier;

	frameBuffer 	= CFrameBuffer::getInstance();
	renderer 	= render;
	font.face_id 	= faceid;
	font.width  	= isize;
	font.height 	= isize;
	font.flags = FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT;

	scaler.face_id = font.face_id;
	scaler.width   = isize * 64;
	scaler.height  = isize * 64;
	scaler.pixel   = false;
	scaler.x_res   = render->xres;
	scaler.y_res   = render->yres;

	setSize(isize);
}

FT_Error CFont::getGlyphBitmap(FT_ULong glyph_index, FTC_SBit *sbit)
{
	return renderer->getGlyphBitmap(&scaler, glyph_index, sbit);
}

int CFont::setSize(int isize)
{
	int temp = font.width; 
	font.width = font.height = isize;

	scaler.width  = isize * 64;
	scaler.height = isize * 64;

	FT_Error err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "%s: FTC_Manager_LookupSize failed (0x%x)\n", __FUNCTION__, err);
		return 0;
	}
	face = size->face;

	// hack begin (this is a hack to get correct font metrics, didn't find any other way which gave correct values)
	FTC_SBit glyph;
	int index;

	index = FT_Get_Char_Index(face, 'M'); // "M" gives us ascender
	getGlyphBitmap(index, &glyph);
	int tM = glyph->top;
	fontwidth = glyph->width;

	index = FT_Get_Char_Index(face, 'g'); // "g" gives us descender
	getGlyphBitmap(index, &glyph);
	int hg = glyph->height;
	int tg = glyph->top;

	ascender = tM;
	descender = tg - hg; //this is a negative value!
	int halflinegap = - (descender>>1); // |descender/2| - we use descender as linegap, half at top, half at bottom
	upper = halflinegap + ascender+3;   // we add 3 at top
	lower = -descender + halflinegap+1; // we add 1 at bottom
	height = upper + lower;               // this is total height == distance of lines
	// hack end

	return temp;
}

int CFont::getHeight(void)
{
	return height;
}

int CFont::getWidth(void)
{
	return fontwidth;
}

int UTF8ToUnicode(const char * &text, bool utf8_encoded) // returns -1 on error
{
	int unicode_value;
	
	if (utf8_encoded && ((((unsigned char)(*text)) & 0x80) != 0))
	{
		int remaining_unicode_length;
		if ((((unsigned char)(*text)) & 0xf8) == 0xf0)
		{
			unicode_value = ((unsigned char)(*text)) & 0x07;
			remaining_unicode_length = 3;
		}
		else if ((((unsigned char)(*text)) & 0xf0) == 0xe0)
		{
			unicode_value = ((unsigned char)(*text)) & 0x0f;
			remaining_unicode_length = 2;
		}
		else if ((((unsigned char)(*text)) & 0xe0) == 0xc0)
		{
			unicode_value = ((unsigned char)(*text)) & 0x1f;
			remaining_unicode_length = 1;
		}
		else                     // cf.: http://www.cl.cam.ac.uk/~mgk25/unicode.html#utf-8
			return -1;       // corrupted character or a character with > 4 bytes utf-8 representation
		
		for (int i = 0; i < remaining_unicode_length; i++)
		{
			text++;
			if (((*text) & 0xc0) != 0x80)
			{
				remaining_unicode_length = -1;
				return -1;          // incomplete or corrupted character
			}
			unicode_value <<= 6;
			unicode_value |= ((unsigned char)(*text)) & 0x3f;
		}
		if (remaining_unicode_length == -1)
			return -1;                  // incomplete or corrupted character
	}
	else
		unicode_value = (unsigned char)(*text);

	return unicode_value;
}

//
std::string fribidiShapeChar(const char* text, bool utf8_encoded)
{
	if(utf8_encoded) // fribidi requires utf-8
	{
		int len = strlen(text);
		
		fribidi_set_mirroring(true);
		fribidi_set_reorder_nsm(false);
			
		// init to utf-8
		FriBidiCharSet fribidiCharset = FRIBIDI_CHAR_SET_UTF8;
		
		// tell bidi that we need bidirectional
		FriBidiCharType Base = FRIBIDI_TYPE_L;
		
		// our buffer
		FriBidiChar *Logical = (FriBidiChar *)malloc(sizeof(FriBidiChar)*(len + 1));
		FriBidiChar *Visual = (FriBidiChar *)malloc(sizeof(FriBidiChar)*(len + 1));
		
		int RtlLen = fribidi_charset_to_unicode(fribidiCharset, const_cast<char *>(text), len, Logical);
		
		char * Rtl = NULL;
		
		// logical to visual
		if (fribidi_log2vis(Logical, RtlLen, &Base, Visual, NULL, NULL, NULL))
		{
			fribidi_remove_bidi_marks(Visual, RtlLen, NULL, NULL, NULL);

			Rtl = (char *)malloc(sizeof(char)*(RtlLen * 4 + 1));
			
			fribidi_unicode_to_charset(fribidiCharset, Visual, RtlLen, Rtl);
		}

		free(Logical);
     		free(Visual);
			
		return std::string(Rtl);
	}
	
	return std::string(text);
}

#define F_MUL 0x7FFF

//
void CFont::paintFontPixel(fb_pixel_t *td, uint8_t src)
{
#define DST_BLUE  0x80
#define DST_GREEN 0x80
#define DST_RED   0x80
#define DST_TRANS 0x80

	if (useFullBG)
	{
		uint8_t *dst = (uint8_t *)td;
		
		if (*td == (fb_pixel_t)0)
		{
			*dst = DST_BLUE  + ((fg_blue  - DST_BLUE)  * src) / 256;
			dst++;
			*dst = DST_GREEN + ((fg_green - DST_GREEN) * src) / 256;
			dst++;
			*dst = DST_RED   + ((fg_red   - DST_RED)   * src) / 256;
			dst++;
			*dst = (uint8_t)int_min(255, DST_TRANS + src);
		}
		else
		{
			*dst = *dst + ((fg_blue  - *dst) * src) / 256;
			dst++;
			*dst = *dst + ((fg_green - *dst) * src) / 256;
			dst++;
			*dst = *dst + ((fg_red   - *dst) * src) / 256;
			dst++;
			*dst = (uint8_t)int_min(0xFF, *dst + src);
		}
	}
	else
		*td = colors[src];
}

//
void CFont::RenderString(int x, int y, const int width, const char *text, const fb_pixel_t color, const int boxheight, bool utf8_encoded, const bool useBackground)
{
	if (!frameBuffer->getActive())
		return;

	fb_pixel_t *buff = NULL;
	int stride = 0;

	buff = (fb_pixel_t *)frameBuffer->getFrameBufferPointer();
	stride = frameBuffer->getStride();

	//
	useFullBG = useBackground;
	
	//
	// useFullBg = false
	// fetch bgcolor from framebuffer, using lower left edge of the font
	// - default render mode
	// - font rendering faster
	//
	// useFullBg = true
	// fetch bgcolor from framebuffer, using the respective real fontpixel position
	// - better quality at font rendering on images or background with color gradient
	// - font rendering slower
	//

	pthread_mutex_lock(&renderer->render_mutex);

	// fribidi
	std::string Text = fribidiShapeChar(text, utf8_encoded);
	text = Text.c_str();

	FT_Error err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "%s:FTC_Manager_LookupSize failed (0x%x)\n", __FUNCTION__, err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return;
	}
	face = size->face;

	int use_kerning = FT_HAS_KERNING(face);

	int left = x;
	int step_y = height;

	// ----------------------------------- box upper end (this is NOT a font metric, this is our method for y centering)
	//
	// **  -------------------------------- y=baseline-upper
	// ||
	// |u  --------------------*----------- y=baseline+ascender
	// |p                     * *
	// hp                    *   *
	// ee     *        *    *     *
	// ir      *      *     *******
	// g|       *    *      *     *
	// h|        *  *       *     *
	// t*  -------**--------*-----*-------- y=baseline
	// |l         *
	// |o        *
	// |w  -----**------------------------- y=baseline+descender   // descender is a NEGATIVE value
	// |r
	// **  -------------------------------- y=baseline+lower == YCALLER
	//
	// ----------------------------------- box lower end (this is NOT a font metric, this is our method for y centering)

	// height = ascender + -1*descender + linegap           // descender is negative!

	// now we adjust the given y value (which is the line marked as YCALLER) to be the baseline after adjustment:
	y -= lower;
	// y coordinate now gives font baseline which is used for drawing

	// caution: this only works if we print a single line of text
	// if we have multiple lines, don't use boxheight or specify boxheight==0.
	// if boxheight is !=0, we further adjust y, so that text is y-centered in the box
	if (boxheight)
	{
		if (boxheight > step_y)			// this is a real box (bigger than text)
			y -= ((boxheight - step_y) >> 1);
		else if (boxheight < 0)			// this normally would be wrong, so we just use it to define a "border"
			y += (boxheight >> 1);		// half of border value at lower end, half at upper end
	}

	int lastindex = 0; 	// 0 == missing glyph (never has kerning values)
	FT_Vector kerning;
	int pen1 = -1; 		// "pen" positions for kerning, pen2 is "x"

	fg_red     = (color & 0x00FF0000) >> 16;
	fg_green   = (color & 0x0000FF00) >>  8;
	fg_blue    = color  & 0x000000FF;
	fb_pixel_t bg_color = 0;

	if (y < 0)
		y = 0;

	//	
	if (!useFullBG)
	{
		// fetch bgcolor from framebuffer, using lower left edge of the font... 
		bg_color = *(buff + x + y * stride / sizeof(fb_pixel_t));

		if (bg_color == (fb_pixel_t)0)
			bg_color = 0x80808080;

		uint8_t bg_trans = (bg_color & 0xFF000000) >> 24;
		uint8_t bg_red   = (bg_color & 0x00FF0000) >> 16;
		uint8_t bg_green = (bg_color & 0x0000FF00) >>  8;
		uint8_t bg_blue  =  bg_color & 0x000000FF;
		
		for (int i = 0; i < 256; i++)
		{
			colors[i] = (((int_min(0xFF, bg_trans + i))           << 24) & 0xFF000000) |
				(((bg_red  + ((fg_red  - bg_red)  * i) / 256) << 16) & 0x00FF0000) |
				(((bg_green + ((fg_green - bg_green) * i) / 256) <<  8) & 0x0000FF00) |
				((bg_blue + ((fg_blue - bg_blue) * i) / 256)        & 0x000000FF);
		}
	}

	int spread_by = 0;
	if (stylemodifier == CFont::Embolden)
	{
		spread_by = (fontwidth / 6) - 1;
		if (spread_by < 1)
			spread_by = 1;
	}

	for (; *text; text++)
	{
		FTC_SBit glyph;

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

		if (*text == '\n')
		{
			/* a '\n' in the text is basically an error, it should not have come
			   until here. To find the offenders, we replace it with a paragraph
			   marker */
			unicode_value = 0x00b6; /* &para;  PILCROW SIGN */
			/* this did not work anyway - pen1 overrides x value down below :-)
			   and there are no checks that we don't leave our assigned box
			x  = left;
			y += step_y;
			 */
		}

		int index = FT_Get_Char_Index(face, unicode_value);

		if (!index)
			continue;
			
		if (getGlyphBitmap(index, &glyph))
		{
			dprintf(DEBUG_NORMAL, "failed to get glyph bitmap.\n");
			continue;
		}

		//kerning
		if (use_kerning)
		{
			FT_Get_Kerning(face, lastindex, index, 0, &kerning);
			x += (kerning.x) >> 6; // kerning!
		}

		int xoff    = x + glyph->left;
		int ap      = xoff * sizeof(fb_pixel_t) + stride * (y - glyph->top);
		uint8_t *d  = ((uint8_t *)buff) + ap;
		uint8_t *s  = glyph->buffer;
		int w       = glyph->width;
		int h       = glyph->height;
		int pitch   = glyph->pitch;
		
		if (ap > -1)
		{
			for (int ay = 0; ay < h; ay++)
			{
				fb_pixel_t *td = (fb_pixel_t *)d;
				int ax;
				
				for (ax = 0; ax < w + spread_by; ax++)
				{
					// width clip
					if (xoff  + ax >= left + width)
						break;
						
					if (stylemodifier != CFont::Embolden)
					{
						// do not paint the backgroundcolor (*s = 0)
						if (*s != 0)
							paintFontPixel(td, *s);
					}
					else
					{
						int lcolor = -1;
						int start = (ax < w) ? 0 : ax - w + 1;
						int end   = (ax < spread_by) ? ax + 1 : spread_by + 1;
						
						for (int i = start; i < end; i++)
						{
							if (lcolor < *(s - i))
							{
								lcolor = *(s - i);
							}
						}
						
						// do not paint the backgroundcolor (lcolor = 0)
						if (lcolor != 0)
						{
							paintFontPixel(td, (uint8_t)lcolor);
						}
					}
					td++;
					s++;
				}
				s += pitch - ax;
				d += stride;
			}
		}
		x += glyph->xadvance + 1;
		
		if (pen1 > x)
			x = pen1;
		pen1 = x;
		lastindex = index;
	}
	
	pthread_mutex_unlock(&renderer->render_mutex);
}

void CFont::RenderString(int x, int y, const int width, const std::string &text, const fb_pixel_t color, const int boxheight, bool utf8_encoded, const bool useBackground)
{
	RenderString(x, y, width, text.c_str(), color, boxheight, utf8_encoded, useBackground);
}

int CFont::getRenderWidth(const char* text, bool utf8_encoded)
{
	pthread_mutex_lock( &renderer->render_mutex );
	
	// fribidi	
	std::string Text = fribidiShapeChar(text, utf8_encoded);
	text = Text.c_str();	

	FT_Error err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "%s:FTC_Manager_LookupSize failed (0x%x)\n", __FUNCTION__, err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return -1;
	}
	
	face = size->face;

	int use_kerning = FT_HAS_KERNING(face);
	int x = 0;
	int lastindex = 0; 	// 0==missing glyph (never has kerning)
	FT_Vector kerning;
	int pen1 = -1; 		// "pen" positions for kerning, pen2 is "x"
	
	for (; *text; text++)
	{
		FTC_SBit glyph;

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;			

		int index = FT_Get_Char_Index(face, unicode_value);

		if (!index)
			continue;
		
		if (getGlyphBitmap(index, &glyph))
		{
			dprintf(DEBUG_NORMAL, "CFont::getRenderWidth: failed to get glyph bitmap.\n");
			continue;
		}
		
		//kerning
		if(use_kerning)
		{
			FT_Get_Kerning(face, lastindex, index, 0, &kerning);
			x += (kerning.x) >> 6; // kerning!
		}

		x += glyph->xadvance+1;
		if(pen1 > x)
			x = pen1;
		pen1 = x;
		lastindex = index;
	}

	if (stylemodifier == CFont::Embolden)
	{
		int spread_by = (fontwidth / 6) - 1;
		if (spread_by < 1)
			spread_by = 1;

		x += spread_by;
	}

	pthread_mutex_unlock( &renderer->render_mutex );

	return x;
}

int CFont::getRenderWidth(const std::string & text, bool utf8_encoded)
{
	return getRenderWidth(text.c_str(), utf8_encoded);
}

