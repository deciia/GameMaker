//----------------------------------------------------------------------
// viddrvc.cpp                    Graphics--standard C driver routines
// Copyright 1994 G. Andrew Stone
// Recreational Software Designs
// NOT PUBLIC DOMAIN!
//----------------------------------------------------------------------

#include <bios.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include "gen.h"
#include "pixel.hpp"
#include "Coord2d.hpp"
#include "VidDrive.hpp"

static void SetMode (VideoMode *v);
static void Clear   (Pixel Col,VideoMode *v);
static void Point   (Coord2d Pos, Pixel col,VideoMode *v);
static Pixel GetCol (Coord2d Pos,VideoMode *v);
static void Rep1Col    (Coord2d Pos,uint Num,Pixel col,VideoMode *v);
static void Rep1ColRev (Coord2d Pos,uint Num,Pixel col,VideoMode *v);
static void RepCols    (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v);
static void RepColsRev (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v);
static int  GetCols    (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v);
static int  GetColsRev (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v);
static void BoxFill (Coord2d Start,Coord2d End,Pixel col,VideoMode *v);
static void DrawBlock (Coord2d Pos,Block b,VideoMode *v);
//static void SetViewPos (unsigned long int pos,VideoMode *v);
//static void SetWritePage (unsigned long int pos,VideoMode *v);
static void Box(Coord2d Start,Coord2d End,Pixel col,VideoMode *v);
static void Line(Coord2d Start,Coord2d End,Pixel col,VideoMode *v);
static void BCurve(Coord2d p1,Coord2d p2,Coord2d VMid,unsigned char col,VideoMode *v);
 

VideoMode Defaults;

extern "C" void SetAllPal(RGBdata *pal,VideoMode *v);
extern "C" void far GetAllPal(RGBdata *pal,VideoMode *v);
extern "C" void SetAllPalTo(RGBdata *pal,VideoMode *v);
extern "C" void Palette(int num,RGBdata pal,VideoMode *v);


char *FontPtr;

boolean VideoDriver::Validate(int MinVersion)
  {
  if ((strcmp(ID,"GM")==0)&&(strcmp(Object,"Video Driver")==0))
    {
    if (Version<MinVersion) return(False);
    return(True);
    }
  else return(False);
  }


void VideoMode::SetStdRoutines(void)
  {
  VideoMode *Video   = this;
  Video->SetMode     = ::SetMode;
  Video->Clear       = ::Clear;
  Video->Point       = ::Point;
  Video->GetCol      = ::GetCol;
  Video->Rep1Col     = ::Rep1Col;
  Video->Rep1ColRev  = ::Rep1ColRev;
  Video->RepCols     = ::RepCols;
  Video->RepColsRev  = ::RepColsRev;
  Video->GetCols     = ::GetCols;
  Video->GetColsRev  = ::GetColsRev;
  Video->Line        = ::Line;
  Video->Box         = ::Box;
  Video->BoxFill     = ::BoxFill;
  Video->DrawBlock   = ::DrawBlock;
  Video->BCurve      = ::BCurve;
  Video->Spline      = ::BCurve;   // Place holder for now.
  Video->SetAllPal   = ::SetAllPal;
  Video->GetAllPal   = ::GetAllPal;
  Video->SetAllPalTo = ::SetAllPalTo;
  Video->SetPal      = ::Palette;
  }

VideoDriver* LoadVideoDriver(char *FileName)
  {
  int           filehandle=0;           // Declare a file handle
  unsigned int  driversegment=0;        // Declare variable for driver segment
  unsigned int  driveroffset=0;         // Declare variable for driver offset
  unsigned long driversize=0;           // Declare variable for file size
  VideoDriver  *Drvr;
  VideoMode    *Vid;
  char far     *pointertodrivermem;


  Defaults.SetStdRoutines();

  filehandle=open(FileName,O_RDONLY|O_BINARY); // Open the driver file
  if (filehandle==-1) return(False);           // On driver open error exit

  driversize=filelength(filehandle);    // Get the size of the file in bytes
  pointertodrivermem=(char far *) farmalloc(driversize+16L);

  if (pointertodrivermem==NULL)
    {
    close(filehandle);                  // Close the driver file
    return(False);                      // Can't allocate driver memory, exit
    }
  driversegment=FP_SEG(pointertodrivermem); // Get driver memory block segment
  driveroffset=FP_OFF(pointertodrivermem);  // Get driver memory block offset
  if (driveroffset>16)                      // Is the memory offset > 1 segment
    driversegment+=(driveroffset/16);       // Yes, calculate new segment size
  driversegment++;                          // Round up, account for remainder

  Drvr = (VideoDriver*) MK_FP(driversegment,0);

  if (read(filehandle, ((char*)Drvr), (uint) driversize)==-1)
    {                                   // If the load does not work
    close(filehandle);                  // Close the driver file
    farfree(pointertodrivermem);        // Free memory used by driver
    return(False);                      // Can't read driver file, exit
    }
  close(filehandle);                    // Close the driver file

  // Validate Here!
  if (!Drvr->Validate(100)) { farfree(pointertodrivermem); return(False); }


  // Routine Setup
  // This should check for NULL
  //    - if NULL, replace the routine w/ a standard which calls a lower
  //      level routine in the driver.
  //    - if not NULL, then do the MemAdj.
  #define MemAdj(x) FP_SEG(x) = FP_SEG(Vid)

  int cnt;
  for(cnt=0,Vid=&(Drvr->FirstDriver);cnt<=Drvr->NumModes;cnt++,Vid++)
    {
    if (Vid->SetMode    != NULL) MemAdj(Vid->SetMode);
    else Vid->SetMode=SetMode;
    if (Vid->Clear      != NULL) MemAdj(Vid->Clear);
    else Vid->Clear=Clear;
    if (Vid->Point      != NULL) MemAdj(Vid->Point);
    else Vid->Point=Point;
    if (Vid->GetCol     != NULL) MemAdj(Vid->GetCol);
    else Vid->GetCol=GetCol;
    if (Vid->Rep1Col    != NULL) MemAdj(Vid->Rep1Col);
    else Vid->Rep1Col=Rep1Col;
    if (Vid->Rep1ColRev != NULL) MemAdj(Vid->Rep1ColRev);
    else Vid->Rep1ColRev=Rep1ColRev;
    if (Vid->RepCols != NULL)   MemAdj(Vid->RepCols);
    else Vid->RepCols = RepCols;
    if (Vid->RepColsRev != NULL)   MemAdj(Vid->RepColsRev);
    else Vid->RepColsRev = RepColsRev;
    if (Vid->GetCols != NULL)   MemAdj(Vid->GetCols);
    else Vid->GetCols = GetCols;
    if (Vid->GetColsRev != NULL) MemAdj(Vid->GetColsRev);
    else Vid->GetColsRev = GetColsRev;
    if (Vid->Line != NULL) MemAdj(Vid->Line);
    else Vid->Line = Line;
    if (Vid->Box != NULL) MemAdj(Vid->Box);
    else Vid->Box = Box;
    if (Vid->BoxFill != NULL) MemAdj(Vid->BoxFill);
    else Vid->BoxFill = BoxFill;
    if (Vid->DrawBlock != NULL) MemAdj(Vid->DrawBlock);
    else Vid->DrawBlock = DrawBlock;
    if (Vid->BCurve != NULL) MemAdj(Vid->BCurve);
    else Vid->BCurve = BCurve;


    if (Vid->SetAllPal != NULL) MemAdj(Vid->SetAllPal);
    else Vid->SetAllPal = SetAllPal;

    if (Vid->GetAllPal != NULL) MemAdj(Vid->GetAllPal);
    else Vid->GetAllPal = GetAllPal;

    if (Vid->SetAllPalTo != NULL) MemAdj(Vid->SetAllPalTo);
    else Vid->SetAllPalTo = SetAllPalTo;

    if (Vid->SetPal != NULL) MemAdj(Vid->SetPal);
    else Vid->SetPal = Palette;


     // No breakdown Setviewpos and writepage is feasible, so skip them.
     // If the user does not specify, it will call to a retf instruction
     // due to the driver organization.
    MemAdj(Vid->SetViewPos);
    MemAdj(Vid->SetWritePage);
  }
  #undef MemAdj
  return(Drvr);                         // Return with driver loaded flag
  }


/*-----------------------BASIC GRAPHICS FUNCTIONS--------------------*/

void BCurve(Coord2d p1,Coord2d p2,Coord2d VMid,unsigned char col,VideoMode *V)
  {
  int l;
  Coord2d VFrom,VTo,VCur,VBres,VDelta;
  Coord2d Cur,Bres,Old,Delta;
  int Steps,MaxSteps;
  int tf,VMul;

  V->Line(p1,VMid,22,V);
  V->Line(p2,VMid,22,V);

  VFrom = VMid - p1;              // Create the start direction vector.
  VTo   = p2 - VMid;              // Create the end direction vector.

  Delta = p2-p1;

  // Normalize the vectors.
  tf=((Abs(VFrom.x)+Abs(VTo.x)))/2;
  tf=Delta.x/tf;
  tf=Abs(tf);
  VMul    = tf*VTo.x;
  VTo.x   = VMul;
  VMul    = VFrom.x*tf;
  VFrom.x = VMul;

  tf=((Abs(VFrom.y)+Abs(VTo.y)))/2;
  tf=Delta.y/tf;
  tf=Abs(tf);
  VMul    = tf*VTo.y;
  VTo.y   = VMul;
  VMul    = VFrom.y*tf;
  VFrom.y = VMul;

  // Set up Bresenham's to convert 1 vector to the other.
  VDelta = VTo-VFrom;
  VCur   = VFrom;
  Cur    = p1;
  VBres.Set(0,0);
  Bres.Set(0,0);

  MaxSteps= (Abs(Delta.x)+Abs(Delta.y))/30;

  for (Steps=0;Steps<MaxSteps;Steps++)
    {
    //bioskey(0);
    VBres += AbsC(VDelta);
    while (VBres.x>=MaxSteps) { VBres.x -= MaxSteps; VCur.x+=Sign(VDelta.x); }
    while (VBres.y>=MaxSteps) { VBres.y -= MaxSteps; VCur.y+=Sign(VDelta.y); }

    Old=Cur;

    Bres += AbsC(VCur);
    while (Bres.x>=MaxSteps) { Bres.x -= MaxSteps; Cur.x+=Sign(Delta.x)*Sign(VCur.x); }
    while (Bres.y>=MaxSteps) { Bres.y -= MaxSteps; Cur.y+=Sign(Delta.y)*Sign(VCur.y); }

    V->Line(Old,Cur,col,V);
    //V->Line(Old,Old.x+VCur.x/10,Old.y+VCur.y/10,70,V);
    }
  }


static void SetMode (VideoMode *v)
  {
  union REGS r;
  r.x.ax = v->BIOSMode;
  int86(0x10,&r,&r);
  }

static void Clear   (Pixel Col,VideoMode *v)
  {
  Coord2d zero(0,0);
  v->BoxFill(zero,v->Size,Col,v);
  }

// ROUTINES NEED BOUNDS CHECKING!
// Interdependancy checking required!

static void Point   (Coord2d Pos, Pixel col,VideoMode *v)
  {
  v->Rep1Col(Pos,1,col,v);
  }

static Pixel GetCol (Coord2d Pos,VideoMode *v)
  {
  Pixel Data;
  v->GetCols (Pos,1,&Data,v);
  return(Data);
  }

static void Rep1Col (Coord2d Pos,uint Num,Pixel col,VideoMode *v)
  {
  Num+=Pos.x;
  if (Num>v->Size.x) Num=v->Size.x;
  for (;Pos.x<Num;Pos.x++) v->Point(Pos,col,v);
  }

static void Rep1ColRev (Coord2d Pos,uint Num,Pixel col,VideoMode *v)
  {
  Pos.x-=Num;
  v->Rep1Col(Pos,Num,col,v);
  }

static void RepCols (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v)
  {
  Num+=Pos.x;
  if (Num>v->Size.x) Num=v->Size.x;
  for (;Pos.x<Num;Pos.x++,cols++) v->Point(Pos,*cols,v);
  }

static void RepColsRev (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v)
  {
  }

static int  GetCols (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v)
  {
  Num+=Pos.x;
  if (Num>v->Size.x) Num=v->Size.x;
  for (;Pos.x<Num;Pos.x++,cols++) (*cols) = v->GetCol(Pos,v);
  }

static int  GetColsRev (Coord2d Pos,uint Num,Pixel *cols,VideoMode *v)
  {}

static void BoxFill (Coord2d Start,Coord2d End,Pixel col,VideoMode *v)
  {
  int Len=(End.x-Start.x)+1;
  for (;Start.y<=End.y;Start.y++) v->Rep1Col(Start,Len,col,v);
  }

static void DrawBlock (Coord2d Pos,Block b,VideoMode *v)
  {}

static void Box(Coord2d ur,Coord2d ll,Pixel col,VideoMode *v)
  {
  if (ur.x==ll.x) ll.x++;
  if (ur.y==ll.y) ll.y++;

  Coord2d ul = Min(ur,ll);
  Coord2d lr = Max(ur,ll);

  ur = MakeCoord2d(lr.x,ul.y);
  ll = MakeCoord2d(ul.x,lr.y);

  v->Line(ul,ur-MakeCoord2d(1,0),col,v);
  v->Line(ur,lr-MakeCoord2d(0,1),col,v);
  v->Line(lr,ll+MakeCoord2d(1,0),col,v);
  v->Line(ll,ul+MakeCoord2d(0,1),col,v);
  }

static void Line(Coord2d Start,Coord2d End,Pixel col,VideoMode *v)
  {
  register int l;
  Coord2d Delta,Sign;
  int absdx=0, absdy=0;
  Coord2d Cnt(0,0);

  Delta.x = End.x-Start.x;
  Delta.y = End.y-Start.y;

  Sign.x=Sign(Delta.x);
  Sign.y=Sign(Delta.y);
  absdx=Sign.x*Delta.x;
  absdy=Sign.y*Delta.y;
  v->Point(Start,col,v);

  if (absdx>=absdy)
    {
    for (l=0; l<absdx; l++)
      {
      Cnt.y += absdy;
      if (Cnt.y>=absdx)
        {
        Cnt.y-=absdx;
        Start.y += Sign.y;
        }
      Start.x += Sign.x;
      v->Point(Start,col,v);
      }
    }
  else
    {
    for (l=0; l<absdy; l++)
      {
      Cnt.x += absdx;
      if (Cnt.x>=absdy)
        {
        Cnt.x -= absdy;
        Start.x += Sign.x;
        }
      Start.y += Sign.y;
      v->Point (Start,col,v);
      }
    }
  }

/*
static void SetViewPos (unsigned long int pos,VideoMode *v)
  {}
static void SetWritePage (unsigned long int pos,VideoMode *v)
  {}
*/


/*----------------------------------------------------------------------*/


