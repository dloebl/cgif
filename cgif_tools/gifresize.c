#include <cgif.h>
#include <libnsgif.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define MULU16(a, b) (((uint32_t)a) * ((uint32_t)b)) // helper macro to correctly multiply two U16's without default signed int promotion

/* callbacks for libnsgif */
static void* cb_create(int width, int height) {
  return malloc(width * height * 4);
}

static void cb_destroy(void* bitmap) {
  free(bitmap);
}

static uint8_t* cb_get_buffer(void* bitmap) {
  return (uint8_t*)bitmap;
}

/* helper function to get file size of open file in C99 */
static uint64_t getFileSize(FILE* pFile) {
  long curPos = ftell(pFile);
  fseek(pFile, 0, SEEK_END);
  long endPos = ftell(pFile);
  fseek(pFile, curPos, SEEK_SET);
  return endPos;
}

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

/* resize image without any color averaging and reusing (RGB) colors from before */
static uint8_t* resize_naive_rgb(const uint8_t* pImageData, uint16_t width, uint16_t height, uint16_t width_new, uint16_t height_new){
  uint8_t* pImgResized;
  uint16_t original_x, original_y;

  pImgResized = malloc(MULU16(3 * width_new, height_new)); // 3 for three colors in RGB
  for(uint16_t i = 0; i < height_new; ++i) {
    for(uint16_t j = 0; j < width_new; ++j) {
      original_y = MULU16(height, i) / height_new;
      original_x = MULU16(width, j) / width_new;
      pImgResized[MULU16(3 * i, width_new) + 3 * j]     = pImageData[MULU16(4 * original_y, width) + 4 * original_x];
      pImgResized[MULU16(3 * i, width_new) + 3 * j + 1] = pImageData[MULU16(4 * original_y, width) + 4 * original_x + 1];
      pImgResized[MULU16(3 * i, width_new) + 3 * j + 2] = pImageData[MULU16(4 * original_y, width) + 4 * original_x + 2];
    }
  }
  return pImgResized;
}

/* get a hash from rgb color values (use open addressing, if entry does not exist, next free spot is returned) */
static uint32_t col_hash(const uint8_t* rgb, const uint8_t* hashTable, const uint8_t* indexUsed, const uint32_t tableSize){
  uint32_t h;
  const int p = 700001; // prime number for hashing

  h = ((rgb[0] * 256 * 256 + rgb[1] * 256 + rgb[2]) % p) % tableSize;
  while(1) {
    if(indexUsed[h] == 0 || memcmp(rgb, &hashTable[3 * h], 3) == 0){
      return h;
    } else {
      ++h; // go on searching for a free spot
      h = h % tableSize; //start over
    }
  }
  return h;
}

/* take array indexed by hash(rgb) and turn it into a dense array */
static uint32_t* hash_to_dense(const uint8_t* pPalette, const uint32_t* arry, uint32_t cnt, const uint8_t* hashTable, const uint8_t* indexUsed, uint32_t tableSize){
  // pPalette: color tabel with cnt entries
  // arry: array indexed by hash of RGB-value in pPalette
  uint32_t* arryDense = malloc(sizeof(uint32_t) * cnt);
  uint32_t h;

  for(uint32_t i = 0; i < cnt; ++i) {
    h = col_hash(&pPalette[3 * i], hashTable, indexUsed, tableSize);
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
static uint8_t get_leave_node_index(const treeNode* root, const uint8_t* rgb) {
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

/* get index in static tree with fixed boxes for rgb */
static uint8_t get_static_tree_index_256(const uint8_t* rgb){
  return (rgb[0] / 43) * 43 + (rgb[1] / 32) * 5 + rgb[2] / 52; // 6x8x5=240 boxes for rgb
}

/* static tree to make very simple color quantization -- always gives 256 colors */
static uint8_t quantize_static_tree(uint8_t* pPalette, uint8_t* pPalette256, uint8_t* hashTable, uint8_t* indexUsed, uint32_t* frequ, uint32_t cnt, uint32_t tableSize){
  // pPalette: color palette indexed without gap (max index: cnt-1)
  // frequ: frequency of a color (indexed by hash generated from rgb color in pPalette)
  uint32_t* sum = malloc(256*sizeof(uint32_t));
  uint32_t* pPaletteSum = malloc(3*256*sizeof(uint32_t));
  uint32_t i, h;
  uint8_t idx;
  memset(sum, 0, 256 * sizeof(uint32_t));
  memset(pPaletteSum, 0, 3 * 256 * sizeof(uint32_t));
  for(i = 0; i < cnt; ++i){
    idx = get_static_tree_index_256(&pPalette[3 * i]);
    h   = col_hash(&pPalette[3*i], hashTable, indexUsed, tableSize);
    pPaletteSum[3 * idx] = pPaletteSum[3 * idx] + frequ[h] * pPalette[3 * i];             // red
    pPaletteSum[3 * idx + 1] = pPaletteSum[3 * idx + 1] + frequ[h] * pPalette[3 * i + 1]; // green
    pPaletteSum[3 * idx + 2] = pPaletteSum[3 * idx + 2] + frequ[h] * pPalette[3 * i + 2]; // blue
    sum[idx] = sum[idx] + frequ[h];
  }
  for(i = 0; i < 256; ++i){
    if(sum[i] > 0){ // the box can be empty (color is in color-table but won't be used), round down
      pPalette256[3 * i] = pPaletteSum[3 * i] / sum[i];
      pPalette256[3 * i + 1] = pPaletteSum[3 * i + 1] / sum[i];
      pPalette256[3 * i + 2] = pPaletteSum[3 * i + 2] / sum[i];
    }
  }
  free(sum);
  free(pPaletteSum);
  uint8_t colMax[3] = {255, 255, 255};
  return get_static_tree_index_256(colMax) + 1; // return maximum number of colors
}

/* convert rgb image to indices and palette 
   The overall number of colors must already be <= 256 which is e.g. the case when the image comes directly from a GIF */
// pImageDataRGB: image in RGB (input)
// numPixel:      number of pixels of input image
// pImageData:    pointer to image as indices (output)
// palette:       pointer to color palette (output)
// pPalette256:   new color palette with 256 colors max.
// depthMax:      maximum depth of decision tree for colors quantization (sets number of colors)
static uint32_t rgb_to_index(const uint8_t* pImageDataRGB, uint32_t numPixel, uint8_t* pImageData, uint8_t* pPalette256, uint8_t depthMax) {
  uint32_t i,j,h;
  uint32_t  cnt       = 0;                                  // count the number of colors
  uint32_t  tableSize = 512;                                // size of the hash table
  uint32_t* colIdx = malloc(sizeof(uint32_t)*tableSize);    // index of the color
  uint32_t* frequ = malloc(sizeof(uint32_t)*tableSize);     // frequency of colors
  uint8_t* hashTable = malloc(3 * tableSize);               // stores RGB vales at position determined by hash
  uint8_t* indexUsed = malloc(tableSize);                   // which part of the hash table was already used
  uint8_t* hashTable_new = malloc(3 * tableSize);           // for new hash table
  uint8_t* indexUsed_new = malloc(tableSize);               // for new hash table
  uint32_t* frequ_new = malloc(sizeof(uint32_t)*tableSize); // for new hash table
  uint8_t* pPalette = malloc(3 * tableSize);

  memset(pPalette, 0, 3 * tableSize);                       // unused part of color table is uninitialized otheriwse
  memset(indexUsed, 0, tableSize);
  for(i = 0; i < numPixel; ++i){
    h = col_hash(&pImageDataRGB[3 * i], hashTable, indexUsed, tableSize);
    if(indexUsed[h] == 0){
      memcpy(&hashTable[3 * h],  &pImageDataRGB[3 * i], 3); // add new color to hash table
      memcpy(&pPalette[3 * cnt], &pImageDataRGB[3 * i], 3); // add new color to palette
      indexUsed[h] = 1;
      colIdx[h]    = cnt;
      frequ[h]     = 1;
      ++cnt; // one new color added
    } else { // if color exists already
      frequ[h] += 1;
    }
    // resize the hash table (if more than half-full)
    if(cnt > (tableSize >> 1)) {
      pPalette      = realloc(pPalette, 3 * 2 * tableSize);
      colIdx        = realloc(colIdx, sizeof(uint32_t) * 2 * tableSize);
      hashTable_new = realloc(hashTable_new, 3 * 2 * tableSize);
      indexUsed_new = realloc(indexUsed_new, 2 * tableSize);
      frequ_new     = realloc(frequ_new, sizeof(uint32_t) * 2 * tableSize);
      memset(indexUsed_new, 0, 2 * tableSize);
      cnt = 0;
      for(j = 0; j < tableSize; ++j){
        if(indexUsed[j] == 1){
          h = col_hash(&hashTable[3 * j], hashTable_new, indexUsed_new, 2 * tableSize); // recompute hash
          memcpy(&hashTable_new[3 * h], &hashTable[3 * j], 3);
          memcpy(&pPalette[3 * cnt], &hashTable[3 * j], 3);
          indexUsed_new[h] = 1;
          frequ_new[h]     = frequ[j];
          colIdx[h]        = cnt;
          ++cnt;
        }
      }
      tableSize <<= 1; // double table size
      hashTable = realloc(hashTable, 3 * tableSize);
      indexUsed = realloc(indexUsed, tableSize);
      frequ     = realloc(frequ, sizeof(uint32_t) * tableSize);
      memcpy(hashTable, hashTable_new, 3 * tableSize);
      memcpy(indexUsed, indexUsed_new, tableSize);
      memcpy(frequ, frequ_new, tableSize*sizeof(uint32_t));
    }
  }

  const uint16_t colMax = (1uL << depthMax) - 1; // maximum number of colors (-1 to leave space for transparency), TBD: maybe no quantization for static image with 256 colors
  if(cnt > colMax) { // color-quantization is needed
    uint32_t* pFrequDense = hash_to_dense(pPalette, frequ, cnt, hashTable, indexUsed, tableSize);
    treeNode* root        = create_decision_tree(pPalette, pFrequDense, pPalette256, cnt, colMax, depthMax); // create decision tree (dynamic, splits along rgb-dimension with highest variance)
    for(i = 0; i < numPixel; ++i) {
      pImageData[i] = get_leave_node_index(root, &pImageDataRGB[3 * i]);  // use decision tree to get indices for new colors
    }
    //cnt = quantize_static_tree(pPalette, pPalette256, hashTable, indexUsed, frequ, cnt, tableSize); // create static tree for color quantization
    //for(i = 0; i < numPixel; ++i) pImageData[i] = get_static_tree_index_256(&pImageDataRGB[3 * i]); // use static tree to get new colors
    free_decision_tree(root);
    free(pFrequDense);
    cnt = colMax;
  } else { // no color-quantization is needed
    for(i = 0; i < numPixel; ++i){
      h = col_hash(&pImageDataRGB[3 * i], hashTable, indexUsed, tableSize);
      pImageData[i] = colIdx[h];
    }
    memcpy(pPalette256, pPalette, 3 * 256);
  }

  free(colIdx);
  free(hashTable);
  free(indexUsed);
  free(hashTable_new);
  free(indexUsed_new);
  free(pPalette);
  free(frequ);
  free(frequ_new);
  return cnt; // return number of colors found
}

// gifresize <input-gif> <output-gif> <new-width> <new-height>
int main(int argn, char** pArgs) {
  gif_result  resultDec;
  cgif_result resultEnc;

  if(argn != 5) {
    fprintf(stderr, "%s\n", "invalid number of arguments\nsyntax:\ngifresize <input-gif> <output-gif> <new-width> <new-height>");
    return 1; // error
  }
  char* sInput = pArgs[1];
  char* sOutput = pArgs[2];
  uint16_t newWidth = atoi(pArgs[3]);
  uint16_t newHeight = atoi(pArgs[4]);
  if(newWidth == 0 || newHeight == 0) {
    fprintf(stderr, "%s\n", "invalid new width or new height");
    return 2; // error
  }
  //
  // read in GIF via libnsgif
  FILE* pFile = fopen(sInput, "rb");
  if(!pFile) {
    fprintf(stderr, "%s\n", "failed to open <input-gif>");
    return 3; // error
  }
  uint64_t fileSize = getFileSize(pFile);
  uint8_t* pGIFData = malloc(fileSize);
  size_t cnt = fread(pGIFData, 1, fileSize, pFile);
  fclose(pFile);
  if(cnt != fileSize) {
    fprintf(stderr, "%s\n", "failed to read from <input-gif>");
    free(pGIFData);
    return 4; // error
  }

  // read GIF
  gif_animation dGIF;
  gif_bitmap_callback_vt bcb = { cb_create, cb_destroy, cb_get_buffer, NULL, NULL, NULL };
  gif_create(&dGIF, &bcb);
  resultDec = gif_initialise(&dGIF, fileSize, pGIFData);
  //
  // check for errors
  if(resultDec != GIF_OK) {
    fprintf(stderr, "%s\n", "<input-gif> is not a valid GIF");
    free(pGIFData);
    return 5;
  }
  //
  // init GIF and Frame config
  CGIF_Config gConf      = {0};
  gConf.path             = sOutput;
  gConf.attrFlags        = CGIF_ATTR_IS_ANIMATED | CGIF_ATTR_NO_GLOBAL_TABLE;
  gConf.width            = newWidth;
  gConf.height           = newHeight;
  gConf.numLoops         = dGIF.loop_count;
  CGIF* eGIF             = cgif_newgif(&gConf);
  if(eGIF == NULL) {
    fprintf(stderr, "failed to create output GIF\n");
    gif_finalise(&dGIF);
    free(pGIFData);
    return 6;
  }
  uint8_t* pImageData = malloc(MULU16(newWidth, newHeight));
  uint8_t* aPalette = malloc(256 * 3); // max size of palette
  CGIF_FrameConfig fConf = {0};
  fConf.pLocalPalette    = aPalette;
  fConf.pImageData       = pImageData;
  fConf.attrFlags        = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
  fConf.genFlags         = CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;

  // encode frame by frame. generate one local color table per frame.
  for(uint32_t i = 0; i < dGIF.frame_count; ++i) {
    resultDec = gif_decode_frame(&dGIF, i);
    if(resultDec != GIF_OK) break; // decode error occurred
    fConf.delay = dGIF.frames[i].frame_delay;
    uint8_t* pRGBA   = dGIF.frame_image;
    uint8_t* pNewRGB = resize_naive_rgb(pRGBA, dGIF.width, dGIF.height, newWidth, newHeight); // resize RGBA frame
    int cntBlock  = rgb_to_index(pNewRGB, MULU16(newWidth, newHeight), pImageData, aPalette, 8); // get local palette from RGB frame
    fConf.numLocalPaletteEntries = cntBlock;
    resultEnc = cgif_addframe(eGIF, &fConf);
    free(pNewRGB);
    if(resultEnc != CGIF_OK) break; // encode error occurred
  }
  resultEnc = cgif_close(eGIF);
  gif_finalise(&dGIF);
  //
  // free stuff
  free(pImageData);
  free(aPalette);
  free(pGIFData);
  // check for decoder errors
  if(resultDec != GIF_OK) {
    fprintf(stderr, "%s\n", "failed to decode frame");
    return 7;
  }
  // check for encoder errors
  if(resultEnc != CGIF_OK) {
    fprintf(stderr, "failed to create output GIF\n");
    return 8;
  }
  return 0;
}
