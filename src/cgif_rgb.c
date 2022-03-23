#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cgif.h"

#define MULU16(a, b) (((uint32_t)a) * ((uint32_t)b)) // helper macro to correctly multiply two U16's without default signed int promotion

// to be done:
// cleanup/refactor this module.

struct st_cgif_rgb {
  CGIF*          pGIF;
  CGIFrgb_Config config;
  uint8_t*       pBefImageData;
  cgif_chan_fmt  befFmtChan;
};

typedef struct {
  uint8_t r,g,b,a;
} Pixel;

typedef struct treeNode {
  struct treeNode* child0; // pointer to child-node (elements smaller or equal than mean)
  struct treeNode* child1; // pointer to child-node (elements larger than mean)
  float mean[3];           // average of the colors
  uint32_t idxMin, idxMax; // minimum and maximum index referring to global palette
  uint8_t cutDim;          // dimension along which the cut (node split) is done
  uint8_t colIdx;          // color index (only meaningful for leave node)
  uint8_t height;          // node height
  bool isLeave;            // is leave node or not
} treeNode;

static uint64_t argmax64(float* arry, uint64_t n){
  uint64_t imax = 0;
  float vmax = 0;

  for(uint64_t i = 0; i < n; ++i) {
    if(arry[i] > vmax){
      imax = i;
      vmax = arry[i];
    }
  }
  return imax;
}

/* get the next prime number above the next power of two, input must be within [512, 2*256^3]*/
static int getNextPrimePower2(int N){
  if(N <= 512) {return 521;}
  else if(N <= 1024) {return 1031;}
  else if (N <= 2048) {return 2053;}
  else if (N <= 4096) {return 4099;}
  else if (N <= 8192) {return 8209;}
  else if (N <= 16384) {return 16411;}
  else if (N <= 32768) {return 32771;}
  else if (N <= 65536) {return 65537;}
  else if (N <= 131072) {return 131101;}
  else if (N <= 262144) {return 262147;}
  else if (N <= 524288) {return 524309;}
  else if (N <= 1048576) {return 1048583;}
  else if (N <= 2097152) {return 2097169;}
  else if (N <= 4194304) {return 4194319;}
  else if (N <= 8388608) {return 8388617;}
  else {return 256*256*256;} // no prime number, but the largest table size which is meaningful
}

static float min(float a, float b){
  return (a <= b) ? a : b;
}

static float max(float a, float b){
  return (a >= b) ? a : b;
}

static Pixel getPixelVal(const uint8_t* pData, cgif_chan_fmt fmtChan) {
  Pixel result;

  result.r = *(pData++);
  result.g = *(pData++);
  result.b = *(pData++);
  // check whether there is a alpha channel, if so convert it to a 1 bit alpha channel.
  switch(fmtChan) {
  case CGIF_CHAN_FMT_RGB:
    result.a = 0xFF;
    break;
  case CGIF_CHAN_FMT_RGBA:
    result.a = (*(pData) <= 127) ? 0 : 0xFF;
    break;
  }
  return result;
}

/* get a hash from rgb color values (use open addressing, if entry does not exist, next free spot is returned) */
static int32_t col_hash(const uint8_t* rgb, const uint8_t* hashTable, const uint8_t* indexUsed, const uint32_t tableSize, cgif_chan_fmt fmtChan, uint32_t* cntCollision) {
  uint32_t h;
  const Pixel pix = getPixelVal(rgb, fmtChan);
  *cntCollision = 0; // count the number of collisions
  // has alpha channel?
  if(pix.a == 0) {
    return -1;
  }
  h = (pix.r * 256 * 256 + pix.g * 256 + pix.b) % tableSize;
  while(1) {
    if(indexUsed[h] == 0 || memcmp(rgb, &hashTable[3 * h], 3) == 0) {
      return h;
    } else {
      ++(*cntCollision);
      ++h; // go on searching for a free spot
      h = h % tableSize; // start from beginning if there is no free stop at the end
    }
  }
  return h;
}

/* take array indexed by hash(rgb) and turn it into a dense array */
static uint32_t* hash_to_dense(const uint8_t* pPalette, const uint32_t* arry, uint32_t cnt, const uint8_t* hashTable, const uint8_t* indexUsed, uint32_t tableSize, cgif_chan_fmt fmtChan) {
  // pPalette: color table with cnt entries
  // arry: array indexed by hash of RGB-value in pPalette
  uint32_t* arryDense = malloc(sizeof(uint32_t) * cnt);
  uint32_t h, cntCollision;

  (void)fmtChan;
  for(uint32_t i = 0; i < cnt; ++i) {
    h = col_hash(&pPalette[3 * i], hashTable, indexUsed, tableSize, 3, &cntCollision);
    arryDense[i] = arry[h];
  }
  return arryDense;
}

/* get mean of color-cloud along all 3 dimensions (at least one color must be present, otherwise div 0 issue)*/
static void get_mean(const uint8_t* pPalette, const uint32_t* frequ, uint32_t idxMin, uint32_t idxMax, float* mean){
  // pPalette: RGB color palette
  // frequ: frequency of the colors (indexing as pPalette, no hashing)
  // idxMin, idxMax: palette range for which the mean is computed
  // mean: mean value along all three dimensions
  float m[3]   = {0,0,0};
  float sum[3] = {0,0,0};
  uint32_t i;
  uint8_t dim;

  for(i = idxMin; i <= idxMax; ++i) {
    for(dim = 0; dim < 3; ++dim) {
      sum[dim] += frequ[i];
      m[dim]   += frequ[i] * pPalette[3 * i + dim];
    }
  }
  for(dim = 0; dim < 3; ++dim) {
    mean[dim] = m[dim] / sum[dim];
  }
}

/* get variance of color-cloud along all 3 dimensions*/
static void get_variance(const uint8_t* pPalette, const uint32_t* frequ, uint32_t idxMin, uint32_t idxMax, float* var, float* mean){
  // pPalette: RGB color palette
  // frequ: frequency of the colors (indexing as pPalette, no hashing)
  // idxMin, idxMax: palette range for which the variance is computed
  // var/mean: variance/mean value along all three dimensions (array with 3 entries)
  uint32_t i;
  uint8_t dim;
  float v[3] = {0,0,0};
  float sum[3] = {0,0,0};

  get_mean(pPalette, frequ, idxMin, idxMax, mean);
  for(i = idxMin; i <= idxMax; ++i) {
    for(dim = 0; dim < 3; ++dim) {
      sum[dim] += frequ[i];
      v[dim]   += frequ[i] * (pPalette[3 * i + dim] - mean[dim]) * (pPalette[3 * i + dim] - mean[dim]);
    }
  }
  for(dim = 0; dim < 3; ++dim) {
    var[dim] = v[dim] / sum[dim];
  }
}

static treeNode* new_tree_node(uint8_t* pPalette, uint32_t* frequ, uint16_t* numLeaveNodes, uint32_t idxMin, uint32_t idxMax, uint8_t height, uint8_t colIdx) {
  float var[3];

  treeNode* node = malloc(sizeof(treeNode));
  node->idxMin   = idxMin; // minimum color in pPalette belonging to the node
  node->idxMax   = idxMax; // maximum color in pPalette belonging to the node
  get_variance(pPalette, frequ, idxMin, idxMax, var, node->mean);
  node->cutDim  = argmax64(var, 3); // dimension along which the cut (node split) is done
  node->height  = height; // node height
  node->colIdx  = colIdx; // index referring to (averaged) color in new color table
  node->isLeave = 1;      // every new node starts as a leave node
  (*numLeaveNodes)++;     // increase counter when leave node is added
  return node;
}

/* create the decision tree. (Similar to qsort with limited depth: pPalette, frequ get sorted) */
static void crawl_decision_tree(treeNode* parent, uint16_t* numLeaveNodes, uint8_t* pPalette, uint32_t* frequ, uint16_t colMax, uint8_t depthMax) {
  uint32_t i, k, saveNum;
  uint8_t saveBlk[3];

  if(*numLeaveNodes <= (colMax - 1) && (parent->idxMax - parent->idxMin) >= 1 && parent->height < depthMax) { // conditions for a node split
    i = parent->idxMin; // start of block minimum
    k = parent->idxMax; // start at block maximum
    while(i < k) { // split parent node in two blocks (like one step in qsort)
      for(; pPalette[3 * i + parent->cutDim] <= parent->mean[parent->cutDim]; ++i); // && i<parent->idxMax not needed (other condition is false when i==parent>idxMax since there must be at most 1 element above mean)
      for(; pPalette[3 * k + parent->cutDim] > parent->mean[parent->cutDim];  --k); // && k>parent->idxMin not needed (other condition is false when k==parent>idxMin since there must be at most 1 element below mean)
      if(k > i) {
        memcpy(saveBlk, &(pPalette[3 * i]), 3);
        memcpy(&(pPalette[3 * i]), &(pPalette[3 * k]), 3); // swap RGB-blocks in pPalette
        memcpy(&(pPalette[3 * k]), saveBlk, 3);            // swap RGB-blocks in pPalette
        saveNum  = frequ[k];
        frequ[k] = frequ[i]; // swap also the frequency
        frequ[i] = saveNum;  // sawp also the frequency
      }
    }
    parent->isLeave = 0; // parent is no leave node anymore when children added
    (*numLeaveNodes)--;  // decrease counter since parent is removed as a leave node
    parent->child0 = new_tree_node(pPalette, frequ, numLeaveNodes, parent->idxMin, i - 1, parent->height + 1, parent->colIdx); // i-1 is last index of 1st block, one child takes color index from parent
    parent->child1 = new_tree_node(pPalette, frequ, numLeaveNodes, i, parent->idxMax, parent->height + 1, *numLeaveNodes);
    crawl_decision_tree(parent->child0, numLeaveNodes, pPalette, frequ, colMax, depthMax);
    crawl_decision_tree(parent->child1, numLeaveNodes, pPalette, frequ, colMax, depthMax);
  }
}

/* fill 256 color table using the decision tree */
static void get_palette_from_decision_tree(const treeNode* root, uint8_t* pPalette256){
  if(root->isLeave == 0) {
    get_palette_from_decision_tree(root->child0, pPalette256);
    get_palette_from_decision_tree(root->child1, pPalette256);
  } else {
    pPalette256[3 * root->colIdx]     = (uint8_t)roundf(root->mean[0]);
    pPalette256[3 * root->colIdx + 1] = (uint8_t)roundf(root->mean[1]);
    pPalette256[3 * root->colIdx + 2] = (uint8_t)roundf(root->mean[2]);
  }
}

/* get color index by using the decision tree */
static uint8_t get_leave_node_index(const treeNode* root, const float* rgb) {
  if(root->isLeave == 0) { // if there is a split
    if(rgb[root->cutDim] <= root->mean[root->cutDim]) {
      return get_leave_node_index(root->child0, rgb);
    } else {
      return get_leave_node_index(root->child1, rgb);
    }
  } else {
    return root->colIdx; // return color index of leave node
  }
}

/* color quantization with mean cut method (TBD? switch to median cut)
   (works with dense palette, no hash table) */
static treeNode* create_decision_tree(uint8_t* pPalette,  uint32_t* pFrequDense, uint8_t* pPalette256, uint32_t cnt, uint16_t colMax, uint8_t depthMax){
  uint16_t numLeaveNodes = 0;
  treeNode* root = new_tree_node(pPalette, pFrequDense, &numLeaveNodes, 0, cnt - 1, 0, 0);
  crawl_decision_tree(root, &numLeaveNodes, pPalette, pFrequDense, colMax, depthMax);
  get_palette_from_decision_tree(root, pPalette256); // fill the reduced color table
  return root;
}

/* free memory allocated for the tree */
static void free_decision_tree(treeNode* root){
  if(root->isLeave == 0) { // if the node has children
    free_decision_tree(root->child0);
    free_decision_tree(root->child1);
  }
  free(root); // free root once free is called for children
}

/* get image with max 256 color indices using Floyd-Steinberg dithering */
static void get_quantized_dithered_image(uint8_t* pImageData, float* pImageDataRGBfloat, uint8_t* pPalette256, treeNode* root, uint32_t numPixel, uint32_t width, bool dithering, uint8_t transIndex, cgif_chan_fmt fmtChan, uint8_t* pBef, cgif_chan_fmt befFmtChan, int hasAlpha) {
  // pImageData: image with (max 256) color indices (length: numPixel)
  // pImageDataRGBfloat: image with RGB colors (length: 3*numPixel) must be signed to avoid overflow due to error passing, float only needed because of 0.9 factor
  // pPalette256: quantized color palette (indexed by node->colIdx), only used if dithering is on
  // root: root node of the decision tree for color quantization
  // numPixel, width: size of the image
  // dithering: use dithering or not
  uint32_t i;
  uint8_t k;
  int err; // color errors
  /*
    we noticed certain artifacts with Floyd-Steinberg dithering if the error is passed too far.
    use a damping factor to reduce these effects.
  */
  const float factor = 0.90;
  if(!dithering) {
    for(i = 0; i < numPixel; ++i) {
      if(hasAlpha) {
        if(pImageDataRGBfloat[fmtChan * i + 3] == 0) {
          pImageData[i] = transIndex;
          continue;
        }
      } else {
        // do the transparency trick
        float* p = pImageDataRGBfloat;
        if(pBef && p[i * fmtChan] == pBef[i * befFmtChan] && p[i * fmtChan + 1] == pBef[i * befFmtChan + 1] && p[i * fmtChan + 2] == pBef[i * befFmtChan + 2]) {
          pImageData[i] = transIndex;
          continue;
        }
      }
      pImageData[i] = get_leave_node_index(root, &pImageDataRGBfloat[fmtChan * i]);  // use decision tree to get indices for new colors
    }
  } else {
    for(i = 0; i < numPixel; ++i) {
      if(fmtChan == CGIF_CHAN_FMT_RGBA) {
        if(pImageDataRGBfloat[fmtChan * i + 3] == 0) {
          pImageData[i] = transIndex;
          continue;
        }
      }
      // TBD add the transparency trick

      // restrict color + error to 0-255 interval
      for(k = 0; k<3; ++k) {
        pImageDataRGBfloat[fmtChan * i + k] = max(0,min(pImageDataRGBfloat[fmtChan * i + k], 255)); // cut to 0-255 before
      }

      pImageData[i] = get_leave_node_index(root, &pImageDataRGBfloat[fmtChan * i]);  // use decision tree to get indices for new colors
      for(k = 0; k<3; ++k) {
        err = pImageDataRGBfloat[fmtChan * i + k] - pPalette256[3 * pImageData[i] + k]; // compute color error
        //diffuse error with Floyd-Steinberg dithering.
        if(i % width < width-1){
          pImageDataRGBfloat[fmtChan * (i+1) + k] += factor * (7*err >> 4);
        }
        if(i < numPixel - width){
          pImageDataRGBfloat[fmtChan * (i+width) + k] += factor * (5*err >> 4);
          if(i % width > 0){
            pImageDataRGBfloat[fmtChan * (i+width-1) + k] += factor * (3*err >> 4);
          }
          if(i % width < width-1){
            pImageDataRGBfloat[fmtChan * (i+width+1) + k] += factor * (1*err >> 4);
          }
        }
        // Sierra dithering
        /*if(i % width < width-1){
          pImageDataRGBfloat[fmtChan * (i+1) + k] += factor * (5*err >> 5);
          if(i % width < width-2){
            pImageDataRGBfloat[fmtChan * (i+2) + k] += factor * (3*err >> 5);
          }
        }
        if(i < numPixel - width){
          pImageDataRGBfloat[fmtChan * (i+width) + k] += factor * (5*err >> 5);
          if(i % width > 0){
            pImageDataRGBfloat[fmtChan * (i+width-1) + k] += factor * (4*err >> 5);
            if(i % width > 1){
              pImageDataRGBfloat[fmtChan * (i+width-2) + k] += factor * (2*err >> 5);
            }
          }
          if(i % width < width-1){
            pImageDataRGBfloat[fmtChan * (i+width+1) + k] += factor * (4*err >> 5);
            if(i % width < width-2){
              pImageDataRGBfloat[fmtChan * (i+width+2) + k] += factor * (2*err >> 5);
            }
          }
          if(i < numPixel - 2*width){
            pImageDataRGBfloat[fmtChan * (i+2*width) + k] += factor * (3*err >> 5);
            if(i % width > 0){
              pImageDataRGBfloat[fmtChan * (i+2*width-1) + k] += factor * (2*err >> 5);
            }
            if(i % width < width-1){
              pImageDataRGBfloat[fmtChan * (i+2*width+1) + k] += factor * (2*err >> 5);
            }
          }
        }*/
      }
    }
  }
}

/* convert rgb image to indices and palette 
   The overall number of colors must already be <= 256 which is e.g. the case when the image comes directly from a GIF */
// pImageDataRGB: image in RGB (input)
// numPixel,width:number of pixels of input image, width (only used for dithering)
// fmtChan: select if image data are RGB or RGBa
// pImageData:    pointer to image as indices (output)
// palette:       pointer to color palette (output)
// pPalette256:   new color palette with 256 colors max.
// depthMax:      maximum depth of decision tree for colors quantization (sets number of colors)
static
uint32_t rgb_to_index(const uint8_t* pImageDataRGB, uint32_t numPixel, uint32_t width, cgif_chan_fmt fmtChan, uint8_t* pImageData, uint8_t* pPalette256, uint8_t depthMax, int* pHasAlpha, uint8_t* pBef, cgif_chan_fmt befFmtChan) {
  uint32_t i,j;
  uint32_t cntCollision;                                    // count the number of collisions
  int32_t  h;
  uint32_t  cnt       = 0;                                  // count the number of colors
  uint32_t  tableSize = 262147;                             // initial size of the hash table
  uint32_t tableSizeNew;                                    // size of hash-table when increasing its size
  tableSize = getNextPrimePower2(tableSize);                // increase table size to next prime number
  int       hasAlpha = 0;
  uint8_t   transIndex;
  uint32_t* colIdx = malloc(sizeof(uint32_t)*tableSize);    // index of the color
  uint32_t* frequ = malloc(sizeof(uint32_t)*tableSize);     // frequency of colors (histogram)
  uint8_t* hashTable = malloc(3 * tableSize);               // stores RGB vales at position determined by hash
  uint8_t* indexUsed = malloc(tableSize);                   // which part of the hash table was already used
  uint8_t* hashTable_new;                                   // for new hash table
  uint8_t* indexUsed_new;                                   // for new hash table
  uint32_t* frequ_new;                                      // for new hash table
  uint8_t* pPalette = malloc(3 * tableSize);                // color palette with all the colors (before color quantization)
  const uint8_t sizePixel = fmtChan;                        // number of bytes for one pixel (e.g. 3 for RGB, 4 for RGBa)
  memset(pPalette, 0, 3 * tableSize);                       // unused part of color table is uninitialized otheriwse
  memset(indexUsed, 0, tableSize);                          // initially no entry in hash-table is used

  for(i = 0; i < numPixel; ++i) {
    h = col_hash(&pImageDataRGB[sizePixel * i], hashTable, indexUsed, tableSize, fmtChan, &cntCollision);
    if(h == -1) { // -1 means that the user has set a user-defined transparency (alpha channel)
      hasAlpha = 1;
      continue; // do not take this pixel into account (e.g. alpha channel present)
    }
    if(indexUsed[h] == 0) {
      memcpy(&hashTable[3 * h],  &pImageDataRGB[sizePixel * i], 3); // add new color to hash table
      memcpy(&pPalette[3 * cnt], &pImageDataRGB[sizePixel * i], 3); // add new color to palette
      indexUsed[h] = 1;    // mark hash table entry as used
      colIdx[h]    = cnt;  // assign index to color
      frequ[h]     = 1;    // the new color occured 1x by now
      ++cnt;               // one new color added
    } else {               // if color exists already
      frequ[h] += 1;       // increment color histrogram
    }
    // resize the hash table (if more than half-full)
    if((cnt > (tableSize >> 1) || cntCollision > 30) && tableSize < 256*256*256) {
      tableSizeNew  = getNextPrimePower2(tableSize); // increase table size to the next prime number above the next power of two
      pPalette      = realloc(pPalette, 3 * tableSizeNew);
      colIdx        = realloc(colIdx, sizeof(uint32_t) * tableSizeNew);
      hashTable_new = malloc(3 * tableSizeNew);
      indexUsed_new = malloc(tableSizeNew);
      frequ_new     = malloc(sizeof(uint32_t) * tableSizeNew);
      memset(indexUsed_new, 0, tableSizeNew);
      cnt = 0;
      for(j = 0; j < tableSize; ++j) { // TBD: wouldn't it be easier to loop over pPalette and also leave pPalette in place?, if indexUsed is also unnecessary then
        if(indexUsed[j] == 1){
          h = col_hash(&hashTable[3 * j], hashTable_new, indexUsed_new, tableSizeNew, 3, &cntCollision); // recompute hash with new table size
          memcpy(&hashTable_new[3 * h], &hashTable[3 * j], 3);                             // put value from old hash table to right position of the new one
          memcpy(&pPalette[3 * cnt], &hashTable[3 * j], 3);
          indexUsed_new[h] = 1;
          frequ_new[h]     = frequ[j];
          colIdx[h]        = cnt;
          ++cnt;
        }
      }
      tableSize = tableSizeNew;
      free(hashTable);            // free old hash table that is not used anymore
      free(indexUsed);
      free(frequ);
      hashTable = hashTable_new;  // pass pointer to new hash table
      indexUsed = indexUsed_new;
      frequ = frequ_new;
    }
  }

  const uint16_t colMax = (1uL << depthMax) - 1; // maximum number of colors (-1 to leave space for transparency), TBD: maybe no quantization for static image with 256 colors
  if(cnt > colMax) { // color-quantization is needed
    uint32_t* pFrequDense = hash_to_dense(pPalette, frequ, cnt, hashTable, indexUsed, tableSize, fmtChan);
    treeNode* root        = create_decision_tree(pPalette, pFrequDense, pPalette256, cnt, colMax, depthMax); // create decision tree (dynamic, splits along rgb-dimension with highest variance)
    float* pImageDataRGBfloat = malloc(fmtChan * numPixel * sizeof(float)); // TBD fmtChan + only when hasAlpha
    for(i = 0; i < fmtChan * numPixel; ++i){
      pImageDataRGBfloat[i] = pImageDataRGB[i];
    }
    transIndex = colMax;
    get_quantized_dithered_image(pImageData, pImageDataRGBfloat, pPalette256, root, numPixel, width, true, transIndex, fmtChan, pBef, befFmtChan, hasAlpha); // do color quantization and dithering
    free(pImageDataRGBfloat); // RGB image is not needed anymore
    free_decision_tree(root); // tree for color quantization is not needed anymore
    free(pFrequDense);
    cnt = colMax;
  } else { // no color-quantization is needed if the number of colors is small enough
    transIndex = cnt;
    for(i = 0; i < numPixel; ++i) {
      if(!hasAlpha && pBef) {
        if(memcmp(&pImageData[sizePixel * i], &pBef[befFmtChan * i], 3) == 0) {
          pImageData[i] = transIndex;
          continue;
        }
      }
      h = col_hash(&pImageDataRGB[sizePixel * i], hashTable, indexUsed, tableSize, fmtChan, &cntCollision);
      pImageData[i] = (h == -1) ? transIndex : colIdx[h];
    }
    memcpy(pPalette256, pPalette, 3 * 256); // keep the color palette (no quantization)
  }

  *pHasAlpha = hasAlpha;
  free(colIdx);
  free(hashTable);
  free(indexUsed);
  free(pPalette);
  free(frequ);
  return cnt; // return number of colors found
}

CGIFrgb* cgif_rgb_newgif(const CGIFrgb_Config* pConfig) {
  CGIF_Config idxConfig = {0};
  CGIFrgb* pGIFrgb;
  
  pGIFrgb = malloc(sizeof(CGIFrgb));
  memset(pGIFrgb, 0, sizeof(CGIFrgb));
  idxConfig.pWriteFn  = pConfig->pWriteFn;
  idxConfig.pContext  = pConfig->pContext;
  idxConfig.path      = pConfig->path;
  idxConfig.numLoops  = pConfig->numLoops;
  idxConfig.width     = pConfig->width;
  idxConfig.height    = pConfig->height;
  idxConfig.attrFlags = CGIF_ATTR_IS_ANIMATED | CGIF_ATTR_NO_GLOBAL_TABLE;
  pGIFrgb->pGIF       = cgif_newgif(&idxConfig);
  if(pGIFrgb->pGIF == NULL) {
    free(pGIFrgb);
    return NULL;
  }
  pGIFrgb->config = *pConfig;
  return pGIFrgb;  
}

cgif_result cgif_rgb_addframe(CGIFrgb* pGIF, const CGIFrgb_FrameConfig* pConfig) {
  uint8_t          aPalette[256 * 3];
  memset(aPalette, 0, 256 * 3);       // rgb_to_index does not necessarily fill/use all palette entries (-> initialize aPalette to avoid underfined behaviour)
  CGIF_FrameConfig fConfig     = {0};
  uint8_t* pNewBef;
  const uint16_t   imageWidth  = pGIF->config.width;
  const uint16_t   imageHeight = pGIF->config.height;
  const uint32_t   numPixel    = MULU16(imageWidth, imageHeight);
  int              hasAlpha;
  
  pNewBef = malloc(pConfig->fmtChan * MULU16(imageWidth, imageHeight));
  memcpy(pNewBef, pConfig->pImageData, pConfig->fmtChan * MULU16(imageWidth, imageHeight));
  fConfig.pLocalPalette = aPalette;
  fConfig.pImageData    = malloc(pGIF->config.width * (uint32_t)pGIF->config.height);
  fConfig.delay         = pConfig->delay;
  fConfig.attrFlags     = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;

  const int sizeLCT      = rgb_to_index(pConfig->pImageData, numPixel, pGIF->config.width, pConfig->fmtChan, fConfig.pImageData, aPalette, 8, &hasAlpha, pGIF->pBefImageData, pGIF->befFmtChan);
  fConfig.numLocalPaletteEntries = sizeLCT;
  if(hasAlpha) {
    fConfig.attrFlags   |= CGIF_FRAME_ATTR_HAS_ALPHA;
    fConfig.transIndex   = sizeLCT;
  } else {
    fConfig.attrFlags |= CGIF_FRAME_ATTR_HAS_SET_TRANS;
    fConfig.transIndex = sizeLCT;
  }
  cgif_result r = cgif_addframe(pGIF->pGIF, &fConfig);
  free(pGIF->pBefImageData);
  pGIF->pBefImageData = pNewBef;
  pGIF->befFmtChan    = pConfig->fmtChan;

  // cleanup
  free(fConfig.pImageData);
  return r;
}

cgif_result cgif_rgb_close(CGIFrgb* pGIF) {
  cgif_result r = cgif_close(pGIF->pGIF);
  free(pGIF->pBefImageData);
  free(pGIF);
  return r;
}
