#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "DEFS.H"

#ifdef GRAPH2

#include "GraphicsEventQueue.h"
#include "Planar2ChunkyDecoder.h"
#include "BitplaneDMA.h"
#include "PixelSerializer.h"
#include "BitplaneDraw.h"
#include "SpriteState.h"
#include "DIWXStateMachine.h"
#include "DIWYStateMachine.h"
#include "DDFStateMachine.h"

class Graphics
{
private:
  GraphicsEventQueue _queue;

  void InitializeEventQueue(void);
  void InitializeDIWXEvent(void);
  void InitializeDIWYEvent(void);
  void InitializeDDFEvent(void);
  void InitializeBitplaneDMAEvent(void);
  void InitializePixelSerializerEvent(void);

public:
  DIWXStateMachine DIWXStateMachine;
  DIWYStateMachine DIWYStateMachine;
  DDFStateMachine DDFStateMachine;
  BitplaneDMA BitplaneDMA;
  PixelSerializer PixelSerializer;
  Planar2ChunkyDecoder Planar2ChunkyDecoder;
  BitplaneDraw BitplaneDraw;
  Sprites Sprites;

  void Commit(ULO untilRasterY, ULO untilRasterX);

  void EndOfFrame(void);
  void SoftReset(void);
  void HardReset(void);
  void EmulationStart(void);
  void EmulationStop(void);
  void Startup(void);
  void Shutdown(void);
};

extern Graphics GraphicsContext;

#endif
#endif