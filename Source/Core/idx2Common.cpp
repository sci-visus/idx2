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


/*---------------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------------*/
void
Dealloc(params* P)
{
  // TODO NEXT
}


/*---------------------------------------------------------------------------------------------
Write the metadata (.idx2) file to disk.
---------------------------------------------------------------------------------------------*/
/* Write the metadata file (idx) */
// TODO: return error type
// TODO NEXT
void
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName)
{

}


/*---------------------------------------------------------------------------------------------
Parse metadata from a given buffer.
---------------------------------------------------------------------------------------------*/
// TODO NEXT
error<idx2_err_code>
ReadMetaFileFromBuffer(idx2_file* Idx2, buffer& Buf)
{ return idx2_Error(idx2_err_code::NoError); }


/*---------------------------------------------------------------------------------------------
Read the given metadata (.idx2) file.
---------------------------------------------------------------------------------------------*/
error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName)
{
  buffer Buf;
  idx2_CleanUp(DeallocBuf(&Buf));
  idx2_PropagateIfError(ReadFile(FileName, &Buf));
  return ReadMetaFileFromBuffer(Idx2, Buf);
}


/*---------------------------------------------------------------------------------------------
Return dimensions corresponding to a template.
---------------------------------------------------------------------------------------------*/
v3i
GetDimsFromTemplate(const idx2_file& Idx2, stref Template)
{
  v3i Dims3(1);
  idx2_For (i8, I, 0, Template.Size)
  {
    idx2_Assert(isalpha(Template[I]) && islower(Template[I]));
    i8 D = Idx2.DimensionMap[Template[I] - 'x'];
    Dims3[D] *= 2;
  }
  return Dims3;
}


/*---------------------------------------------------------------------------------------------
Return part of the postfix template from the beginning until a given position.
---------------------------------------------------------------------------------------------*/
idx2_Inline static stref
GetPostfixTemplate(const idx2_file& Idx2, i8 Pos)
{
  return stref(Idx2.Template.Postfix.Data + Pos, Idx2.Template.Postfix.Len - Pos);
}


/*---------------------------------------------------------------------------------------------
Return part of the postfix template corresponding to a given position and size.
---------------------------------------------------------------------------------------------*/
idx2_Inline static stref
GetPostfixTemplate(const idx2_file& Idx2, i8 Pos, i8 Size)
{
  return stref(Idx2.Template.Postfix.Data + Pos, Size);
}


/*---------------------------------------------------------------------------------------------
Return part of the template corresponding to a given level.
---------------------------------------------------------------------------------------------*/
idx2_Inline static stref
GetLevelTemplate(const idx2_file& Idx2, i8 Level)
{
  i8 Pos = Idx2.Template.LevelParts[Level][0];
  i8 Size = Idx2.Template.LevelParts[Level][1];
  return GetPostfixTemplate(Idx2, Pos, Size);
}


/*---------------------------------------------------------------------------------------------
Break a template represented as a string into parts that can be better interpreted.
---------------------------------------------------------------------------------------------*/
// TODO NEXT: return an error (the template may be invalid)
/* The full template can be
zzzyy:xyz:xyz:xy (zzzyy is the prefix), or
:xyz:xyz:zzz:yyy (there is no prefix)
*/
static void
ProcessTransformTemplate(idx2_file* Idx2)
{
  // TODO NEXT: check the syntax of the template
  transform_template& Template = Idx2->Template;
  tokenizer Tokenizer(Template.Full.ConstPtr, ":");

  /* parse the prefix (if any) */
  stref Part = Next(&Tokenizer); // the prefix if it exists, else the first part of the postfix
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


/*---------------------------------------------------------------------------------------------
Check if the transform template is valid.
---------------------------------------------------------------------------------------------*/
static error<idx2_err_code>
VerifyTransformTemplate(const idx2_file& Idx2)
{
  /* check for unused dimensions */
  for (auto I = 0; I < Size(Idx2.Dimensions); ++I)
  {
    char C = Idx2.Dimensions[I].ShortName;
    if (Idx2.DimensionMap[C - 'a'] == -1)
      return idx2_Error(idx2_err_code::DimensionsTooMany,
                        "Dimension %c does not appear in the indexing template\n", C);
  }

  /* check for repeated dimensions on a level */
  for (auto L = 0; L < Size(Idx2.Template.LevelParts); ++L)
  {
    auto Length = Idx2.Template.LevelParts[L][1];
    if (Length > 3)
      return idx2_Error(idx2_err_code::DimensionsTooMany,
                        "More than three dimensions in level %d\n", L);
    if (Length == 0)
      return idx2_Error(idx2_err_code::SyntaxError,
                        ": or | needs to be followed by a dimension in the indexing template\n");
    const auto Part = GetLevelTemplate(Idx2, L);
    if (Length == 2 && (Part[0] == Part[1]))
      return idx2_Error(idx2_err_code::DimensionsRepeated,
                        "Repeated dimensions on level %d\n", L);
      //printf("Repeated dimensions on level %d\n", L);
    if (Length == 3 && (Part[0] == Part[1] || Part[0] == Part[2] || Part[1] == Part[2]))
      return idx2_Error(idx2_err_code::DimensionsRepeated,
                        "Repeated dimensions on level %d\n", L);
      //printf("Repeated dimensions on level %d\n", L);
  }

  /* check that the indexing template agrees with the dimensions */
  nd_size Dims(1);
  for (auto I = 0; I < Size(Idx2.Template.Full); ++I)
  {
    char C = Idx2.Template.Full[I];
    if (!isalpha(C))
      continue;
    i8 D = Idx2.DimensionMap[C - 'a'];
    Dims[D] *= 2;
  }
  for (auto I = 0; I < Size(Idx2.Dimensions); ++I)
  {
    const dimension_info& Dim = Idx2.Dimensions[I];
    i32 D = (i32)NextPow2(Size(Dim));
    if (D > Dims[I])
      return idx2_Error(idx2_err_code::DimensionMismatched,
                        "Dimension %c needs to appear %d more times in the indexing template\n",
                        Dim.ShortName,
                        Log2Floor(D) - Log2Floor(Dims[I]));
    if (D < Dims[I])
      return idx2_Error(idx2_err_code::DimensionMismatched,
                        "Dimension %c needs to appear %d fewer times in the indexing template\n",
                        Dim.ShortName,
                        Log2Floor(Dims[I]) - Log2Floor(D));
  }

  return idx2_Error(idx2_err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Compute a list of subbands with necessary details for each level.
---------------------------------------------------------------------------------------------*/
static void
BuildSubbandsForAllLevels(idx2_file* Idx2)
{
  idx2_For (i8, L, 0, Idx2->NLevels)
  {
    stref TemplateL = GetLevelTemplate(*Idx2, L);
    subbands_per_level SubbandsL;
    const v3i& Dims3 = Idx2->BrickInfo[L].Dims3Pow2;
    const v3i& Spacing3 = Idx2->BrickInfo[L].Spacing3;
    // TODO NEXT: do we need both versions?
    SubbandsL.PowOf2 = BuildLevelSubbands(TemplateL, Idx2->DimensionMap, Dims3, Spacing3);
    SubbandsL.PowOf2Plus1 = BuildLevelSubbands(TemplateL, Idx2->DimensionMap, idx2_ExtDims(Dims3), Spacing3);
    PushBack(&Idx2->Subbands, SubbandsL);
  }
}


/*---------------------------------------------------------------------------------------------
Compute a mask for each level, where each bit determines whether the corresponding subbands
should be decoded, based on a given DownsamplingFactor.
---------------------------------------------------------------------------------------------*/
static void
ComputeSubbandMasks(idx2_file* Idx2, const params& P)
{
  v3i Spacing3 = v3i(1) << P.DownsamplingFactor3;
  idx2_For (int, L, 0, Idx2->NLevels)
  {
    subbands_per_level& SubbandsL = Idx2->Subbands[L];
    u8 Mask = 0xFF;
    idx2_For (i8, Sb, 0, Size(SubbandsL.PowOf2Plus1))
    {
      v3i F3 = From(SubbandsL.PowOf2Plus1[Sb].GlobalGrid);
      v3i S3 = Spacing(SubbandsL.PowOf2Plus1[Sb].GlobalGrid);
      SubbandsL.Spacings[Sb] = v3i(1);
      for (int D = 0; D < 3; ++D)
      {
        // check if a grid1 that starts at F3 with spacing S3 is a subgrid
        // of a grid2 that starts at 0 with spacing Spacing3
        if ((F3[D] % Spacing3[D]) == 0)
        {
          if ((S3[D] % Spacing3[D]) != 0)
            SubbandsL.Spacings[Sb][D] = Spacing3[D] / S3[D];
        }
        else // skip the subband
        {
          Mask = UnsetBit(Mask, Sb);
        }
      }
    }
    if (L + 1 < Idx2->NLevels && Mask == 1)
      Mask = 0; // explicitly disable subband 0 if it is the only subband to decode
    SubbandsL.DecodeMasks = Mask;
  }
}


/*---------------------------------------------------------------------------------------------
Compute templates corresponding to bricks/chunks/files on each level.
---------------------------------------------------------------------------------------------*/
// TODO NEXT: check the values of BrickBitsPerChunk etc to make sure they are under the limits
static void
ComputeBrickChunkFileInfo(idx2_file* Idx2, const params& P)
{
  Resize(&Idx2->BrickInfo, Idx2->NLevels);
  Resize(&Idx2->ChunkInfo, Idx2->NLevels);
  Resize(&Idx2->FileInfo, Idx2->NLevels);

  i8 BrickBits, ChunkBits, FileBits;
  i8 BrickIndexBits, ChunkIndexBits, FileIndexBits;
  i8 Length;
  v3i NBricks3;
  idx2_For (i8, L, 0, Idx2->NLevels)
  {
    brick_info_per_level& BrickInfoL = Idx2->BrickInfo[L];
    chunk_info_per_level& ChunkInfoL = Idx2->ChunkInfo[L];
    file_info_per_level& FileInfoL = Idx2->FileInfo[L];

    /* compute Group3 */
    {
      stref Template = GetLevelTemplate(*Idx2, L);
      BrickInfoL.Group3 = GetDimsFromTemplate(*Idx2, Template);
    }

    /* compute brick templates */
    {
      Length = Sum<i8>(Idx2->Template.LevelParts[L]);
      BrickInfoL.Template = GetLevelTemplate(*Idx2, L);
      BrickBits = Min(Length, Idx2->BitsPerBrick);
      BrickIndexBits = Length - BrickBits;
      BrickInfoL.IndexTemplate = GetPostfixTemplate(*Idx2, 0, BrickIndexBits);
      BrickInfoL.Template = GetPostfixTemplate(*Idx2, BrickIndexBits, BrickBits);
      BrickInfoL.Dims3Pow2 = GetDimsFromTemplate(*Idx2, BrickInfoL.Template);
      BrickInfoL.Spacing3 = GetDimsFromTemplate(*Idx2, GetPostfixTemplate(*Idx2, Length));
      if (L == 0)
        NBricks3 = (Idx2->Dims3 + BrickInfoL.Dims3Pow2 - 1) / BrickInfoL.Dims3Pow2;
      else
        NBricks3 = (NBricks3 + BrickInfoL.Group3 - 1) / BrickInfoL.Group3;
      BrickInfoL.NBricks3 = NBricks3;
    }

    /* compute chunk templates */
    {
      ChunkBits = Min(Length, i8(Idx2->BitsPerBrick + Idx2->BrickBitsPerChunk));
      ChunkIndexBits = Length - ChunkBits;
      BrickInfoL.IndexTemplateInChunk = GetPostfixTemplate(*Idx2, ChunkIndexBits, BrickIndexBits - ChunkIndexBits);
      BrickInfoL.NBricksPerChunk3 = GetDimsFromTemplate(*Idx2, BrickInfoL.IndexTemplateInChunk);
      ChunkInfoL.Template = GetPostfixTemplate(*Idx2, ChunkIndexBits, ChunkBits);
      ChunkInfoL.IndexTemplate = GetPostfixTemplate(*Idx2, 0, ChunkIndexBits);
      ChunkInfoL.NChunks3 = (BrickInfoL.NBricks3 + BrickInfoL.NBricksPerChunk3 - 1) / BrickInfoL.NBricksPerChunk3;
    }

    /* compute file templates */
    {
      FileBits = Min(Length, i8(Idx2->BitsPerBrick + Idx2->BrickBitsPerChunk + Idx2->ChunkBitsPerFile));
      FileIndexBits = Length - FileBits;
      ChunkInfoL.IndexTemplateInFile = GetPostfixTemplate(*Idx2, FileIndexBits, ChunkIndexBits - FileIndexBits);
      ChunkInfoL.NChunksPerFile3 = GetDimsFromTemplate(*Idx2, ChunkInfoL.IndexTemplateInFile);
      FileInfoL.Template = GetPostfixTemplate(*Idx2, FileIndexBits, FileBits);
      FileInfoL.IndexTemplate = GetPostfixTemplate(*Idx2, 0, FileIndexBits);
      FileInfoL.NFiles3 = (ChunkInfoL.NChunks3 + ChunkInfoL.NChunksPerFile3 - 1) / ChunkInfoL.NChunksPerFile3;
    }

    /* compute file dir depths */
    {
      array<i8>& FileDirDepthsL = FileInfoL.FileDirDepths;
      PushBack(&FileDirDepthsL, i8(FileBits - BrickBits));
      i8 AccumulatedBits = FileBits - BrickBits;
      while (AccumulatedBits < BrickIndexBits)
      {
        i8 FileBitsPerDir = Min(i8(BrickIndexBits - AccumulatedBits), Idx2->FileBitsPerDir);
        AccumulatedBits += FileBitsPerDir;
        PushBack(&FileDirDepthsL, FileBitsPerDir);
      }
      Reverse(Begin(FileDirDepthsL), End(FileDirDepthsL));
    }
  }
}


/*---------------------------------------------------------------------------------------------
Guess the transform template.
---------------------------------------------------------------------------------------------*/
stack_array<u8, MaxTemplateLength>
GuessTransformTemplate(const idx2_file& Idx2, template_hint Hint)
{
  stack_array<u8, MaxTemplateLength> Template;
  v3i DimsLog = Log2Ceil(Idx2.Dims3);
  i8 Level = 0;
  i8 Pos = 0;
  while (Sum<i32>(DimsLog) != 0)
  {
    bool Used[3] = {};
    i32 DimMax = 0;
    i8 DMax = -1;
    while (true) // loop for one level
    {
      DMax = -1;
      if (Hint == template_hint::Anisotropic)
      {
        for (i8 D = 0; D < 3; ++D)
        {
          if (DimsLog[D] > DimMax && !Used[D])
          {
            DMax = D;
            DimMax = DimsLog[D];
          }
        }
      }
      else if (Hint == template_hint::Isotropic)
      {
        for (i8 D = 2; D >= 0; --D)
        {
          if (DimsLog[D] > 0 && !Used[D])
          {
            DMax = D;
            DimMax = DimsLog[D];
          }
        }
      }
      else
      {
        idx2_Assert(false);
      }

      if (DMax != -1) // found a dimension to continue the current level
      {
        Used[DMax] = true;
        --DimsLog[DMax];
        --DimMax;
        Template[Pos++] = 'x' + DMax; // TODO NEXT: we need to translate from number to char
      }
      else // done with the current level
      {
        if (Sum<i32>(DimsLog) >= 21) // so that the coarsest level is at least 128^3-equivalent
          Template[Pos++] = ':';
        break;
      }
    }
  }

  idx2_For (i8, I, 0, Pos/2)
    Swap(&Template[I], &Template[Pos - I - 1]);

  return Template;
}


/*---------------------------------------------------------------------------------------------
Compute auxiliary information to be used during hierarchy traversals.
---------------------------------------------------------------------------------------------*/
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


/*---------------------------------------------------------------------------------------------
Traverse a hierarchy of bricks following a template, and run a callback function for each brick.
---------------------------------------------------------------------------------------------*/
error<idx2_err_code>
TraverseBricks(const file_chunk_brick_traversal& T, const traverse_item& ChunkTop)
{
  return T.Traverse(T.Idx2->BrickInfo[T.Level].IndexTemplateInChunk,
                    ChunkTop.From3 * T.Idx2->BrickInfo[T.Level].NBricksPerChunk3,
                    T.Idx2->BrickInfo[T.Level].NBricksPerChunk3,
                    T.ExtentInBricks,
                    T.VolExtentInBricks,
                    T.BrickCallback);
}


/*---------------------------------------------------------------------------------------------
Traverse a hierarchy of chunks following a template, and run a callback function for each chunk.
---------------------------------------------------------------------------------------------*/
error<idx2_err_code>
TraverseChunks(const file_chunk_brick_traversal& T, const traverse_item& FileTop)
{
  return T.Traverse(T.Idx2->ChunkInfo[T.Level].IndexTemplateInFile,
                    FileTop.From3 * T.Idx2->ChunkInfo[T.Level].NChunksPerFile3,
                    T.Idx2->ChunkInfo[T.Level].NChunksPerFile3,
                    T.ExtentInChunks,
                    T.VolExtentInChunks,
                    TraverseBricks);
}


/*---------------------------------------------------------------------------------------------
Traverse a hierarchy of files following a template, and run a callback function for each file.
---------------------------------------------------------------------------------------------*/
error<idx2_err_code>
TraverseFiles(const file_chunk_brick_traversal& T)
{
  return T.Traverse(T.Idx2->FileInfo[T.Level].IndexTemplate,
                    v3i(0),
                    T.Idx2->FileInfo[T.Level].NFiles3,
                    T.ExtentInFiles,
                    T.VolExtentInFiles,
                    TraverseChunks);
}


/*---------------------------------------------------------------------------------------------
Verify the integrity of the parameters in idx2_file and compute necessary auxiliary data
structures for use during encoding or decoding.
---------------------------------------------------------------------------------------------*/
error<idx2_err_code>
Finalize(idx2_file* Idx2, params* P)
{
  P->Tolerance = Max(fabs(P->Tolerance), Idx2->Tolerance);
  //GuessNumLevelsIfNeeded(Idx2); // TODO NEXT
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


/*---------------------------------------------------------------------------------------------
Deallocate the main idx2_file struct.
---------------------------------------------------------------------------------------------*/
// TODO NEXT
void
Dealloc(idx2_file* Idx2)
{
  Dealloc(&Idx2->Subbands);
}


/*---------------------------------------------------------------------------------------------
Return a grid given a query extent (box) and a downsampling factor.
---------------------------------------------------------------------------------------------*/
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


/*---------------------------------------------------------------------------------------------
A generic template for hierarchy traversal following a template.
---------------------------------------------------------------------------------------------*/
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
    i8 D = Idx2->DimensionMap[Template[Top.Pos] - 'a'];
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

