#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <list>
#include <string>

#include <VapourSynth.h>
#include <VSHelper.h>

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


void sanitise_text(std::string& txt) {
   for (int i = 0; txt[i]; i++) {
      if (txt[i] == '\r') {
        if (txt[i+1] == '\n') {
           txt.erase(i, 1);
        } else {
           txt[i] = '\n';
        }
        continue;
      }

      if (txt[i] == '\n') {
         continue;
      }

      // Must adjust the character code because of the five characters
      // missing from the font.
      if ((unsigned char)txt[i] < 32 ||
          (unsigned char)txt[i] == 129 ||
          (unsigned char)txt[i] == 141 ||
          (unsigned char)txt[i] == 143 ||
          (unsigned char)txt[i] == 144 ||
          (unsigned char)txt[i] == 157) {
         txt[i] = '_';
         continue;
      }

      // Ugly! Ugly! UGLY!
      if ((unsigned char)txt[i] > 157) {
         txt[i] = (char)((unsigned char)txt[i] - 5);
      } else if ((unsigned char)txt[i] > 144) {
         txt[i] = (char)((unsigned char)txt[i] - 4);
      } else if ((unsigned char)txt[i] > 141) {
         txt[i] = (char)((unsigned char)txt[i] - 2);
      } else if ((unsigned char)txt[i] > 129) {
         txt[i] = (char)((unsigned char)txt[i] - 1);
      }
   }
}


std::list<std::string> split_text(const std::string& txt, int width, int height) {
   std::list<std::string> lines;

   // First split by \n
   int prev_pos = -1;
   for (int i = 0; i < (int)txt.size(); i++) {
      if (txt[i] == '\n') {
         //if (i > 0 && i - prev_pos > 1) { // No empty lines allowed
            lines.push_back(txt.substr(prev_pos + 1, i - prev_pos - 1));
         //}
         prev_pos = i;
      }
   }
   lines.push_back(txt.substr(prev_pos + 1));

   // Then split any lines that don't fit
   int horizontal_capacity = width / character_width;
   std::list<std::string>::iterator iter;
   for (iter = lines.begin(); iter != lines.end(); iter++) {
      if ((int)(*iter).size() > horizontal_capacity) {
         lines.insert(std::next(iter), (*iter).substr(horizontal_capacity));
         (*iter).erase(horizontal_capacity);
      }
   }

   // Also drop lines that would go over the frame's bottom edge
   int vertical_capacity = height / character_height;
   if ((int)lines.size() > vertical_capacity) {
      lines.resize(vertical_capacity);
   }

   return lines;
}


void scrawl_text(std::string txt, int alignment, VSFrameRef *frame, const VSAPI *vsapi) {
   const VSFormat *frame_format = vsapi->getFrameFormat(frame);
   int width = vsapi->getFrameWidth(frame, 0);
   int height = vsapi->getFrameHeight(frame, 0);

   const int margin_h = 16;
   const int margin_v = 16;

   sanitise_text(txt);

   std::list<std::string> lines = split_text(txt, width - margin_h*2, height - margin_v*2);

   int start_x = 0;
   int start_y = 0;

   switch (alignment) {
      case 7:
      case 8:
      case 9:
         start_y = margin_v;
         break;
      case 4:
      case 5:
      case 6:
         start_y = (height - lines.size()*character_height) / 2;
         break;
      case 1:
      case 2:
      case 3:
         start_y = height - lines.size()*character_height - margin_v;
         break;
   }

   std::list<std::string>::const_iterator iter;
   for (iter = lines.begin(); iter != lines.end(); iter++) {
      switch (alignment) {
         case 1:
         case 4:
         case 7:
            start_x = margin_h;
            break;
         case 2:
         case 5:
         case 8:
            start_x = (width - (*iter).size()*character_width) / 2;
            break;
         case 3:
         case 6:
         case 9:
            start_x = width - (*iter).size()*character_width - margin_h;
            break;
      }

      for (unsigned int i = 0; i < (*iter).size(); i++) {
         int dest_x = start_x + i*character_width;
         int dest_y = start_y;

         if (frame_format->colorFamily == cmRGB) {
            for (int plane = 0; plane < frame_format->numPlanes; plane++) {
               uint8_t *image = vsapi->getWritePtr(frame, plane);
               int stride = vsapi->getStride(frame, plane);

               scrawl_character((*iter)[i], image, stride, dest_x, dest_y, frame_format->bitsPerSample);
            }
         } else {
            for (int plane = 0; plane < frame_format->numPlanes; plane++) {
               uint8_t *image = vsapi->getWritePtr(frame, plane);
               int stride = vsapi->getStride(frame, plane);

               if (plane == 0) {
                  scrawl_character((*iter)[i], image, stride, dest_x, dest_y, frame_format->bitsPerSample);
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
               } // if plane
            } // for plane in planes
         } // if colorFamily
      } // for i in line
      start_y += character_height;
   } // for iter in lines
}


enum Filters {
   FILTER_TEXT,
   FILTER_CLIPINFO,
   FILTER_COREINFO,
   FILTER_FRAMENUM,
   FILTER_FRAMEPROPS
};


typedef struct {
   VSNodeRef *node;
   const VSVideoInfo *vi;

   std::string text;
   int alignment;
   intptr_t filter;
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
         scrawl_text(std::to_string(n), d->alignment, dst, vsapi);
      } else if (d->filter == FILTER_FRAMEPROPS) {
         const VSMap *props = vsapi->getFramePropsRO(src);
         int numKeys = vsapi->propNumKeys(props);
         int i;
         std::string text = "Frame properties:\n";

         for (i = 0; i < numKeys; i++) {
            const char *key = vsapi->propGetKey(props, i);
            char type = vsapi->propGetType(props, key);
            int numElements = vsapi->propNumElements(props, key);
            int idx;
            // "<key>: <val0> <val1> <val2> ... <valn-1>"
            text.append(key).append(": ");
            if (type == ptInt) {
               for (idx = 0; idx < numElements; idx++) {
                  int64_t value = vsapi->propGetInt(props, key, idx, NULL);
                  text.append(std::to_string(value));
                  if (idx < numElements-1) {
                     text.append(" ");
                  }
               }
            } else if (type == ptFloat) {
               for (idx = 0; idx < numElements; idx++) {
                  double value = vsapi->propGetFloat(props, key, idx, NULL);
                  text.append(std::to_string(value));
                  if (idx < numElements-1) {
                     text.append(" ");
                  }
               }
            } else if (type == ptData) {
               for (idx = 0; idx < numElements; idx++) {
                  const char *value = vsapi->propGetData(props, key, idx, NULL);
                  int size = vsapi->propGetDataSize(props, key, idx, NULL);
                  if (size > 100) {
                     text.append("<property too long>");
                  } else {
                     text.append(value);
                  }
                  if (idx < numElements-1) {
                     text.append(" ");
                  }
               }
            }

            text.append("\n");
         }

         scrawl_text(text, d->alignment, dst, vsapi);
      } else {
         scrawl_text(d->text, d->alignment, dst, vsapi);
      }

      vsapi->freeFrame(src);

      return dst;
   }

   return 0;
}


static void VS_CC scrawlFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData *d = (ScrawlData *)instanceData;

   vsapi->freeNode(d->node);
   free(d);
}


static void VS_CC scrawlCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
   ScrawlData d;
   ScrawlData *data;
   int err;

   d.node = vsapi->propGetNode(in, "clip", 0, 0);
   d.vi = vsapi->getVideoInfo(d.node);

   d.alignment = vsapi->propGetInt(in, "alignment", 0, &err);
   if (err) {
      d.alignment = 7; // top left
   }

   if (d.alignment < 1 || d.alignment > 9) {
      vsapi->setError(out, "Scrawl: alignment must be between 1 and 9 (think numpad)");
      vsapi->freeNode(d.node);
      return;
   }

   d.filter = (intptr_t)userData;

   const char *instanceName = "shut up gcc";

   switch (d.filter) {
      case FILTER_TEXT:
         d.text = vsapi->propGetData(in, "text", 0, 0);

         instanceName = "Text";
         break;
      case FILTER_CLIPINFO:
         if (isConstantFormat(d.vi)) {
            d.text.append("Clip info:\n");
            d.text.append("Width: ").append(std::to_string(d.vi->width)).append(" px\n");
            d.text.append("Height: ").append(std::to_string(d.vi->height)).append(" px\n");
            d.text.append("Length: ").append(std::to_string(d.vi->numFrames)).append(" px\n");
            d.text.append("FpsNum: ").append(std::to_string(d.vi->fpsNum)).append("\n");
            d.text.append("FpsDen: ").append(std::to_string(d.vi->fpsDen)).append("\n");
            d.text.append("Format: ").append(d.vi->format->name);
         } else {
            d.text.append("Clip info:\n");
            d.text.append("Width: may vary\n");
            d.text.append("Height: may vary\n");
            d.text.append("Length: ").append(std::to_string(d.vi->numFrames)).append(" px\n");
            d.text.append("FpsNum: ").append(std::to_string(d.vi->fpsNum)).append("\n");
            d.text.append("FpsDen: ").append(std::to_string(d.vi->fpsDen)).append("\n");
            d.text.append("Format: may vary");
         }

         instanceName = "ClipInfo";
         break;
      case FILTER_COREINFO:
      {
         const VSCoreInfo *ci = vsapi->getCoreInfo(core);

         d.text.append(ci->versionString).append("\n");
         d.text.append("Threads: ").append(std::to_string(ci->numThreads)).append("\n");
         d.text.append("Maximum framebuffer cache size: ").append(std::to_string(ci->maxFramebufferSize)).append(" bytes\n");
         d.text.append("Used framebuffer cache size: ").append(std::to_string(ci->usedFramebufferSize)).append(" bytes");

         instanceName = "CoreInfo";
         break;
      }
      case FILTER_FRAMENUM:
         instanceName = "FrameNum";
         break;
      case FILTER_FRAMEPROPS:
         instanceName = "FrameProps";
         break;
   }

   data = new ScrawlData();
   *data = d;

   vsapi->createFilter(in, out, instanceName, scrawlInit, scrawlGetFrame, scrawlFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
   configFunc("com.nodame.scrawl", "scrawl", "Simple text output plugin for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
   registerFunc("Text",
                "clip:clip;"
                "text:data;"
                "alignment:int:opt;",
                scrawlCreate, (void *)FILTER_TEXT, plugin);
   registerFunc("ClipInfo",
                "clip:clip;"
                "alignment:int:opt;",
                scrawlCreate, (void *)FILTER_CLIPINFO, plugin);
   registerFunc("CoreInfo",
                "clip:clip;"
                "alignment:int:opt;",
                scrawlCreate, (void *)FILTER_COREINFO, plugin);
   registerFunc("FrameNum",
                "clip:clip;"
                "alignment:int:opt;",
                scrawlCreate, (void *)FILTER_FRAMENUM, plugin);
   registerFunc("FrameProps",
                "clip:clip;"
                "alignment:int:opt;",
                scrawlCreate, (void *)FILTER_FRAMEPROPS, plugin);
}
