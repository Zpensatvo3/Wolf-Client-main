#pragma once
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace launcher::ui {
    using namespace Gdiplus;
    inline constexpr int width=760, height=430;
    struct layout {
        RectF card{380,78,354,304};
        RectF key{404,142,306,52}, action{404,216,306,55}, status{404,294,306,74};
        RectF minimize{664,15,32,30}, close{706,15,32,30}, signout{577,15,76,30};
        RectF input() const { return {key.X+44,key.Y+16,key.Width-86,22}; }
        RectF eye() const { return {key.GetRight()-40,key.Y+7,32,key.Height-14}; }
    };
    inline const layout bounds;
    enum class target { none, input, eye, action, minimize, close, signout };
    inline target hit(float x,float y,bool login,bool signed_in,bool busy) {
        if (bounds.close.Contains(x,y)) return target::close;
        if (bounds.minimize.Contains(x,y)) return target::minimize;
        if (busy) return target::none;
        if (signed_in && bounds.signout.Contains(x,y)) return target::signout;
        if (bounds.action.Contains(x,y)) return target::action;
        if (login && bounds.eye().Contains(x,y)) return target::eye;
        if (bounds.key.Contains(x,y)) return target::input;
        return target::none;
    }
    inline Color foreground{255,247,247,250}, muted{255,166,170,188}, pink{255,236,79,132};
    inline void rounded_path(GraphicsPath& path,RectF r,float radius) {
        const float d=(std::min)(radius*2,(std::min)(r.Width,r.Height));
        path.AddArc(r.X,r.Y,d,d,180,90); path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
        path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90); path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);
        path.CloseFigure();
    }
    inline void panel(Graphics& g,RectF r,float radius,Color a,Color b,Color edge) {
        GraphicsPath path; rounded_path(path,r,radius);
        LinearGradientBrush brush(r,a,b,LinearGradientModeForwardDiagonal);
        Pen border(edge,1); g.FillPath(&brush,&path); g.DrawPath(&border,&path);
    }
    inline void text(Graphics& g,const std::wstring& value,RectF rect,float size,Color color,
        bool center=false,bool bold=false,bool wrap=false) {
        Font font(bold ? L"Segoe UI Semibold" : L"Segoe UI",size,FontStyleRegular,UnitPixel);
        SolidBrush brush(color); StringFormat format;
        format.SetAlignment(center ? StringAlignmentCenter : StringAlignmentNear);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetFormatFlags(wrap ? StringFormatFlagsLineLimit : StringFormatFlagsNoWrap);
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(value.c_str(),-1,&font,rect,&format,&brush);
    }
    inline float advance(Graphics& g,const std::wstring& value,float size) {
        Font font(L"Segoe UI Semibold",size,FontStyleRegular,UnitPixel);
        StringFormat format(StringFormat::GenericTypographic());
        format.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsMeasureTrailingSpaces);
        RectF measured; g.MeasureString(value.c_str(),-1,&font,PointF(0,0),&format,&measured);
        return measured.Width;
    }
    inline void heading(Graphics& g,const std::wstring& a,const std::wstring& b,float x,float y,float maximum) {
        float size=42;
        while (size>32 && advance(g,a+b,size)>maximum) size-=.5f;
        Font font(L"Segoe UI Semibold",size,FontStyleRegular,UnitPixel);
        StringFormat format(StringFormat::GenericTypographic()); format.SetFormatFlags(StringFormatFlagsNoWrap);
        SolidBrush white(foreground),accent(pink);
        g.DrawString(a.c_str(),-1,&font,PointF(x,y),&format,&white);
        g.DrawString(b.c_str(),-1,&font,PointF(x+advance(g,a,size),y),&format,&accent);
    }
    inline void glow(Graphics& g,RectF r,Color color) {
        GraphicsPath path; path.AddEllipse(r); PathGradientBrush brush(&path);
        brush.SetCenterColor(color); Color transparent(0,color.GetR(),color.GetG(),color.GetB()); int count=1;
        brush.SetSurroundColors(&transparent,&count); g.FillPath(&brush,&path);
    }
    inline void background(Graphics& g) {
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        RectF area(0,0,float(width),float(height));
        LinearGradientBrush base(area,Color(255,12,13,21),Color(255,10,11,18),LinearGradientModeForwardDiagonal);
        g.FillRectangle(&base,area);
        glow(g,RectF(-160,0,590,560),Color(24,231,42,110));
        RectF sphere(-345,76,680,680); GraphicsPath disc; disc.AddEllipse(sphere);
        LinearGradientBrush wash(sphere,Color(255,48,17,34),Color(0,12,13,21),LinearGradientModeVertical);
        const REAL fade[]{0.f,.80f,1.f,1.f},positions[]{0.f,.30f,.55f,1.f};
        wash.SetBlend(fade,positions,4);
        g.FillPath(&wash,&disc);
        for (int layer=4;layer>=0;--layer) {
            LinearGradientBrush light(sphere,Color(BYTE(layer ? 8 : 85),238,54,121),Color(0,238,54,121),LinearGradientModeVertical);
            light.SetBlend(fade,positions,4);
            Pen edge(&light,layer ? float(layer*3) : .9f); g.DrawArc(&edge,sphere,260,100);
        }
        GraphicsPath frame; rounded_path(frame,RectF(.5f,.5f,width-1.f,height-1.f),13);
        Pen border(Color(255,67,66,82),1); g.DrawPath(&border,&frame);
    }
    inline void key_icon(Graphics& g,float x,float y,Color color) {
        Pen pen(color,1.7f); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        g.DrawEllipse(&pen,x+2,y-7,8.f,8.f); g.DrawLine(&pen,x+3,y,x-6,y+9);
        g.DrawLine(&pen,x-6,y+9,x-9,y+6); g.DrawLine(&pen,x-3,y+6,x-5,y+4);
    }
    inline void eye_icon(Graphics& g,RectF r,bool reveal,Color color) {
        Pen pen(color,1.5f); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        const float x=r.X+r.Width*.5f,y=r.Y+r.Height*.5f;
        GraphicsPath path; path.AddBezier(x-8,y,x-3,y-7,x+3,y-7,x+8,y);
        path.AddBezier(x+8,y,x+3,y+7,x-3,y+7,x-8,y); g.DrawPath(&pen,&path);
        g.DrawEllipse(&pen,x-2.5f,y-2.5f,5.f,5.f);
        if (!reveal) g.DrawLine(&pen,x-6,y+7,x+7,y-7);
    }
    inline void cube_icon(Graphics& g,float x,float y,Color color) {
        Pen pen(color,1.7f); PointF points[]{{x,y-9},{x+9,y-4},{x+9,y+6},{x,y+11},{x-9,y+6},{x-9,y-4},{x,y-9}};
        g.DrawLines(&pen,points,7); g.DrawLine(&pen,x,y+1,x,y+11);
        g.DrawLine(&pen,x-9,y-4,x,y+1); g.DrawLine(&pen,x,y+1,x+9,y-4);
    }
    inline void arrow(Graphics& g,float x,float y,Color color) {
        Pen pen(color,1.6f); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        g.DrawLine(&pen,x-6,y,x+6,y); g.DrawLine(&pen,x+1,y-5,x+6,y); g.DrawLine(&pen,x+1,y+5,x+6,y);
    }
    struct presentation {
        bool login=true, signed_in=false, busy=false, done=false, error=false, reveal=false, input_focused=false;
        target hover=target::none, keyboard=target::none;
        std::wstring message=L"Ready to sign in.", selected=L"Minecraft Window";
        uint64_t received=0,total=0;
        float animation=0;
    };
    inline void draw(Graphics& g,const presentation& v) {
        background(g);
        glow(g,RectF(27,21,18,18),Color(30,239,72,131));
        LinearGradientBrush dot(PointF(32,25),PointF(40,36),Color(255,250,94,149),Color(255,220,45,105));
        g.FillEllipse(&dot,32.f,25.f,12.f,12.f);
        text(g,L"OneStep",RectF(58,17,100,28),16,foreground,false,true);
        if (v.signed_in) text(g,L"Sign out",bounds.signout,12.5f,v.busy ? Color(255,91,92,109) : v.hover==target::signout ? foreground : muted,true);
        for (const auto item:{target::minimize,target::close}) if (v.hover==item)
            panel(g,item==target::close ? bounds.close : bounds.minimize,7,Color(255,34,29,42),Color(255,26,24,34),Color(0,0,0,0));
        Pen chrome(muted,1.6f); g.DrawLine(&chrome,675.f,30.f,685.f,30.f);
        g.DrawLine(&chrome,717.f,25.f,727.f,35.f); g.DrawLine(&chrome,727.f,25.f,717.f,35.f);
        if (v.login) {
            heading(g,L"Welcome ",L"back.",40,169,316);
            text(g,L"Enter your license key to continue.",RectF(42,229,316,34),16,muted);
        } else {
            heading(g,L"Choose",L"",40,153,316);
            heading(g,L"your ",L"session.",40,202,316);
            text(g,L"Select a window and launch.",RectF(42,267,316,34),16,muted);
        }
        panel(g,bounds.card,15,Color(255,19,21,30),Color(255,15,17,25),Color(255,44,46,60));
        text(g,v.login ? L"License Key" : L"Session",RectF(404,102,306,26),16,foreground,false,true);
        panel(g,bounds.key,11,Color(255,23,25,36),Color(255,23,25,36),
            v.input_focused || v.hover==target::input || v.hover==target::eye ? Color(255,163,65,108) : Color(255,47,50,66));
        const auto icon=v.busy ? Color(255,113,63,89) : pink;
        if (v.login) {
            key_icon(g,bounds.key.X+25,bounds.key.Y+25,icon);
            eye_icon(g,bounds.eye(),v.reveal,v.busy ? Color(255,92,93,108) : v.hover==target::eye ? foreground : muted);
        } else {
            cube_icon(g,bounds.key.X+25,bounds.key.Y+25,icon);
            text(g,v.selected,RectF(bounds.key.X+54,bounds.key.Y+9,bounds.key.Width-94,34),15,v.busy ? muted : foreground,false,true);
            Pen chevron(muted,1.7f); const float x=bounds.key.GetRight()-24,y=bounds.key.Y+25;
            g.DrawLine(&chevron,x-5,y-2,x,y+3); g.DrawLine(&chevron,x,y+3,x+5,y-2);
        }
        if (!v.busy && !v.done) for (int i=4;i>0;--i) {
            GraphicsPath halo; auto r=bounds.action; r.Inflate(float(i*3),float(i*3)); rounded_path(halo,r,11+float(i));
            Pen light(Color(BYTE(6-i),236,50,117),3.f); g.DrawPath(&light,&halo);
        }
        const bool enabled=!v.busy && !v.done;
        const bool highlight=v.hover==target::action || v.keyboard==target::action;
        panel(g,bounds.action,11,enabled ? Color(255,highlight ? 251 : 245,highlight ? 105 : 86,145) : Color(255,77,36,57),
            enabled ? Color(255,218,44,104) : Color(255,65,30,49),enabled ? Color(255,251,112,158) : Color(255,102,51,76));
        const auto label=v.busy ? (v.login ? L"Signing In..." : L"Launching...") : v.done ? L"Client is running" : v.login ? L"Sign In" : L"Launch Client";
        const float label_width=advance(g,label,17),group_width=label_width+(enabled ? 30 : 0);
        const float group_x=bounds.action.X+(bounds.action.Width-group_width)*.5f;
        Font button_font(L"Segoe UI Semibold",17,FontStyleRegular,UnitPixel);
        StringFormat button_format(StringFormat::GenericTypographic()); button_format.SetFormatFlags(StringFormatFlagsNoWrap);
        SolidBrush button_text(enabled ? foreground : Color(255,186,163,177));
        g.DrawString(label,-1,&button_font,PointF(group_x,bounds.action.Y+(bounds.action.Height-button_font.GetHeight(&g))*.5f),&button_format,&button_text);
        if (enabled) arrow(g,group_x+label_width+23,bounds.action.Y+28,foreground);
        panel(g,bounds.status,11,Color(255,20,22,31),Color(255,19,21,30),Color(255,42,45,59));
        const Color status_color=v.error ? Color(255,255,108,134) : v.busy ? Color(255,246,185,97) : Color(255,24,236,145);
        glow(g,RectF(412,303,28,28),Color(55,status_color.GetR(),status_color.GetG(),status_color.GetB()));
        SolidBrush status_dot(status_color); g.FillEllipse(&status_dot,421.f,313.f,10.f,10.f);
        text(g,L"Status",RectF(443,302,247,23),13,foreground);
        text(g,v.message,RectF(443,323,247,v.busy ? 28.f : 39.f),13.5f,muted,false,false,true);
        if (v.busy) {
            const RectF track(443,353,247,2); SolidBrush rail(Color(255,47,43,58)),progress(pink);
            g.FillRectangle(&rail,track);
            if (v.total) {
                auto filled=track; filled.Width*=static_cast<float>(std::clamp(double(v.received)/double(v.total),0.,1.));
                if (filled.Width>0) g.FillRectangle(&progress,filled);
            } else {
                const auto saved=g.Save(); g.SetClip(track);
                g.FillRectangle(&progress,track.X+std::fmod(v.animation*.6f,1.f)*(track.Width+70)-70,track.Y,70.f,track.Height);
                g.Restore(saved);
            }
        }
        if (v.keyboard==target::eye) {
            GraphicsPath path; rounded_path(path,bounds.eye(),5); Pen focus(pink,1); g.DrawPath(&focus,&path);
        }
    }
}
