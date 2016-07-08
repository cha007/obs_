/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

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
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "OBSApi.h"


struct CCStruct
{
    DWORD structSize;       //size of this structure
    DWORD curColor;         //current color stored
    BOOL  bDisabled;        //whether the control is disabled or not

    long  cx,cy;            //size of control

    DWORD custColors[16];   //custom colors in the colors dialog box
};



void WINAPI DrawColorControl(HDC hDC, CCStruct *pCCData);
LRESULT WINAPI ColorControlProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);


static DWORD customColors[16] = {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                                 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                                 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                                 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF};

void CCSetCustomColors(DWORD *colors)
{
    memcpy(customColors, colors, sizeof(DWORD)*16);
}

void CCGetCustomColors(DWORD *colors)
{
    memcpy(colors, customColors, sizeof(DWORD)*16);
}

void WINAPI DrawColorControl(HDC hDC, CCStruct *pCCData)
{
    HDC hdcTemp;
    HBITMAP hBmp, hbmpOld;
    HBRUSH hBrush;
    RECT rect;

    //Create temp draw data
    hdcTemp = CreateCompatibleDC(hDC);
    hBmp = CreateCompatibleBitmap(hDC, pCCData->cx, pCCData->cy);
    hbmpOld = (HBITMAP)SelectObject(hdcTemp, hBmp);


    //draw the outline
    hBrush = CreateSolidBrush(INVALID);

    rect.top    = rect.left = 0;
    rect.right  = pCCData->cx;
    rect.bottom = pCCData->cy;
    FillRect(hdcTemp, &rect, hBrush);

    DeleteObject(hBrush);


    //draw the color
    hBrush = CreateSolidBrush(pCCData->bDisabled ? 0x808080 : pCCData->curColor);

    rect.top    = rect.left = 1;
    rect.right  = pCCData->cx-1;
    rect.bottom = pCCData->cy-1;
    FillRect(hdcTemp, &rect, hBrush);

    DeleteObject(hBrush);


    //Copy drawn data back onto the main device context
    BitBlt(hDC, 0, 0, pCCData->cx, pCCData->cy, hdcTemp, 0, 0, SRCCOPY);

    //Delete temp draw data
    SelectObject(hdcTemp, hbmpOld);
    DeleteObject(hBmp);
    DeleteDC(hdcTemp);
}

DWORD CCGetColor(HWND hwnd)
{
    if(!hwnd)
        return 0;

    CCStruct *pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

    DWORD color = REVERSE_COLOR(pCCData->curColor); //MAKERGB(RGB_B(pCCData->curColor), RGB_G(pCCData->curColor), RGB_R(pCCData->curColor));

    return color | 0xFF000000;
}

void  CCSetColor(HWND hwnd, DWORD color)
{
    if(!hwnd)
        return;

    color &= 0xFFFFFF;

    color = REVERSE_COLOR(color);

    CCStruct *pCCData = (CCStruct*)GetWindowLongPtr(hwnd, 0);

    pCCData->curColor = color;

    HDC hDC = GetDC(hwnd);
    DrawColorControl(hDC, pCCData);
    ReleaseDC(hwnd, hDC);
}

void  CCSetColor(HWND hwnd, const Color3 &color)
{
    CCSetColor(hwnd, (DWORD)Vect_to_RGB(color));
}
