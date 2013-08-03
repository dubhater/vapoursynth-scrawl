#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>

#include "ter-116n.h"


void scrawl_character(unsigned char c, uint8_t *image, int stride, int dest_x, int dest_y, int bitsPerSample) {
   int black = 16 << (bitsPerSample - 8);
   int white = 235 << (bitsPerSample - 8);
   int x, y;
   if (bitsPerSample == 8) {
      for (y = 0; y < character_height; y++) {
         for (x = 0; x < character_width; x++) {
            if (__font_bitmap__[c * character_height + y] & (1 << (7 - x))) {
               image[dest_y*stride + dest_x + x] = white;
            } else {
               image[dest_y*stride + dest_x + x] = black;
            }
         }

         dest_y++;
      }
   } else {
      for (y = 0; y < character_height; y++) {
         for (x = 0; x < character_width; x++) {
            if (__font_bitmap__[c * character_height + y] & (1 << (7 - x))) {
               ((uint16_t*)image)[dest_y*stride/2 + dest_x + x] = white;
            } else {
               ((uint16_t*)image)[dest_y*stride/2 + dest_x + x] = black;
            }
         }

         dest_y++;
      }
   }
}


static inline void vs_memset16(void *ptr, int value, size_t num) {
	uint16_t *tptr = (uint16_t *)ptr;
	while (num-- > 0)
		*tptr++ = (uint16_t)value;
}


void scrawl_text(const char *text, VSFrameRef *frame, const VSAPI *vsapi) {
   const VSFormat *frame_format = vsapi->getFrameFormat(frame);
   int width = vsapi->getFrameWidth(frame, 0);
   int height = vsapi->getFrameHeight(frame, 0);

   int start_x = 16;
   int start_y = 16;

   int characters_scrawled = 0;
   int plane, i;

   int txt_length = strlen(text);
   unsigned char *txt = malloc(txt_length);
   memcpy(txt, text, txt_length);

   for (i = 0; i < txt_length; i++) {
      int dest_x = start_x + characters_scrawled * character_width;
      int dest_y = start_y;

      if (txt[i] == '\r') {
         // Pretend \r doesn't exist.
         continue;
      }

      if (txt[i] == '\n') {
         // Must draw on a new line in the next iteration,
         start_y += character_height;
         characters_scrawled = 0;
         continue;
      }

      if (dest_x >= (width - character_width - 16)) {
         // Must draw on a new line in this iteration.
         dest_x = start_x;
         dest_y += character_height;
         start_y += character_height;
         characters_scrawled = 0;
      }

      if (dest_y >= (height - character_height)) {
         // There is no room for this line.
         return;
      }

      if (txt[i] < 32) {
         txt[i] = '_';
      }
      

      if (frame_format->colorFamily == cmRGB) {
         for (plane = 0; plane < frame_format->numPlanes; plane++) {
            uint8_t *image = vsapi->getWritePtr(frame, plane);
            int stride = vsapi->getStride(frame, plane);

            scrawl_character(txt[i], image, stride, dest_x, dest_y, frame_format->bitsPerSample);
         }
      } else {
         for (plane = 0; plane < frame_format->numPlanes; plane++) {
            uint8_t *image = vsapi->getWritePtr(frame, plane);
            int stride = vsapi->getStride(frame, plane);

            if (plane == 0) {
               scrawl_character(txt[i], image, stride, dest_x, dest_y, frame_format->bitsPerSample);
            } else {
               int sub_w = character_width  >> frame_format->subSamplingW;
               int sub_h = character_height >> frame_format->subSamplingH;
               int sub_dest_x = dest_x >> frame_format->subSamplingW;
               int sub_dest_y = dest_y >> frame_format->subSamplingH;
               int y;

               if (frame_format->bitsPerSample == 8) {
                  for (y = 0; y < sub_h; y++) {
                     memset(image + (y+sub_dest_y)*stride + sub_dest_x, 128, sub_w);
                  }
               } else {
                  for (y = 0; y < sub_h; y++) {
                     vs_memset16((uint16_t*)image + (y+sub_dest_y)*stride/2 + sub_dest_x, 128 << (frame_format->bitsPerSample - 8), sub_w);
                  }
               }
            }
         }
      }
      characters_scrawled++;
   }
   free(txt);
}


enum Filters {
   FILTER_TEXT,
   FILTER_CLIPINFO,
   FILTER_COREINFO,
   FILTER_FRAMENUM
};


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   char *text;
   enum Filters filter;
} ScrawlData;


static void VS_CC scrawlInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
   ScrawlData *d = (ScrawlData *) * instanceData;
   vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC scrawlGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
   ScrawlData *d = (ScrawlData *) * instanceData;

   if (activationReason == arInitial) {
      vsapi->requestFrameFilter(n, d->node, frameCtx);
   } else if (activationReason == arAllFramesReady) {
      const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
      VSFrameRef *dst = vsapi->copyFrame(src, core);

      if (d->filter == FILTER_FRAMENUM) {
         char *text = malloc(20);
         sprintf(text, "%d", n);
         scrawl_text(text, dst, vsapi);
         free(text);
      } else {
         scrawl_text(d->text, dst, vsapi);
      }

      vsapi->freeFrame(src);

      return dst;
   }

   return 0;
}


static void VS_CC scrawlFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData *d = (ScrawlData *)instanceData;

   if (d->filter == FILTER_CLIPINFO || d->filter == FILTER_COREINFO) {
      free(d->text);
   }

   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC textCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData d;
   ScrawlData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.filter = FILTER_TEXT;

   d.text = (char *)vsapi->propGetData(in, "text", 0, 0);

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "Text", scrawlInit, scrawlGetFrame, scrawlFree, fmParallel, 0, data, core);
   return;
}


static void VS_CC clipinfoCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData d;
   ScrawlData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.filter = FILTER_CLIPINFO;

   d.text = malloc(1024);
   if (isConstantFormat(d.vi)) {
      sprintf(d.text, "Clip info:\n"
                      "Width:  %d px\n"
                      "Height: %d px\n"
                      "Length: %d frames\n"
                      "FpsNum: %ld\n"
                      "FpsDen: %ld\n"
                      "Format: %s",
                      d.vi->width,
                      d.vi->height,
                      d.vi->numFrames,
                      d.vi->fpsNum,
                      d.vi->fpsDen,
                      d.vi->format->name);
   } else {
      sprintf(d.text, "Clip info:\n"
                      "Width:  may vary\n"
                      "Height: may vary\n"
                      "Length: %d frames\n"
                      "FpsNum: %ld\n"
                      "FpsDen: %ld\n"
                      "Format: may vary",
                      d.vi->numFrames,
                      d.vi->fpsNum,
                      d.vi->fpsDen);
   }

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "ClipInfo", scrawlInit, scrawlGetFrame, scrawlFree, fmParallel, 0, data, core);
   return;
}


static void VS_CC coreinfoCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData d;
   ScrawlData *data;

   const VSCoreInfo *ci = vsapi->getCoreInfo(core);

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.filter = FILTER_COREINFO;

   d.text = malloc(1024);
   sprintf(d.text, "%s\n"
                   "Threads: %d\n"
                   "Maximum framebuffer size: %ld bytes\n"
                   "Used framebuffer size: %ld bytes",
                   ci->versionString,
                   ci->numThreads,
                   ci->maxFramebufferSize,
                   ci->usedFramebufferSize);

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "CoreInfo", scrawlInit, scrawlGetFrame, scrawlFree, fmParallel, 0, data, core);
   return;
}


static void VS_CC framenumCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData d;
   ScrawlData *data;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.filter = FILTER_FRAMENUM;

   data = malloc(sizeof(d));
   *data = d;

   vsapi->createFilter(in, out, "FrameNum", scrawlInit, scrawlGetFrame, scrawlFree, fmParallel, 0, data, core);
   return;
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.scrawl", "scrawl", "Simple text output plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Text",
                "clip:clip;"
                "text:data;",
                textCreate, 0, plugin);
   registerFunc("ClipInfo",
                "clip:clip;",
                clipinfoCreate, 0, plugin);
   registerFunc("CoreInfo",
                "clip:clip;",
                coreinfoCreate, 0, plugin);
   registerFunc("FrameNum",
                "clip:clip;",
                framenumCreate, 0, plugin);
}
