#!/usr/bin/env python3
"""Render actual startup functions/font with a host matrix to check color/placement."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
font = (root / 'src/PixelItFont.cpp').read_text()
font = font[font.index('const uint8_t PixelItBitmaps'):font.index('const uint8_t LargePixels')]
# Stop after the first font; the following fonts are unrelated.
font = font[:font.index('const GFXfont PixelItFont')]+font[font.index('const GFXfont PixelItFont'):].split(';', 1)[0]+';'
visuals = (root / 'src/UpdateVisuals.cpp').read_text()
visuals = '\n'.join(line for line in visuals.splitlines() if not line.startswith('#include'))
hardware = (root / 'src/Hardware.cpp').read_text()
connecting = hardware[hardware.index('namespace\n{'):]
effects = (root / 'src/Effects.cpp').read_text()
boot = effects[effects.index('namespace\n{\nvoid ApplyBootLogoColors()'):effects.index('void ShowBatteryScreen()')]
program = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "UpdateVisuals.h"
#define PROGMEM
#define pgm_read_byte(p) (*(p))
#define pgm_read_word(p) (*(p))
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
struct GFXglyph { uint16_t bitmapOffset; uint8_t width,height,xAdvance; int8_t xOffset,yOffset; };
struct GFXfont { uint8_t *bitmap; GFXglyph *glyph; uint16_t first,last; uint8_t yAdvance; };
''' + font + r'''
struct CRGB { uint8_t r=0,g=0,b=0; };
CRGB leds[256];
bool reversed = false;
struct Matrix {
    int cx=0,cy=0,shows=0,prints=0; uint16_t color=0; bool bootCheck=false;
    int XY(int x,int y) { return y*32 + ((reversed && (y&1)) ? 31-x : x); }
    uint16_t Color(int r,int g,int b) {return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
    void clear() { for(auto &p:leds) p=CRGB(); }
    void setFont(const GFXfont*) {}
    void setTextWrap(bool) {}
    void setTextColor(uint16_t c) { color=c; }
    void setCursor(int x,int y) { cx=x;cy=y; }
    void drawPixel(int x,int y,uint16_t c) {
        if(x<0||x>=32||y<0||y>=8) return;
        auto &p=leds[XY(x,y)]; p.r=((c>>11)&31)*255/31;p.g=((c>>5)&63)*255/63;p.b=(c&31)*255/31;
    }
    uint16_t pixel(int x,int y) {auto &p=leds[XY(x,y)];return Color(p.r,p.g,p.b);}
    void print(const char *s) {
        ++prints;
        for(;*s;++s) {
            const auto &g=PixelItFont.glyph[uint8_t(*s)-PixelItFont.first];
            for(int y=0;y<g.height;++y) for(int x=0;x<g.width;++x) {
                int bit=y*g.width+x;
                if(PixelItFont.bitmap[g.bitmapOffset+bit/8] & (0x80>>(bit%8)))
                    drawPixel(cx+g.xOffset+x,cy+g.yOffset+y,color);
            }
            cx+=g.xAdvance;
        }
    }
    void show() {
        ++shows;
        if(bootCheck) for(int y=0;y<8;++y) for(int x=0;x<32;++x)
            if(pixel(x,y)) assert(pixel(x,y)==(x>=22?Color(255,255,255):LogoGradientColor(x<4?0:x-4,23)));
    }
} display;
Matrix *matrix=&display;
bool wifiScreenAktiv=true;
struct WiFiManager {};
#define F(x) x
void Log(const char*,const char*) {}
int16_t connectingBarX=0,connectingBarDir=1;
const uint8_t CONNECTING_TRAIL_LEN=6;
const uint8_t CONNECTING_TRAIL_BRIGHTNESS[]={255,180,110,60,25,10};
int tickerMs=0; void (*tickerFn)()=nullptr;
struct Ticker { void detach(){tickerMs=0;tickerFn=nullptr;} void attach_ms(int ms,void(*fn)()){tickerMs=ms;tickerFn=fn;} } connectingAnimTicker;
unsigned delayTotal=0;
void delay(unsigned ms){delayTotal+=ms;}
void FadeIn(){matrix->show();}
void DrawTextHelper(const char *s,int,bool,bool,bool,bool,int r,int g,int b,int x,int y) {
    matrix->setTextColor(matrix->Color(r,g,b));matrix->setCursor(x,y+5);matrix->print(s);
}
''' + visuals + connecting + boot + r'''
void checkCenter() {
    int left=32,right=-1,top=8,bottom=-1;
    for(int y=0;y<7;++y) for(int x=0;x<32;++x) if(matrix->pixel(x,y)) {
        left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
    }
    assert(right>=left);
    assert(std::abs(left-(31-right))<=1);
    assert(std::abs(top-(6-bottom))<=1);
}
int main() {
    assert(LogoGradientColor(0,23)==matrix->Color(128,0,255));
    assert(LogoGradientColor(22,23)==matrix->Color(255,65,190));
    assert(LogoGradientColor(22,23,0)==0);
    wifiScreenAktiv=false;
    ShowConnectingScreen();
    assert(tickerMs==0 && tickerFn==nullptr);
    const int hiddenShows=display.shows,hiddenPrints=display.prints;
    ConnectingAnimationTick();
    EnteredHotspotCallback(nullptr);
    assert(display.shows==hiddenShows && display.prints==hiddenPrints);
    for(auto &pixel:leds) assert(pixel.r==0 && pixel.g==0 && pixel.b==0);
    wifiScreenAktiv=true;
    for(bool mapping:{false,true}) {
        reversed=mapping; matrix->clear(); delayTotal=0; display.bootCheck=true;
        ShowBootAnimation(); assert(delayTotal==2300);display.bootCheck=false;
        connectingTextPlaced=false;
        ShowConnectingScreen();checkCenter();
        assert(tickerMs==60 && tickerFn==ConnectingAnimationTick);
        assert(matrix->pixel(0,7)==LogoGradientColor(0,6));
        const int initialX=connectingTextX,initialY=connectingTextY;
        for(int frame=0;frame<80;++frame) {
            int before=display.prints;
            ConnectingAnimationTick();checkCenter();
            assert(display.prints==before+1); // Ink measurement is cached.
            assert(connectingTextX==initialX && connectingTextY==initialY);
            assert(connectingBarX>=0 && connectingBarX<32);
            for(int i=0;i<6;++i) {
                int x=connectingBarX-connectingBarDir*i;
                if(x>=0&&x<32) assert(matrix->pixel(x,7)==LogoGradientColor(i,6,CONNECTING_TRAIL_BRIGHTNESS[i]));
            }
        }
    }
}
'''
with tempfile.TemporaryDirectory() as temp:
    path=Path(temp)
    (path/'startup.cpp').write_text(program)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-I',str(root/'include'),str(path/'startup.cpp'),'-o',str(path/'startup')],check=True)
    subprocess.run([str(path/'startup')],check=True)
print('Boot palette and WiFi ink-centering/trail tests passed')
