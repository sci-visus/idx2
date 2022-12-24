#include "idx2Common.h"
#include "InputOutput.h"
#include "Math.h"


#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#endif
#endif
#define SEXPR_IMPLEMENTATION
#include "sexpr.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
#include "zstd/zstd.c"
#include "zstd/zstd.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace idx2
{

free_list_allocator BrickAlloc_;


void
Dealloc(params* P)
{
}


void
SetName(idx2_file* Idx2, cstr Name)
{
  snprintf(Idx2->Name, sizeof(Idx2->Name), "%s", Name);
}


void
SetVersion(idx2_file* Idx2, const v2i& Ver)
{
  Idx2->Version = Ver;
}


void
SetDimensions(idx2_file* Idx2, const v3i& Dims3)
{
  Idx2->Dims3 = Dims3;
}


void
SetDataType(idx2_file* Idx2, dtype DType)
{
  Idx2->DType = DType;
}


void
SetNumLevels(idx2_file* Idx2, i8 NLevels)
{
  Idx2->NLevels = NLevels;
}


void
SetTolerance(idx2_file* Idx2, f64 Tolerance)
{
  Idx2->Tolerance = Tolerance;
}


void
SetBitPlanesPerChunk(idx2_file* Idx2, int BitPlanesPerChunk)
{
  Idx2->BitPlanesPerChunk = BitPlanesPerChunk;
}


void
SetBitPlanesPerFile(idx2_file* Idx2, int BitPlanesPerFile)
{
  Idx2->BitPlanesPerFile = BitPlanesPerFile;
}


void
SetDir(idx2_file* Idx2, stref Dir)
{
  Idx2->Dir = Dir;
}


void
SetDownsamplingFactor(idx2_file* Idx2, const v3i& DownsamplingFactor3)
{
  Idx2->DownsamplingFactor3 = DownsamplingFactor3;
}


/* Write the metadata file (idx) */
// TODO: return error type
// TODO NEXT
void
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName)
{
  FILE* Fp = fopen(FileName, "w");
  fprintf(Fp, "(\n"); // begin (
  fprintf(Fp, "  (common\n");
  fprintf(Fp, "    (type \"Simulation\")\n"); // TODO: add this config to Idx2
  fprintf(Fp, "    (name \"%s\")\n", P.Meta.Name);
  fprintf(Fp, "    (field \"%s\")\n", P.Meta.Field);
  fprintf(Fp, "    (dimensions %d %d %d)\n", idx2_PrV3i(Idx2.Dims3));
  stref DType = ToString(Idx2.DType);
  fprintf(Fp, "    (data-type \"%s\")\n", idx2_PrintScratchN(Size(DType), "%s", DType.ConstPtr));
  fprintf(Fp, "    (min-max %.20f %.20f)\n", Idx2.ValueRange.Min, Idx2.ValueRange.Max);
  fprintf(Fp, "    (accuracy %.20f)\n", Idx2.Tolerance);
  fprintf(Fp, "  )\n"); // end common)
  fprintf(Fp, "  (format\n");
  fprintf(Fp, "    (version %d %d)\n", Idx2.Version[0], Idx2.Version[1]);
  char TransformOrder[128];
  // TODO NEXT
  //DecodeTransformOrder(Idx2.TransformOrder, TransformOrder);
  fprintf(Fp, "    (transform-order \"%s\")\n", TransformOrder);
  fprintf(Fp, "    (num-levels %d)\n", Idx2.NLevels);
  fprintf(Fp, "    (bit-planes-per-chunk %d)\n", Idx2.BitPlanesPerChunk);
  fprintf(Fp, "  )\n"); // end format)
  fprintf(Fp, ")\n");   // end )
  fclose(Fp);
}


// TODO NEXT
error<idx2_err_code>
ReadMetaFileFromBuffer(idx2_file* Idx2, buffer& Buf)
{
  SExprResult Result = ParseSExpr((cstr)Buf.Data, Size(Buf), nullptr);
  if (Result.type == SE_SYNTAX_ERROR)
  {
    fprintf(stderr, "Error(%d): %s.\n", Result.syntaxError.lineNumber, Result.syntaxError.message);
    return idx2_Error(idx2_err_code::SyntaxError);
  }
  else
  {
    SExpr* Data = (SExpr*)malloc(sizeof(SExpr) * Result.count);
    idx2_CleanUp(free(Data));
    array<SExpr*> Stack;
    Reserve(&Stack, Result.count);
    idx2_CleanUp(Dealloc(&Stack));
    // This time we supply the pool
    SExprPool Pool = { Result.count, Data };
    Result = ParseSExpr((cstr)Buf.Data, Size(Buf), &Pool);
    // result.expr contains the successfully parsed SExpr
    //    printf("parse .idx2 file successfully\n");
    PushBack(&Stack, Result.expr);
    bool GotId = false;
    SExpr* LastExpr = nullptr;
    while (Size(Stack) > 0)
    {
      SExpr* Expr = Back(Stack);
      PopBack(&Stack);
      if (Expr->next)
        PushBack(&Stack, Expr->next);
      if (GotId)
      {
        if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "version"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Version[0] = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Version[1] = Expr->i;
          //          printf("Version = %d.%d\n", Idx2->Version[0], Idx2->Version[1]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "name"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          memcpy(Idx2->Name, Buf.Data + Expr->s.start, Expr->s.len);
          Idx2->Name[Expr->s.len] = 0;
          //          printf("Name = %s\n", Idx2->Name);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "dimensions"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Dims3.X = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Dims3.Y = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Dims3.Z = Expr->i;
          //          printf("Dims = %d %d %d\n", idx2_PrV3i(Idx2->Dims3));
        }
        if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "tolerance"))
        {
          idx2_Assert(Expr->type == SE_FLOAT);
          Idx2->Tolerance = Expr->f;
          //          printf("Accuracy = %.17g\n", Idx2->Accuracy);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "data-type"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          Idx2->DType = StringTo<dtype>()(stref((cstr)Buf.Data + Expr->s.start, Expr->s.len));
          //          printf("Data type = %.*s\n", ToString(Idx2->DType).Size,
          //          ToString(Idx2->DType).ConstPtr);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "min-max"))
        {
          idx2_Assert(Expr->type == SE_FLOAT || Expr->type == SE_INT);
          Idx2->ValueRange.Min = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_FLOAT || Expr->type == SE_INT);
          Idx2->ValueRange.Max = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "transform-order"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          // TODO NEXT
          //Idx2->TransformOrder =
          //  EncodeTransformOrder(stref((cstr)Buf.Data + Expr->s.start, Expr->s.len));
          //char TransformOrder[128];
          //DecodeTransformOrder(Idx2->TransformOrder, TransformOrder);
          //          printf("Transform order = %s\n", TransformOrder);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "num-levels"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->NLevels = i8(Expr->i);
          //          printf("Num levels = %d\n", Idx2->NLevels);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "bits-per-brick"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->BitsPerBrick = Expr->i;
          //          printf("Bricks per chunk = %d\n", Idx2->BricksPerChunks[0]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "brick-bits-per-chunk"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->BrickBitsPerChunk = Expr->i;
          //          printf("Bricks per chunk = %d\n", Idx2->BricksPerChunks[0]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "chunk-bits-per-file"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->ChunkBitsPerFile = Expr->i;
          //          printf("Chunks per file = %d\n", Idx2->ChunksPerFiles[0]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "file-bits-per-directory"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->FileBitsPerDir = Expr->i;
          //          printf("Files per directory = %d\n", Idx2->FilesPerDir);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "bit-planes-per-chunk"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->BitPlanesPerChunk = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "bit-planes-per-file"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->BitPlanesPerFile = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "transform-template"))
        {
          // TODO NEXT
        }
      }
      if (Expr->type == SE_ID)
      {
        LastExpr = Expr;
        GotId = true;
      }
      else if (Expr->type == SE_LIST)
      {
        PushBack(&Stack, Expr->head);
        GotId = false;
      }
      else
      {
        GotId = false;
      }
    }
  }
  return idx2_Error(idx2_err_code::NoError);
}

error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName)
{
  buffer Buf;
  idx2_CleanUp(DeallocBuf(&Buf));
  idx2_PropagateIfError(ReadFile(FileName, &Buf));
  return ReadMetaFileFromBuffer(Idx2, Buf);
}


v3i
GetDimsFromTemplate(stref Template)
{
  v3i Dims3(1);
  idx2_For (i8, I, 0, Template.Size)
  {
    idx2_Assert(isalpha(Template[I]) && islower(Template[I]));
    i8 D = Template[I] - 'x'; // TODO NEXT: use a table to translate character to number
    Dims3[D] *= 2;
  }
  return Dims3;
}


// TODO NEXT: return an error (the template may be invalid)
/* The full template can be
zzzyy::xyz:xyz:xy (zzzyy is the prefix), or
::xyz:xyz:zzz:yyy (there is no prefix)
*/
// TODO NEXT: we have changed the syntax of the template (there is now a prefix and a middle part)
// the difference is that the prefix is not used until the end (just add onto whatever address we have computed for the files)
static void
ProcessTransformTemplate(idx2_file* Idx2)
{
  // TODO NEXT: check the syntax of the template
  transform_template& Template = Idx2->Template;
  tokenizer Tokenizer(Template.Full.ConstPtr, ":");

  /* parse the prefix (if any) */
  stref Part = Next(&Tokenizer); // this is the prefix if it exists, else it is the first part of the postfix
  i8 Pos = i8(Part.ConstPtr - Template.Full.ConstPtr);
  if (Pos == 0) // there is a prefix
    Template.Prefix = stref(Template.Full.ConstPtr, Part.Size);
  else // there is no prefix, reset to parse from the beginning
    Reset(&Tokenizer);

  /* parse the postfix */
  i8 DstPos = 0;
  while (stref Part = Next(&Tokenizer))
  {
    i8 Pos = i8(Part.ConstPtr - Template.Full.ConstPtr);
    i8 Size = Part.Size;
    stref Source(Template.Full.ConstPtr + Pos, Size);
    stref Destination(Template.Postfix.Data + DstPos, Size);
    Copy(Source, &Destination, true);
    PushBack(&Template.LevelParts, v2<i8>(DstPos, Size));
    DstPos += Size;
  }

  /* reverse so that the LevelParts start from level 0 */
  Reverse(Begin(Template.LevelParts), End(Template.LevelParts));
}

idx2_Inline static stref
GetPostfixTemplate(const idx2_file& Idx2, i8 Pos)
{
  return stref(Idx2.Template.Postfix.Data + Pos, Idx2.Template.Postfix.Len - Pos);
}

idx2_Inline static stref
GetPostfixTemplate(const idx2_file& Idx2, i8 Pos, i8 Size)
{
  return stref(Idx2.Template.Postfix.Data + Pos, Size);
}

idx2_Inline static stref
GetPostfixTemplatePart(const transform_template& Template, const v2<i8>& Part)
{
  return stref(Template.Postfix.Data + Part[0], Part[1]);
}

idx2_Inline static stref
GetPostfixTemplateForLevel(const idx2_file& Idx2, i8 Level)
{
  return GetPostfixTemplatePart(Idx2.Template, Idx2.Template.LevelParts[Level]);
}


static void
BuildSubbandsForAllLevels(idx2_file* Idx2)
{
  idx2_For (i8, L, 0, Idx2->NLevels)
  {
    stref TemplatePart = GetPostfixTemplateForLevel(*Idx2, L);
    subbands_per_level SubbandsOnLevelL;
    const v3i& Dims3 = Idx2->BrickInfo[L].Dims3Pow2;
    const v3i& Spacing3 = Idx2->BrickInfo[L].Spacing3;
    // TODO NEXT: do we need both versions?
    SubbandsOnLevelL.PowOf2 = BuildSubbandsForOneLevel(TemplatePart, Idx2->DimensionMap, Dims3, Spacing3);
    SubbandsOnLevelL.PowOf2Plus1 = BuildSubbandsForOneLevel(TemplatePart, Idx2->DimensionMap, idx2_ExtDims(Dims3), Spacing3);
    PushBack(&Idx2->Subbands, SubbandsOnLevelL);
  }
}


/* compute the list of subbands to decode given the downsampling factor */
static void
ComputeSubbandMasks(idx2_file* Idx2, const params& P)
{
  v3i Spacing3 = v3i(1) << P.DownsamplingFactor3;
  idx2_For (int, L, 0, Idx2->NLevels)
  {
    subbands_per_level& SubbandsOnLevelL = Idx2->Subbands[L];
    u8 Mask = 0xFF;
    idx2_For (i8, Sb, 0, Size(SubbandsOnLevelL.PowOf2Plus1))
    {
      v3i F3 = From(SubbandsOnLevelL.PowOf2Plus1[Sb].GlobalGrid);
      v3i S3 = Spacing(SubbandsOnLevelL.PowOf2Plus1[Sb].GlobalGrid);
      SubbandsOnLevelL.Spacings[Sb] = v3i(1);
      for (int D = 0; D < 3; ++D)
      {
        // check if a grid1 that starts at F3 with spacing S3 is a subgrid
        // of a grid2 that starts at 0 with spacing Spacing3
        if ((F3[D] % Spacing3[D]) == 0)
        {
          if ((S3[D] % Spacing3[D]) != 0)
            SubbandsOnLevelL.Spacings[Sb][D] = Spacing3[D] / S3[D];
        }
        else // skip the subband
        {
          Mask = UnsetBit(Mask, Sb);
        }
      }
    }
    if (L + 1 < Idx2->NLevels && Mask == 1)
      Mask = 0; // explicitly disable subband 0 if it is the only subband to decode
    SubbandsOnLevelL.DecodeMasks = Mask;
  }
}


// TODO NEXT: check the values of BrickBitsPerChunk etc to make sure they are under the limits
static void
ComputeBrickChunkFileInfo(idx2_file* Idx2, const params& P)
{
  Resize(&Idx2->BrickInfo, Idx2->NLevels);
  Resize(&Idx2->ChunkInfo, Idx2->NLevels);
  Resize(&Idx2->FileInfo, Idx2->NLevels);

  v3i NBricks3;
  idx2_For (i8, L, 0, Idx2->NLevels)
  {
    brick_info_per_level& BrickInfoOnLevelL = Idx2->BrickInfo[L];
    chunk_info_per_level& ChunkInfoOnLevelL = Idx2->ChunkInfo[L];
    file_info_per_level& FileInfoOnLevelL = Idx2->FileInfo[L];

    /* compute Group3 */
    stref Template = GetPostfixTemplateForLevel(*Idx2, L);
    BrickInfoOnLevelL.Group3 = GetDimsFromTemplate(Template);

    /* compute brick templates */
    i8 Length = Sum<i8>(Idx2->Template.LevelParts[L]);
    BrickInfoOnLevelL.Template = GetPostfixTemplateForLevel(*Idx2, L);
    i8 BrickBits = Min(Length, Idx2->BitsPerBrick);
    i8 BrickIndexBits = Length - BrickBits;
    BrickInfoOnLevelL.IndexTemplate = GetPostfixTemplate(*Idx2, 0, BrickIndexBits);
    BrickInfoOnLevelL.Template = GetPostfixTemplate(*Idx2, BrickIndexBits, BrickBits);
    BrickInfoOnLevelL.Dims3Pow2 = GetDimsFromTemplate(BrickInfoOnLevelL.Template);
    BrickInfoOnLevelL.Spacing3 = GetDimsFromTemplate(GetPostfixTemplate(*Idx2, Length));
    if (L == 0)
      NBricks3 = (Idx2->Dims3 + BrickInfoOnLevelL.Dims3Pow2 - 1) / BrickInfoOnLevelL.Dims3Pow2;
    else
      NBricks3 = (NBricks3 + BrickInfoOnLevelL.Group3 - 1) / BrickInfoOnLevelL.Group3;
    BrickInfoOnLevelL.NBricks3 = NBricks3;

    /* compute chunk templates */
    i8 ChunkBits = Min(Length, i8(Idx2->BitsPerBrick + Idx2->BrickBitsPerChunk));
    i8 ChunkIndexBits = Length - ChunkBits;
    BrickInfoOnLevelL.IndexTemplateInChunk = GetPostfixTemplate(*Idx2, ChunkIndexBits, BrickIndexBits - ChunkIndexBits);
    BrickInfoOnLevelL.NBricksPerChunk3 = GetDimsFromTemplate(BrickInfoOnLevelL.IndexTemplateInChunk);
    ChunkInfoOnLevelL.Template = GetPostfixTemplate(*Idx2, ChunkIndexBits, ChunkBits);
    ChunkInfoOnLevelL.IndexTemplate = GetPostfixTemplate(*Idx2, 0, ChunkIndexBits);
    ChunkInfoOnLevelL.NChunks3 = (BrickInfoOnLevelL.NBricks3 + BrickInfoOnLevelL.NBricksPerChunk3 - 1) / BrickInfoOnLevelL.NBricksPerChunk3;

    /* compute file templates */
    i8 FileBits = Min(Length, i8(Idx2->BitsPerBrick + Idx2->BrickBitsPerChunk + Idx2->ChunkBitsPerFile));
    i8 FileIndexBits = Length - FileBits;
    ChunkInfoOnLevelL.IndexTemplateInFile = GetPostfixTemplate(*Idx2, FileIndexBits, ChunkIndexBits - FileIndexBits);
    ChunkInfoOnLevelL.NChunksPerFile3 = GetDimsFromTemplate(ChunkInfoOnLevelL.IndexTemplateInFile);
    FileInfoOnLevelL.Template = GetPostfixTemplate(*Idx2, FileIndexBits, FileBits);
    FileInfoOnLevelL.IndexTemplate = GetPostfixTemplate(*Idx2, 0, FileIndexBits);
    FileInfoOnLevelL.NFiles3 = (ChunkInfoOnLevelL.NChunks3 + ChunkInfoOnLevelL.NChunksPerFile3 - 1) / ChunkInfoOnLevelL.NChunksPerFile3;

    /* compute file dir depths */
    array<i8>& FileDirDepthsOnLevelL = FileInfoOnLevelL.FileDirDepths;
    PushBack(&FileDirDepthsOnLevelL, i8(FileBits - BrickBits));
    i8 AccumulatedBits = FileBits - BrickBits;
    while (AccumulatedBits < BrickIndexBits)
    {
      i8 FileBitsPerDir = Min(i8(BrickIndexBits - AccumulatedBits), Idx2->FileBitsPerDir);
      AccumulatedBits += FileBitsPerDir;
      PushBack(&FileDirDepthsOnLevelL, FileBitsPerDir);
    }
    Reverse(Begin(FileDirDepthsOnLevelL), End(FileDirDepthsOnLevelL));
  }
}


/* TODO NEXT: this function needs revision */
static void
GuessNumLevelsIfNeeded(idx2_file* Idx2)
{
  //if (Idx2->NLevels == 0)
  //{
  //  v3i BrickDims3 = Idx2->BrickDims3;
  //  while (BrickDims3 <= Idx2->Dims3)
  //  {
  //    BrickDims3.X = BrickDims3.X == 1 ? BrickDims3.X : BrickDims3.X * 2;
  //    BrickDims3.Y = BrickDims3.Y == 1 ? BrickDims3.Y : BrickDims3.Y * 2;
  //    BrickDims3.Z = BrickDims3.Z == 1 ? BrickDims3.Z : BrickDims3.Z * 2;
  //    if (BrickDims3 <= Idx2->Dims3)
  //      ++Idx2->NLevels;
  //  }
  //}
}


file_chunk_brick_traversal::
file_chunk_brick_traversal(const idx2_file* Idx2,
                           const extent* Extent,
                           i8 Level,
                           traverse_callback* BrickCallback)
{
  this->Idx2 = Idx2;
  this->Extent = Extent;
  this->Level = Level;
  this->BrickCallback = BrickCallback;

  v3i B3, Bf3, Bl3, C3, Cf3, Cl3, F3, Ff3, Fl3; // Brick dimensions, brick first, brick last
  B3 = Idx2->BrickInfo[Level].Dims3Pow2;
  C3 = B3 * Idx2->BrickInfo[Level].NBricksPerChunk3;
  F3 = C3 * Idx2->ChunkInfo[Level].NChunksPerFile3;

  Bf3 = From(*Extent) / B3;
  Bl3 = Last(*Extent) / B3;
  Cf3 = From(*Extent) / C3;
  Cl3 = Last(*Extent) / C3;
  Ff3 = From(*Extent) / F3;
  Fl3 = Last(*Extent) / F3;

  this->ExtentInBricks = extent(Bf3, Bl3 - Bf3 + 1);
  this->ExtentInChunks = extent(Cf3, Cl3 - Cf3 + 1);
  this->ExtentInFiles  = extent(Ff3, Fl3 - Ff3 + 1);

  extent VolExt(Idx2->Dims3);
  v3i Vbf3, Vbl3, Vcf3, Vcl3, Vff3, Vfl3; // VolBrickFirst, VolBrickLast
  Vbf3 = From(VolExt) / B3;
  Vbl3 = Last(VolExt) / B3;
  Vcf3 = From(VolExt) / C3;
  Vcl3 = Last(VolExt) / C3;
  Vff3 = From(VolExt) / F3;
  Vfl3 = Last(VolExt) / F3;

  this->VolExtentInBricks = extent(Vbf3, Vbl3 - Vbf3 + 1);
  this->VolExtentInChunks = extent(Vcf3, Vcl3 - Vcf3 + 1);
  this->VolExtentInFiles  = extent(Vff3, Vfl3 - Vff3 + 1);
}


error<idx2_err_code>
TraverseBricks(const file_chunk_brick_traversal& Traversal, const traverse_item& ChunkTop)
{
  return Traversal.Traverse(Traversal.Idx2->BrickInfo[Traversal.Level].IndexTemplateInChunk,
                            ChunkTop.From3 * Traversal.Idx2->BrickInfo[Traversal.Level].NBricksPerChunk3,
                            Traversal.Idx2->BrickInfo[Traversal.Level].NBricksPerChunk3,
                            Traversal.ExtentInBricks,
                            Traversal.VolExtentInBricks,
                            Traversal.BrickCallback);
}

error<idx2_err_code>
TraverseChunks(const file_chunk_brick_traversal& Traversal, const traverse_item& FileTop)
{
  return Traversal.Traverse(Traversal.Idx2->ChunkInfo[Traversal.Level].IndexTemplateInFile,
                            FileTop.From3 * Traversal.Idx2->ChunkInfo[Traversal.Level].NChunksPerFile3,
                            Traversal.Idx2->ChunkInfo[Traversal.Level].NChunksPerFile3,
                            Traversal.ExtentInChunks,
                            Traversal.VolExtentInChunks,
                            TraverseBricks);
}

error<idx2_err_code>
TraverseFiles(const file_chunk_brick_traversal& Traversal)
{
  return Traversal.Traverse(Traversal.Idx2->FileInfo[Traversal.Level].IndexTemplate,
                            v3i(0),
                            Traversal.Idx2->FileInfo[Traversal.Level].NFiles3,
                            Traversal.ExtentInFiles,
                            Traversal.VolExtentInFiles,
                            TraverseChunks);
}


error<idx2_err_code>
Finalize(idx2_file* Idx2, params* P)
{
  P->Tolerance = Max(fabs(P->Tolerance), Idx2->Tolerance);
  GuessNumLevelsIfNeeded(Idx2);
  if (!(Idx2->NLevels <= MaxLevels))
    return idx2_Error(idx2_err_code::TooManyLevels, "Max # of levels = %d\n", MaxLevels);

  ProcessTransformTemplate(Idx2);
  BuildSubbandsForAllLevels(Idx2);
  ComputeSubbandMasks(Idx2, *P);

  ComputeBrickChunkFileInfo(Idx2, *P);

  // TODO NEXT
  //ComputeTransformDetails(&Idx2->TransformDetails, Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TransformOrder);

  return idx2_Error(idx2_err_code::NoError);
}

// TODO NEXT
void
Dealloc(idx2_file* Idx2)
{
  Dealloc(&Idx2->Subbands);
}


// TODO: handle the case where the query extent is larger than the domain itself
// TODO NEXT: look into this
grid
GetGrid(const idx2_file& Idx2, const extent& Ext)
{
  auto CroppedExt = Crop(Ext, extent(Idx2.Dims3));
  v3i Spacing3(1); // start with stride (1, 1, 1)
  idx2_For (int, D, 0, 3)
    Spacing3[D] <<= Idx2.DownsamplingFactor3[D];

  v3i First3 = From(CroppedExt);
  v3i Last3 = Last(CroppedExt);
  Last3 = ((Last3 + Spacing3 - 1) / Spacing3) * Spacing3; // move last to the right
  First3 = (First3 / Spacing3) * Spacing3; // move first to the left

  return grid(First3, (Last3 - First3) / Spacing3 + 1, Spacing3);
}


error<idx2_err_code>
file_chunk_brick_traversal::Traverse(stref Template,
                                     const v3i& From3, // in units of traverse_item
                                     const v3i& Dims3, // in units of traverse_item
                                     const extent& Extent,
                                     const extent& VolExtent, // in units of traverse_item
                                     traverse_callback* Callback) const
{
  idx2_RAII(array<traverse_item>, Stack, Reserve(&Stack, 64), Dealloc(&Stack));
  v3i Dims3PowOf2((int)NextPow2(Dims3.X), (int)NextPow2(Dims3.Y), (int)NextPow2(Dims3.Z));
  traverse_item Top;
  Top.From3 = From3;
  Top.To3 = From3 + Dims3PowOf2;
  Top.Pos = 0;
  PushBack(&Stack, Top);
  while (Size(Stack) >= 0)
  {
    Top = Back(Stack);
    i8 D = Idx2->DimensionMap[Template[Top.Pos]];
    PopBack(&Stack);
    if (!(Top.To3 - Top.From3 == 1))
    {
      // TODO NEXT: assert that the Pos is still iwthin template
      traverse_item First = Top, Second = Top;
      First.To3[D] =
        Top.From3[D] + (Top.To3[D] - Top.From3[D]) / 2;
      Second.From3[D] =
        Top.From3[D] + (Top.To3[D] - Top.From3[D]) / 2;
      extent Skip(First.From3, First.To3 - First.From3);
      //Second.NItemsBefore = First.NItemsBefore + Prod<u64>(Dims(Crop(Skip, Extent)));
      Second.ItemOrder = First.ItemOrder + Prod<i32>(Dims(Crop(Skip, VolExtent)));
      First.Address = Top.Address;
      Second.Address = Top.Address + Prod<u64>(First.To3 - First.From3);
      First.Pos = Second.Pos = Top.Pos + 1;
      if (Second.From3 < To(Extent) && From(Extent) < Second.To3)
        PushBack(&Stack, Second);
      if (First.From3 < To(Extent) && From(Extent) < First.To3)
        PushBack(&Stack, First);
    }
    else
    {
      Top.LastItem = Size(Stack) == 0;
      idx2_PropagateIfError(Callback(*this, Top));
    }
  }

  return idx2_Error(idx2_err_code::NoError);
}


} // namespace idx2

