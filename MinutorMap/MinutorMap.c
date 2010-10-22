/*
Copyright (c) 2010, Sean Kasun
	Parts Copyright (c) 2010, Ryan Hitchman
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/


// MinutorMap.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "blockInfo.h"
#include <string.h>

static void draw(const char *world,int bx,int bz,int y,int opts,unsigned char *bits);
static void blit(unsigned char *block,unsigned char *bits,int px,int py,
	double zoom,int w,int h);
static Block *LoadBlock(char *filename);
static void b36(char *dest,int num);

//world = path to world saves
//cx = center x world
//cz = center z world
//y = start depth
//w = output width
//h = output height
//zoom = zoom amount (1.0 = 100%)
//bits = byte array for output
//opts = bitmask of render options
//  1<<0 Cave Mode
//  1<<1 Show Obscured
//  1<<2 Depth Shading
//  1<<3 Raytrace Shadows (TODO)
void DrawMap(const char *world,double cx,double cz,int y,int w,int h,double zoom,unsigned char *bits,int opts)
{
    /* We're converting between coordinate systems, so this gets kinda ugly 
     *
     * X     -world x N  -screen y
     * screen origin  |
     *                |
     *                |
     *                |
     *  +world z      |(cz,cx)   -world z
     * W--------------+----------------E
     *  -screen x     |          +screen x
     *                |
     *                | 
     *                |
     *      +world x  | +screen y
     *                S
     */

	unsigned char blockbits[16*16*4];
	int z,x,px,py;
	int blockScale=(int)(16*zoom);

	// number of blocks to fill the screen (plus 2 blocks for floating point inaccuracy)
	int hBlocks=(w+blockScale*2)/blockScale;
	int vBlocks=(h+blockScale*2)/blockScale;


	// cx/cz is the center, so find the upper left corner from that
	double startx=cx-(double)h/(2*zoom);
	double startz=cz+(double)w/(2*zoom);
	int startxblock=(int)(startx/16);
	int startzblock=(int)(startz/16);
	int shiftx=(int)(blockScale-(startz-startzblock*16)*zoom);
	int shifty=(int)((startx-startxblock*16)*zoom);

	if (shiftx<0)
	{
		startzblock++;
		shiftx+=blockScale;
	}
	if (shifty<0)
	{
		startxblock--;
		shifty+=blockScale;
	}

    // x increases south, decreases north
    for (x=0,py=-shifty;x<=vBlocks;x++,py+=blockScale)
    {
        // z increases west, decreases east
        for (z=0,px=-shiftx;z<=hBlocks;z++,px+=blockScale)
        {
			draw(world,startxblock+x,startzblock-z,y,opts,blockbits);
			blit(blockbits,bits,px,py,zoom,w,h);
		}
	}
}

//bx = x coord of pixel
//by = y coord of pixel
//cx = center x world
//cz = center z world
//w = output width
//h = output height
//zoom = zoom amount (1.0 = 100%)
//ox = world x at mouse
//oz = world z at mouse
const char *IDBlock(int bx, int by, double cx, double cz, int w, int h, double zoom,int *ox,int *oz)
{
	//WARNING: keep this code in sync with draw()
	Block *block;
    int x,y,z,px,py,xoff,zoff;
	int blockScale=(int)(16*zoom);
    
	// cx/cz is the center, so find the upper left corner from that
	double startx=cx-(double)h/(2*zoom);
	double startz=cz+(double)w/(2*zoom);
	int startxblock=(int)(startx/16);
	int startzblock=(int)(startz/16);
	int shiftx=(int)(blockScale-(startz-startzblock*16)*zoom);
	int shifty=(int)((startx-startxblock*16)*zoom);

	if (shiftx<0)
	{
		startzblock++;
		shiftx+=blockScale;
	}
	if (shifty<0)
	{
		startxblock--;
		shifty+=blockScale;
	}

    x=(by+shifty)/blockScale;
    py=x*blockScale-shifty;
    z=(bx+shiftx)/blockScale;
    px=z*blockScale-shiftx;

    zoff=((int)((px - bx)/zoom) + 15) % 16;
    xoff=(int)((by - py)/zoom);

	*ox=(startxblock+x)*16+xoff;
	*oz=(startzblock-z)*16+zoff;

    block=(Block *)Cache_Find(startxblock+x, startzblock-z);

    if (block==NULL)
        return "Unknown";

    y=block->heightmap[xoff+zoff*16];

    if (y == (unsigned char)-1) 
        return "Empty";  // nothing was rendered here

    return blocks[block->grid[y+(zoff+xoff*16)*128]].name;
}


//copy block to bits at px,py at zoom.  bits is wxh
static void blit(unsigned char *block,unsigned char *bits,int px,int py,
	double zoom,int w,int h)
{
	int x,y,yofs,bitofs;
	int skipx=0,skipy=0;
	int bw=(int)(16*zoom);
	int bh=(int)(16*zoom);
	if (px<0) skipx=-px;
	if (px+bw>=w) bw=w-px;
	if (bw<=0) return;
	if (py<0) skipy=-py;
	if (py+bh>=h) bh=h-py;
	if (bh<=0) return;
	bits+=py*w*4;
	bits+=px*4;
	for (y=0;y<bh;y++,bits+=w<<2)
	{
		if (y<skipy) continue;
		yofs=((int)(y/zoom))<<6;
		bitofs=0;
		for (x=0;x<bw;x++,bitofs+=4)
		{
			if (x<skipx) continue;
			memcpy(bits+bitofs,block+yofs+(((int)(x/zoom))<<2),4);
		}
	}
}

void CloseAll()
{
	Cache_Empty();
}

// opts is a bitmask representing render options:
// 1<<0 Cave Mode
// 1<<1 Show Obscured
// 1<<2 Depth Shading
// 1<<3 Raytrace Shadows (TODO)
static void draw(const char *world,int bx,int bz,int y,int opts,unsigned char *bits)
{
	int first,second;
	Block *block, *prevblock;
	char filename[256];
	int ofs=0,xOfs=0,prevy,zOfs,bofs;
	int x,z,i;
	unsigned int color, watercolor = blocks[BLOCK_WATER].color, water;
	unsigned char pixel, r, g, b, seenempty;

    char cavemode, showobscured, depthshading, raytrace;

    cavemode=!(!(opts&(1<<0)));
    showobscured=!(!(opts&(1<<1)));
    depthshading=!(!(opts&(1<<2)));
    raytrace=!(!(opts&(1<<3)));

	block=(Block *)Cache_Find(bx,bz);

	if (block==NULL)
	{
        strncpy_s(filename,256,world,256);
        strncat_s(filename,256,"/",256);
        first=bx%64;
        if (first<0) first+=64;
        b36(filename,first);
        strncat_s(filename,256,"/",256);
        second=bz%64;
        if (second<0) second+=64;
        b36(filename,second);
        strncat_s(filename,256,"/c.",256);
        b36(filename,bx);
        strncat_s(filename,256,".",256);
        b36(filename,bz);
        strncat_s(filename,256,".dat",256);

		block=LoadBlock(filename);
		if (block==NULL) //blank tile
		{
			memset(bits,0xff,16*16*4);
			return;
		}

		Cache_Add(bx,bz,block);
	}

    if (block->rendery==y && block->renderopts==opts) // already rendered, use cache
    { 
        memcpy(bits, block->rendercache, sizeof(unsigned char)*16*16*4);

        if (block->rendermissing) // wait, the last render was incomplete
        {
            if (Cache_Find(bx, bz+block->rendermissing) != NULL)
                ; // we can do a better render now that the missing block is loaded
            else
                return; // re-rendering wouldn't change anything
        } else
            return;
    }

    block->rendery=y;
    block->renderopts=opts;
    block->rendermissing=0;

    // find the block to the west, so we can use its heightmap for shading
    prevblock=(Block *)Cache_Find(bx, bz + 1);

    if (prevblock==NULL)
        block->rendermissing=1; //note no loaded block to west
    else if (prevblock->rendery!=y || prevblock->renderopts!=opts) {
        block->rendermissing=1; //note improperly rendered block to west
        prevblock = NULL; //block was rendered at a different y level, ignore
    }

    // x increases south, decreases north
	for (x=0;x<16;x++,xOfs+=128*16)
	{
        if (prevblock!=NULL)
            prevy = prevblock->heightmap[x];
        else
    		prevy=-1;

		zOfs=xOfs+128*15;

        // z increases west, decreases east
		for (z=15;z>=0;z--,zOfs-=128)
		{
			bofs=zOfs+y;
			color=0;
            water=0;
            seenempty=0;
			for (i=y;i>=0;i--,bofs--)
			{
				pixel=block->grid[bofs];
                if (pixel==BLOCK_AIR)
                {
                    seenempty=1;
                    continue;
                }
                if (pixel==BLOCK_STATIONARY_WATER || pixel==BLOCK_WATER) 
                {
                    seenempty=1; // count water as an "empty" (see-through) block
                    if (++water < 8)
                        continue;
                }
                if ((showobscured || seenempty) && pixel<numBlocks && blocks[pixel].canDraw)
				{
					if (prevy==-1) prevy=i;
					if (prevy<i)
						color=blocks[pixel].light;
					else if (prevy>i)
						color=blocks[pixel].dark;
					else
						color=blocks[pixel].color;

                    if (water != 0) {
                        r=(color>>16)/(water + 1) + (watercolor>>16)*water/(water + 1);
                        g=(color>>8&0xff)/(water + 1) + (watercolor>>8&0xff)*water/(water + 1);
                        b=(color&0xff)/(water + 1) + (watercolor&0xff)*water/(water + 1);
                        color = r<<16 | g<<8 | b;
                    }

					break;
				}
			}

			prevy=i;

            if (depthshading) // darken deeper blocks
            {
                int num=prevy+50-(128-y)/5;
                int denom=y+50-(128-y)/5;

                r=(color>>16)*num/denom;
                g=(color>>8&0xff)*num/denom;
                b=(color&0xff)*num/denom;
                color = r<<16 | g<<8 | b;
            }

            if (cavemode) {
                seenempty=0;
                pixel=block->grid[bofs];

                if (pixel==BLOCK_LEAVES || pixel==BLOCK_WOOD) //special case surface trees
                    for (; i>=1; i--,pixel=block->grid[--bofs])
                        if (!(pixel==BLOCK_WOOD||pixel==BLOCK_LEAVES||pixel==BLOCK_AIR))
                            break; // skip leaves, wood, air

                for (;i>=1;i--,bofs--)
                {
                    pixel=block->grid[bofs];
                    if (pixel==BLOCK_AIR)
                    {
                        seenempty=1;
                        continue;
                    }
                    if (seenempty && pixel<numBlocks && blocks[pixel].canDraw)
                    {
                        r=(color>>16)*(prevy-i+10)/138;
                        g=(color>>8&0xff)*(prevy-i+10)/138;
                        b=(color&0xff)*(prevy-i+10)/138; 
                        color = r<<16 | g<<8 | b;
                        break;
                    }
                }
            }

			bits[ofs++]=color>>16;
			bits[ofs++]=color>>8;
			bits[ofs++]=color;
			bits[ofs++]=0xff;

            block->heightmap[x+z*16] = prevy;
		}
	}

    memcpy(block->rendercache, bits, sizeof(unsigned char)*16*16*4);
}
Block *LoadBlock(char *filename)
{
	gzFile gz=newNBT(filename);
    Block *block=malloc(sizeof(Block));
    block->rendery = -1;
	if (nbtGetBlocks(gz, block->grid) == NULL) {
        free(block);
        block = NULL;
    }
	nbtClose(gz);
	return block;
}

void GetSpawn(const char *world,int *x,int *y,int *z)
{
	gzFile gz;
	char filename[256];
	strncpy_s(filename,256,world,256);
	strncat_s(filename,256,"/level.dat",256);
	gz=newNBT(filename);
	nbtGetSpawn(gz,x,y,z);
	nbtClose(gz);
}
void GetPlayer(const char *world,int *px,int *py,int *pz)
{
	gzFile gz;
	char filename[256];
	strncpy_s(filename,256,world,256);
	strncat_s(filename,256,"/level.dat",256);
	gz=newNBT(filename);
	nbtGetPlayer(gz,px,py,pz);
	nbtClose(gz);
}

static void b36(char *dest,int num)
{
	int lnum,i;
	int pos=strlen(dest);
	int len=0;
	for (lnum=num;lnum;len++)
		lnum/=36;
	if (num<0)
	{
		num=-num;
		dest[pos++]='-';
	}
	for (i=0;num;i++)
	{
		dest[pos+len-i-1]="0123456789abcdefghijklmnopqrstuvwxyz"[num%36];
		num/=36;
	}
	if (len==0)
		dest[pos++]='0';
	dest[pos+len]=0;
}
