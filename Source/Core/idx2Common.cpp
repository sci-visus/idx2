#include "idx2Common.h"
#include "nd_volume.h"
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


idx2_file::idx2_file()
{
  memset(DimensionMap.Arr, -1, DimensionMap.Capacity());
}


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
error<err_code>
ReadMetaFileFromBuffer(idx2_file* Idx2, buffer& Buf)
{ return idx2_Error(err_code::NoError); }


/*---------------------------------------------------------------------------------------------
Read the given metadata (.idx2) file.
---------------------------------------------------------------------------------------------*/
error<err_code>
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
nd_size
Dims(const template_view& Template)
{
  nd_size Dims(1);
  idx2_For (i8, I, 0, Template.Size)
  {
    i8 J = I + Template.Begin;
    i8 D = Template[J];
    Dims[D] *= 2;
  }
  return Dims;
}


/*---------------------------------------------------------------------------------------------
Break a template represented as a string into parts that can be better interpreted.
The full template can be
zzzyy:xyz:xyz:xy (zzzyy is the prefix), or
:xyz:xyz:zzz:yyy (there is no prefix)
---------------------------------------------------------------------------------------------*/
// TODO NEXT: return an error (the template may be invalid)
static void
ProcessTransformTemplate(idx2_file* Idx2)
{
  // TODO NEXT: check the syntax of the template
  tokenizer Tokenizer(Idx2->Template.Original.Data, ":");

  /* parse the prefix (if any) */
  stref Part = Next(&Tokenizer); // the prefix if it exists, else the first part of the postfix
  i8 Pos = i8(Part.ConstPtr - Idx2->Template.Original.Data);
  i8 DstPos = 0;
  if (Pos == 0) // there is a prefix
  {
    i8 Size = Part.Size;
    idx2_For (i8, I, 0, Size)
      Idx2->Template.Processed[I] = Idx2->DimensionMap[Idx2->Template.Original[I] - 'a'];
    DstPos += Size;
  }
  else // there is no prefix, reset to parse from the beginning
  {
    Reset(&Tokenizer);
  }

  /* parse the postfix */
  while (stref Part = Next(&Tokenizer))
  {
    i8 Pos = i8(Part.ConstPtr - Idx2->Template.Original.Data);
    i8 Size = Part.Size;
    idx2_For (i8, I, 0, Size)
      Idx2->Template.Processed[DstPos + I] = Idx2->DimensionMap[Idx2->Template.Original[Pos + I] - 'a'];
    PushBack(&Idx2->Template.LevelViews, template_view{&Idx2->Template.Processed, DstPos, Size});
    DstPos += Size;
  }

  /* reverse so that the LevelParts start from level 0 */
  Reverse(idx2_Range(Idx2->Template.LevelViews));
}


/*---------------------------------------------------------------------------------------------
Check if the transform template is valid.
---------------------------------------------------------------------------------------------*/
static error<err_code>
VerifyTransformTemplate(const idx2_file& Idx2)
{
  /* check for unused dimensions */
  for (auto I = 0; I < Size(Idx2.DimensionInfo); ++I)
  {
    char C = Idx2.DimensionInfo[I].ShortName;
    if (Idx2.DimensionMap[C - 'a'] == -1)
      return idx2_Error(err_code::DimensionsTooMany,
                        "Dimension %c does not appear in the indexing template\n", C);
  }

  /* check for repeated dimensions on a level */
  for (auto L = 0; L < Size(Idx2.Template.LevelViews); ++L)
  {
    auto Length = Idx2.Template.LevelViews[L][1];
    if (Length > 3)
      return idx2_Error(err_code::DimensionsTooMany,
                        "More than three dimensions in level %d\n", L);
    if (Length == 0)
      return idx2_Error(err_code::SyntaxError,
                        ": or | needs to be followed by a dimension in the indexing template\n");
    const template_view& View = Idx2.Template.LevelViews[L];
    if (Length == 2 && (View[0] == View[1]))
      return idx2_Error(err_code::DimensionsRepeated,
                        "Repeated dimensions on level %d\n", L);
      //printf("Repeated dimensions on level %d\n", L);
    if (Length == 3 && (View[0] == View[1] || View[0] == View[2] || View[1] == View[2]))
      return idx2_Error(err_code::DimensionsRepeated,
                        "Repeated dimensions on level %d\n", L);
      //printf("Repeated dimensions on level %d\n", L);
  }

  /* check that the indexing template agrees with the dimensions */
  nd_size Dims(1);
  for (auto I = 0; I < Size(Idx2.Template.Original); ++I)
  {
    char C = Idx2.Template.Original[I];
    if (!isalpha(C))
      continue;
    i8 D = Idx2.DimensionMap[C - 'a'];
    Dims[D] *= 2;
  }
  for (auto I = 0; I < Size(Idx2.DimensionInfo); ++I)
  {
    const dimension_info& Dim = Idx2.DimensionInfo[I];
    i32 D = (i32)NextPow2(Size(Dim));
    if (D > Dims[I])
      return idx2_Error(err_code::DimensionMismatched,
                        "Dimension %c needs to appear %d more times in the indexing template\n",
                        Dim.ShortName,
                        Log2Floor(D) - Log2Floor(Dims[I]));
    if (D < Dims[I])
      return idx2_Error(err_code::DimensionMismatched,
                        "Dimension %c needs to appear %d fewer times in the indexing template\n",
                        Dim.ShortName,
                        Log2Floor(Dims[I]) - Log2Floor(D));
  }

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Compute a list of subbands with necessary details for each level.
---------------------------------------------------------------------------------------------*/
static void
ComputeSubbandsForAllLevels(idx2_file* Idx2)
{
  const i8 NLevels = (i8)Size(Idx2->Template.LevelViews);
  idx2_For (i8, L, 0, NLevels)
  {
    const template_view& TemplateL = Idx2->Template.LevelViews[L];
    subbands_per_level SubbandsL;
    const nd_size& DimsPow2 = Idx2->BrickIndexing[L].DimsPow2;
    const nd_size& DimsPow2Plus1 = Idx2->BrickIndexing[L].DimsPow2Plus1;
    const nd_size& Spacing = Idx2->BrickIndexing[L].Spacing;
    // TODO NEXT: do we need both versions?
    SubbandsL.Pow2 = BuildLevelSubbands(TemplateL, DimsPow2, Spacing);
    SubbandsL.Pow2Plus1 = BuildLevelSubbands(TemplateL, DimsPow2Plus1, Spacing);
    PushBack(&Idx2->Subbands, SubbandsL);
  }
}


/*---------------------------------------------------------------------------------------------
Compute a mask for each level, where each bit determines whether the corresponding subbands
should be decoded, based on a given DownsamplingFactor.
---------------------------------------------------------------------------------------------*/
static void
ComputeSubbandMasks(idx2_file* Idx2, const nd_size& Downsampling)
{
  nd_size DesiredSpacing = nd_size(1) << Downsampling;
  const i8 NLevels = (i8)Size(Idx2->Template.LevelViews);
  idx2_For (i8, L, 0, NLevels)
  {
    subbands_per_level& SubbandsL = Idx2->Subbands[L];
    u8 Mask = 0xFF;
    idx2_For (i8, Sb, 0, Size(SubbandsL.Pow2Plus1))
    {
      nd_size From = idx2::From(SubbandsL.Pow2Plus1[Sb].GlobalGrid);
      nd_size Spacing = idx2::Spacing(SubbandsL.Pow2Plus1[Sb].GlobalGrid);
      SubbandsL.Spacings[Sb] = nd_size(1);
      for (int D = 0; D < 3; ++D)
      {
        // check if the (From, Spacing) grid is a subgrid of the (0, DesiredSpacing) grid
        if ((From[D] % DesiredSpacing[D]) == 0)
        {
          if ((Spacing[D] % DesiredSpacing[D]) != 0)
            SubbandsL.Spacings[Sb][D] = DesiredSpacing[D] / Spacing[D];
        }
        else // skip the subband
        {
          Mask = UnsetBit(Mask, Sb);
        }
      }
    }
    if (L + 1 < NLevels && Mask == 1)
      Mask = 0; // explicitly disable subband 0 if it is the only subband to decode
    SubbandsL.DecodeMasks = Mask;
  }
}


/*---------------------------------------------------------------------------------------------
Compute templates corresponding to bricks/chunks/files on each level.
---------------------------------------------------------------------------------------------*/
// TODO NEXT: check the values of BrickBitsPerChunk etc to make sure they are under the limits
static void
ComputeIndexingInfo(idx2_file* Idx2)
{
  const template_int& Template = Idx2->Template.Processed;

  const i8 NLevels = (i8)Size(Idx2->Template.LevelViews);

  Resize(&Idx2->BrickIndexing, NLevels);
  Resize(&Idx2->ChunkIndexing, NLevels);
  Resize(&Idx2->FileIndexing, NLevels);

  i8 BrickBits, ChunkBits, FileBits;
  i8 BrickIndexBits, ChunkIndexBits, FileIndexBits;
  i8 Length;
  nd_size NBricks;
  idx2_For (i8, L, 0, NLevels)
  {
    brick_indexing_per_level& BrickIndexingL = Idx2->BrickIndexing[L];
    chunk_indexing_per_level& ChunkIndexingL = Idx2->ChunkIndexing[L];
    file_indexing_per_level& FileIndexingL = Idx2->FileIndexing[L];

    /* compute Group3 */
    {
      BrickIndexingL.Group = Dims(Idx2->Template.LevelViews[L]);
    }

    /* compute brick templates */
    {
      Length = Sum(Idx2->Template.LevelViews[L]);
      BrickBits = Min(Length, Idx2->BitsPerBrick);
      BrickIndexBits = Length - BrickBits;
      BrickIndexingL.IndexTemplate = template_view{&Template, 0, BrickIndexBits};
      BrickIndexingL.Template = template_view{&Template, BrickIndexBits, BrickBits};
      BrickIndexingL.DimsPow2 = Dims(BrickIndexingL.Template);
      BrickIndexingL.Spacing = Dims(template_view{&Template, 0, Length});
      if (L == 0)
        NBricks = (Idx2->Dims + BrickIndexingL.DimsPow2 - 1) / BrickIndexingL.DimsPow2;
      else
        NBricks = (NBricks + BrickIndexingL.Group - 1) / BrickIndexingL.Group;
      BrickIndexingL.NBricks = NBricks;
    }

    /* compute chunk templates */
    {
      ChunkBits = Min(Length, i8(Idx2->BitsPerBrick + Idx2->BrickBitsPerChunk));
      ChunkIndexBits = Length - ChunkBits;
      BrickIndexingL.IndexTemplateInChunk = template_view{&Template, ChunkIndexBits, i8(BrickIndexBits - ChunkIndexBits)};
      BrickIndexingL.NBricksPerChunk = Dims(BrickIndexingL.IndexTemplateInChunk);
      ChunkIndexingL.Template = template_view{&Template, ChunkIndexBits, ChunkBits};
      ChunkIndexingL.IndexTemplate = template_view{&Template, 0, ChunkIndexBits};
      ChunkIndexingL.NChunks = (BrickIndexingL.NBricks + BrickIndexingL.NBricksPerChunk - 1) / BrickIndexingL.NBricksPerChunk;
    }

    /* compute file templates */
    {
      FileBits = Min(Length, i8(Idx2->BitsPerBrick + Idx2->BrickBitsPerChunk + Idx2->ChunkBitsPerFile));
      FileIndexBits = Length - FileBits;
      ChunkIndexingL.IndexTemplateInFile = template_view{&Template, FileIndexBits, i8(ChunkIndexBits - FileIndexBits)};
      ChunkIndexingL.NChunksPerFile = Dims(ChunkIndexingL.IndexTemplateInFile);
      FileIndexingL.Template = template_view{&Template, FileIndexBits, FileBits};
      FileIndexingL.IndexTemplate = template_view{&Template, 0, FileIndexBits};
      FileIndexingL.NFiles = (ChunkIndexingL.NChunks + ChunkIndexingL.NChunksPerFile - 1) / ChunkIndexingL.NChunksPerFile;
    }

    /* compute file dir depths */
    {
      auto& FileDirDepthsL = FileIndexingL.FileDirDepths;
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
template_str
GuessTransformTemplate(const idx2_file& Idx2, template_hint Hint)
{
  // TODO NEXT: 'f' is not used?
  template_str Template;
  nd_size DimsLog = Log2Ceil(Idx2.Dims);
  i8 Level = 0;
  i8 Pos = 0;
  i8 NDims = (i8)Size(Idx2.DimensionInfo);
  while (Sum<i32>(DimsLog) != 0)
  {
    bool Used[nd_size::Size()] = {};
    i32 DimMax = 0;
    i8 DMax = -1;
    while (true) // loop for one level
    {
      DMax = -1;
      if (Hint == template_hint::Anisotropic)
      {
        for (i8 D = 0; D < NDims; ++D)
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
        for (i8 D = NDims - 1; D >= 0; --D)
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
        Template[Pos++] = Idx2.DimensionMapInverse[DMax];
      }
      else // done with the current level
      {
        // TODO NEXT: how about 2D?
        if (Sum<i32>(DimsLog) >= 21) // so that the coarsest level is at least 128^3-equivalent
          Template[Pos++] = ':';
        break;
      }
    }
  }

  idx2_For (i8, I, 0, Pos/2)
    Swap(&Template[I], &Template[Pos - I - 1]);
  Template[Pos] = 0;
  Template.Size = Pos;

  return Template;
}


/*---------------------------------------------------------------------------------------------
Compute auxiliary information to be used during hierarchy traversals.
---------------------------------------------------------------------------------------------*/
file_chunk_brick_traversal::
file_chunk_brick_traversal(const idx2_file* Idx2,
                           const nd_extent* Extent,
                           i8 Level,
                           traverse_callback* BrickCallback)
{
  this->Idx2 = Idx2;
  this->Extent = Extent;
  this->Level = Level;
  this->BrickCallback = BrickCallback;

  nd_size B3, Bf3, Bl3, C3, Cf3, Cl3, F3, Ff3, Fl3; // Brick dimensions, brick first, brick last
  B3 = Idx2->BrickIndexing[Level].DimsPow2;
  C3 = B3 * Idx2->BrickIndexing[Level].NBricksPerChunk;
  F3 = C3 * Idx2->ChunkIndexing[Level].NChunksPerFile;

  Bf3 = Extent->From / B3;
  Bl3 = Last(*Extent) / B3;
  Cf3 = Extent->From / C3;
  Cl3 = Last(*Extent) / C3;
  Ff3 = Extent->From / F3;
  Fl3 = Last(*Extent) / F3;

  this->ExtentInBricks = nd_extent(Bf3, Bl3 - Bf3 + 1);
  this->ExtentInChunks = nd_extent(Cf3, Cl3 - Cf3 + 1);
  this->ExtentInFiles  = nd_extent(Ff3, Fl3 - Ff3 + 1);

  nd_extent VolExt(Idx2->Dims);
  nd_size Vbf3, Vbl3, Vcf3, Vcl3, Vff3, Vfl3; // VolBrickFirst, VolBrickLast
  Vbf3 = VolExt.From / B3;
  Vbl3 = Last(VolExt) / B3;
  Vcf3 = VolExt.From / C3;
  Vcl3 = Last(VolExt) / C3;
  Vff3 = VolExt.From / F3;
  Vfl3 = Last(VolExt) / F3;

  this->VolExtentInBricks = nd_extent(Vbf3, Vbl3 - Vbf3 + 1);
  this->VolExtentInChunks = nd_extent(Vcf3, Vcl3 - Vcf3 + 1);
  this->VolExtentInFiles  = nd_extent(Vff3, Vfl3 - Vff3 + 1);
}


/*---------------------------------------------------------------------------------------------
Traverse a hierarchy of bricks following a template, and run a callback function for each brick.
---------------------------------------------------------------------------------------------*/
error<err_code>
TraverseBricks(const file_chunk_brick_traversal& T, const traverse_item& ChunkTop)
{
  return T.Traverse(T.Idx2->BrickIndexing[T.Level].IndexTemplateInChunk,
                    ChunkTop.From * T.Idx2->BrickIndexing[T.Level].NBricksPerChunk,
                    T.Idx2->BrickIndexing[T.Level].NBricksPerChunk,
                    T.ExtentInBricks,
                    T.VolExtentInBricks,
                    T.BrickCallback);
}


/*---------------------------------------------------------------------------------------------
Traverse a hierarchy of chunks following a template, and run a callback function for each chunk.
---------------------------------------------------------------------------------------------*/
error<err_code>
TraverseChunks(const file_chunk_brick_traversal& T, const traverse_item& FileTop)
{
  return T.Traverse(T.Idx2->ChunkIndexing[T.Level].IndexTemplateInFile,
                    FileTop.From * T.Idx2->ChunkIndexing[T.Level].NChunksPerFile,
                    T.Idx2->ChunkIndexing[T.Level].NChunksPerFile,
                    T.ExtentInChunks,
                    T.VolExtentInChunks,
                    TraverseBricks);
}


/*---------------------------------------------------------------------------------------------
Traverse a hierarchy of files following a template, and run a callback function for each file.
---------------------------------------------------------------------------------------------*/
error<err_code>
TraverseFiles(const file_chunk_brick_traversal& T)
{
  return T.Traverse(T.Idx2->FileIndexing[T.Level].IndexTemplate,
                    nd_size(0),
                    T.Idx2->FileIndexing[T.Level].NFiles,
                    T.ExtentInFiles,
                    T.VolExtentInFiles,
                    TraverseChunks);
}


/*---------------------------------------------------------------------------------------------
Verify the integrity of the parameters in idx2_file and compute necessary auxiliary data
structures for use during encoding or decoding.
---------------------------------------------------------------------------------------------*/
error<err_code>
Finalize(idx2_file* Idx2)
{
  ProcessTransformTemplate(Idx2);
  VerifyTransformTemplate(*Idx2);
  ComputeSubbandsForAllLevels(Idx2);
  ComputeIndexingInfo(Idx2);
  // TODO NEXT: compute SubbandMasks
  //P->Tolerance = Max(fabs(P->Tolerance), Idx2->Tolerance);
  ////GuessNumLevelsIfNeeded(Idx2); // TODO NEXT
  //if (!(Idx2->NLevels <= MaxLevels))
  //  return idx2_Error(err_code::TooManyLevels, "Max # of levels = %d\n", MaxLevels);

  //ProcessTransformTemplate(Idx2);
  //BuildSubbandsForAllLevels(Idx2);
  //ComputeSubbandMasks(Idx2, *P);

  //ComputeBrickChunkFileInfo(Idx2, *P);

  // TODO NEXT
  //ComputeTransformDetails(&Idx2->TransformDetails, Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TransformOrder);

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Return a grid given a query extent (box) and a downsampling factor.
---------------------------------------------------------------------------------------------*/
// TODO: handle the case where the query extent is larger than the domain itself
// TODO NEXT: look into this
//grid
//GetGrid(const idx2_file& Idx2, const nd_extent& Ext)
//{
//  auto CroppedExt = Crop(Ext, nd_extent(Idx2.Dims));
//  nd_size Spacing(1); // start with stride (1, 1, 1)
//  idx2_For (int, D, 0, nd_size.Dims())
//    Spacing[D] <<= Idx2.DownsamplingFactor3[D];
//
//  v3i First3 = From(CroppedExt);
//  v3i Last3 = Last(CroppedExt);
//  Last3 = ((Last3 + Spacing - 1) / Spacing) * Spacing; // move last to the right
//  First3 = (First3 / Spacing) * Spacing; // move first to the left
//
//  return grid(First3, (Last3 - First3) / Spacing + 1, Spacing);
//}


/*---------------------------------------------------------------------------------------------
A generic template for hierarchy traversal following a template.
---------------------------------------------------------------------------------------------*/
error<err_code>
file_chunk_brick_traversal::Traverse(const template_view& TemplateView,
                                     const nd_size& From, // in units of traverse_item
                                     const nd_size& Dims, // in units of traverse_item
                                     const nd_extent& Extent,
                                     const nd_extent& VolExtent, // in units of traverse_item
                                     traverse_callback* Callback) const
{
  idx2_RAII(array<traverse_item>, Stack, Reserve(&Stack, 64), Dealloc(&Stack));
  nd_size DimsPow2 = NextPow2(Dims);
  traverse_item Top;
  Top.From = From;
  Top.To = From + DimsPow2;
  Top.Pos = 0;
  PushBack(&Stack, Top);
  while (Size(Stack) >= 0)
  {
    Top = Back(Stack);
    i8 D = TemplateView[Top.Pos];
    PopBack(&Stack);
    if (!(Top.To - Top.From == 1))
    {
      // TODO NEXT: assert that the Pos is still iwthin template
      traverse_item First = Top, Second = Top;
      First.To[D] =
        Top.From[D] + (Top.To[D] - Top.From[D]) / 2;
      Second.From[D] =
        Top.From[D] + (Top.To[D] - Top.From[D]) / 2;
      nd_extent Skip(First.From, First.To - First.From);
      //Second.NItemsBefore = First.NItemsBefore + Prod<u64>(Dims(Crop(Skip, Extent)));
      Second.ItemOrder = First.ItemOrder + Prod<i32>(idx2::Dims(nd_Crop(Skip, VolExtent)));
      First.Address = Top.Address;
      Second.Address = Top.Address + Prod<u64>(First.To - First.From);
      First.Pos = Second.Pos = Top.Pos + 1;
      if (Second.From < To(Extent) && idx2::From(Extent) < Second.To)
        PushBack(&Stack, Second);
      if (First.From < To(Extent) && idx2::From(Extent) < First.To)
        PushBack(&Stack, First);
    }
    else
    {
      Top.LastItem = Size(Stack) == 0;
      idx2_PropagateIfError(Callback(*this, Top));
    }
  }

  return idx2_Error(err_code::NoError);
}


} // namespace idx2

