/*
 * NetClean VideoPlayer
 *  Multi process video player for windows.
 *
 * Copyright (c) 2010-2013 NetClean Technologies AB
 * All Rights Reserved.
 *
 * This file is part of NetClean VideoPlayer.
 *
 * NetClean VideoPlayer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * NetClean VideoPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NetClean VideoPlayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "font.h"

#define FNT_FONTHEIGHT 8
#define FNT_FONTWIDTH 8

struct FNT_xy
{
	int x;
	int y;
};

FNT_xy FNT_Generate(std::string str, Bitmap bitmap, Color color)
{
	unsigned int i, x, y, col, row, stop;
	unsigned char *fnt, chr;
	FNT_xy xy;

	fnt = FNT_GetFont();

	xy.x = xy.y = col = row = stop = 0;

	for(i = 0; i < str.size(); i++){
		switch(str[i]){
			case '\n':
				row++;
				col = 0;
				chr = 0;
				break;

			case '\r':
				chr = 0;
				break;

			case '\t':
				chr = 0;
				col += 4 - col % 4;
				break;
		
			case '\0':
				stop = 1;
				chr = 0;
				break;
	
			default:
				col++;
				chr = str[i];
				break;
		}

		if(stop){
			break;
		}

		if((col + 1) * FNT_FONTWIDTH > (unsigned int)xy.x){
			xy.x = col * FNT_FONTWIDTH;
		}
		
		if((row + 1) * FNT_FONTHEIGHT > (unsigned int)xy.y){
			xy.y = (row + 1) * FNT_FONTHEIGHT;
		}

		if(chr != 0 && bitmap.w != 0){
			for(y = 0; y < FNT_FONTHEIGHT; y++){
				for(x = 0; x < FNT_FONTWIDTH; x++){
					if(fnt[str[i] * FNT_FONTHEIGHT + y] >> (7 - x) & 1){
						//pixels[((col - 1) * FNT_FONTWIDTH) + x + (y + row * FNT_FONTHEIGHT) * w] = 1;
						bitmap.setPixel(((col - 1) * FNT_FONTWIDTH) + x, y + row * FNT_FONTHEIGHT, color);
					}
				}
			}
		}
	}

	return xy;	
}

Bitmap DrawText(std::string str, Color color)
{
	Bitmap bitmap; 
	bitmap.freeData = false;
	FNT_xy xy = FNT_Generate(str, bitmap, color);
	bitmap.allocate(xy.x, xy.y);
	bitmap.clear();
	FNT_Generate(str, bitmap, color);
	return bitmap;
}
