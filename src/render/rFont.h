/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

***************************************************************************

*/

#ifndef ArmageTron_FONT_H
#define ArmageTron_FONT_H

#include "rSDL.h"

#include "defs.h"
//#include "rTexture.h"
#include "tString.h"
#include "rColor.h"
#include "tCoord.h"

#include <map>
#include <vector>

#if defined(HAVE_FTGL_FTGL_H) || defined(HAVE_FTGL_H)
// also, the installed version of ftgl then has utf8 support. presumably. use it.
#define FTGL_HAS_UTF8
#else
// alternative includes go here
#endif

// defines for interface with FTGL
#ifdef FTGL_HAS_UTF8
// typedefs for chars and strings
#define FTGL_CHAR char
#define FTGL_STRING std::string
// macro expecting a tString as 'in' and producing at least a 'const FTGL_STRING &' as out, living at least as long as in does.
#define STRING_TO_FTGL( in, out ) FTGL_STRING const & out = in
#else
// description: see other branch
#define FTGL_CHAR wchar_t
#define FTGL_STRING std::wstring
// function to convert utf8 to wchar strings. Only needed if utf8 is not supported natively.
void sr_utf8216(tString const &in, std::wstring &out);
#define STRING_TO_FTGL( in, out ) FTGL_STRING out;  sr_utf8216(in, out)
#endif // FTGL_HAS_UTF8

//! Different types of fonts, they might get different font files assigned
enum sr_fontClass {
    sr_fontConsole       = 00001, //!< for the console
    sr_fontMenu          = 00002, //!< for menu text
    sr_fontMenuTitle     = 00004, //!< for the titles above menus
    sr_fontScoretable    = 00010, //!< for the score table (the one that comes when you press TAB)
    sr_fontMenuHelp      = 00020, //!< for the help that pops up if you idle in the menu
    sr_fontError         = 00040, //!< for error messages like the one that pops up if you get kicked from the server
    sr_fontCockpit       = 00100, //!< for all font in the cockpit
    sr_fontCenterMessage = 00200, //!< for CENTER_MESSAGEs
    sr_fontServerBrowser = 00400, //!< for the LAN and master server browser
    sr_fontCycleLabel    = 01000, //!< for the player name displayed over cycles
    sr_fontServerDetails = 02000  //!< for the details displayed in the server browser (server description, player names etc)
};

//! Different ways to render fonts
enum sr_fontTypes {
    sr_fontOld       = 0, //!< The old font, deprecated
    sr_fontPixmap    = 1, //!< FTGLPixmapFont
    sr_fontBitmap    = 2, //!< FTGLBitmapFont
    sr_fontTexture   = 3, //!< FTGLTextureFont
    sr_fontPolygon   = 4, //!< FTGLPolygonFont
    sr_fontOutline   = 5, //!< FTGLOutlineFont
    sr_fontExtruded  = 6  //!< FTGLExtrdFont (experimental)
};

extern int sr_fontType;

class FTFont;

// maybe make this a child of std::ostream...
class rTextField{
    FTGL_STRING buffer;       // buffer where we store stuff before we print it
    float  width;          // width in openGL units
    int  parIndent;      // number of spaces to insert after automatic newline
    REAL left,top;       // top left corner of the console
    REAL cheight; // character dimensions
    //    rFont *F;             // the font
    int  x,y,realx;      // current cursor position
    float nextx;          // x-coordinate the next char should go to for rendering
    float currentWidth;   //The current position where the next char will go to for caching
    bool multiline;        // linewrapping enabled?

    tColor color_;               //!< current color
    static tColor defaultColor_; //!< default color
    static tColor blendColor_;   //!< color all other colors are multiplied with

    int cursor; // display mode of the cursor; 0: disabled, 1: low, 2: high
    int cursorPos; // position of the cursor (number of FTGL_CHARs to come)

    REAL cursor_x,cursor_y; // position on the screen

    void FlushLine(int len,bool newline=true);
    void FlushLine(bool newline=true);
public:
#define  rCWIDTH_NORMAL  (16/640.0)
#define  rCHEIGHT_NORMAL (32/480.0)

    rTextField(REAL Left,REAL Top,
               REAL Cheight, sr_fontClass Type);

    // all the basic code assumes a 4:3 screen. We won't fix that here on 0.2.9.
    // instead, users need to either multiply their witdh (and maybe left) with
    // AspectWidthMultiplier() or their height (and Top) with AspectHeightMultiplier().
    // Pick one for each context and stick with it. The Width modification should be
    // the default.
    static REAL AspectWidthMultiplier();
    static REAL AspectHeightMultiplier();

    // puts x/y right into the middle of a screen pixel given Width or Height.
    static REAL Pixelize(REAL xy, int WidthHeight);

    virtual ~rTextField(); // for future extensions (buffered console?)

    REAL GetCHeight() const {
        return cheight;
    }

   void SetTop( REAL t ){
        top = t;
    }

    void SetLeft( REAL l ){
        top = l;
    }
    
    REAL GetTop() const{
        return top;
    }
    REAL GetBottom() const{
        return top - cheight * y;
    }


    REAL GetLeft() const{
        return left;
    }

    void SetWidth(float w){
        width=w;
    }

    float GetWidth() const {
        return width;
    }

    void SetIndent(int i){
        parIndent=i;
    }

    int GetIndent() const {
        return parIndent;
    }

    // sets the cursor mode position. Cursor position is in FTGL_CHARs, so not true characters in utf8 mode.
    void SetCursor(int c,int p){
        cursor=c;
        cursorPos=p;
    }

    void ResetColor(){
        FlushLine(false);
        color_ = defaultColor_;
    }

    void EnableLineWrap() {
        multiline = true;
    }

    // rTextField & operator<<(unsigned char c);

    enum ColorMode {
        COLOR_IGNORE,   // ignore color codes, printing everything verbatim
        COLOR_USE,      // normal mode: hide color codes and use them
        COLOR_SHOW      // use color codes, but print them as well
    };

    rTextField & StringOutput(const FTGL_CHAR *c, ColorMode colorMode = COLOR_USE );

    int Lines(){
        return y;
    }

    inline rTextField & SetColor( tColor const & color );	//!< Sets current color
    inline tColor const & GetColor( void ) const;	//!< Gets current color
    inline rTextField const & GetColor( tColor & color ) const;	//!< Gets current color
    static void SetDefaultColor( tColor const & defaultColor );	//!< Sets default color
    static tColor const & GetDefaultColor( void );	//!< Gets default color
    static void GetDefaultColor( tColor & defaultColor );	//!< Gets default color
    static void SetBlendColor( tColor const & blendColor );	//!< Sets color all other colors are multiplied with
    static tColor const & GetBlendColor( void );	//!< Gets color all other colors are multiplied with
    static void GetBlendColor( tColor & blendColor );	//!< Gets color all other colors are multiplied with

    static float GetTextLength (std::string const &str, float height, bool stripColors=false, bool useNewline=true, float *resultingHeight=0); //!< Predict the dimenstions of a string
    static float GetTextLengthRaw (FTGL_STRING const &str, float height, bool useNewline=true, float *resultingHeight=0); //!< Predict the dimenstions of a string

private:
    inline void WriteChar(FTGL_CHAR c); //!< writes a single character as it is, no automatic newline breaking
};

//! Text box class with wrapping: unlike rTextField this is designed to stay in memory all the time and only re- wrap/interpret colors if the contents change
class rTextBox {
public:
    rTextBox(float size, tCoord const &pos, float width); //!< default constructor
    void SetText(tString const &); //!< changes the text contained in the text box and re- computes contents
    void Render(void) const; //!< renders the text
private:
    //! class for single font items (one line, one color)
    class item {
        tCoord m_pos; //!< the top- left position of the text
        float m_size; //!< the color to be used
        FTGL_STRING m_text; //!< the text to be rendered
        rColor m_color; //!< the color to be used
    public:
        inline item(tCoord const &pos, float size, FTGL_STRING const &text, tColor const &color); //!< default constructor
        void Render(void) const; //!< renders the text
    };
    float m_size; //!< the size of the font to be used
    tCoord m_pos; //!< the top-left corner of the font
    float m_width; //!< the width to be wrapped at

    std::vector<item> m_items; //!< the split- up font items
};

//! @param pos the top- left position of the text
//! @param size the height of the text
//! @param text the text to be rendered
//! @param color the color to be used
inline rTextBox::item::item(tCoord const &pos, float size, FTGL_STRING const &text, tColor const &color) :
        m_pos(pos),
        m_size(size),
        m_text(text),
        m_color(color)
{}

rTextField & operator<<(rTextField &c,const FTGL_STRING &x);
template<class T> rTextField & operator<<(rTextField &c,const T &x){
    tColoredString out;
    out << x;
    STRING_TO_FTGL( out, str );
    return c.StringOutput(str.c_str());
}

void DisplayText(REAL x,REAL y,REAL h,const char *text, sr_fontClass type,int center=0,
                 int cursor=0,int cursorPos=0, rTextField::ColorMode colorMode = rTextField::COLOR_USE );

void DisplayTextAutoWidth(REAL x,REAL y,const char *text,REAL h=rCHEIGHT_NORMAL,int center=0,
                 int cursor=0,int cursorPos=0, rTextField::ColorMode colorMode = rTextField::COLOR_USE );

void DisplayTextAutoHeight(REAL x,REAL y,const char *text,REAL w=rCWIDTH_NORMAL,int center=0,
                 int cursor=0,int cursorPos=0, rTextField::ColorMode colorMode = rTextField::COLOR_USE );

// *******************************************************************************************
// *
// *	GetColor
// *
// *******************************************************************************************
//!
//!		@return		current color
//!
// *******************************************************************************************

tColor const & rTextField::GetColor( void ) const
{
    return this->color_;
}

// *******************************************************************************************
// *
// *	GetColor
// *
// *******************************************************************************************
//!
//!		@param	color	current color to fill
//!		@return		A reference to this to allow chaining
//!
// *******************************************************************************************

rTextField const & rTextField::GetColor( tColor & color ) const
{
    color = this->color_;
    return *this;
}

// *******************************************************************************************
// *
// *	SetColor
// *
// *******************************************************************************************
//!
//!		@param	color	current color to set
//!		@return		A reference to this to allow chaining
//!
// *******************************************************************************************

rTextField & rTextField::SetColor( tColor const & color )
{
    this->color_ = color;
    return *this;
}
//! Reloads the font (in case the resolution or font type changes)
void sr_ReloadFont(void);

#endif

