#include "../idx2_v2.h"


using namespace idx2;


/*---------------------------------------------------------------------------------------------
Parse the --dimensions option from the command line.
---------------------------------------------------------------------------------------------*/
bool
OptVal(i32 NArgs, cstr* Args, cstr Opt, array<dimension_info>* Dimensions)
{
  for (i32 I = 0; I < NArgs; ++I)
  {
    if (strncmp(Args[I], Opt, 32) == 0)
    {
      i32 J = I + 1;
      while (true)
      {
        dimension_info Dim;
        if (J >= NArgs || !isalpha(Args[J][0]) || !islower(Args[J][0]))
          return J > I + 1;
        Dim.ShortName = Args[J++][0];
        if (J >= NArgs || !ToInt(Args[J++], &Dim.UpperLimit))
          return false;
        PushBack(Dimensions, Dim);
      }
      return true;
    }
  }
  return false;
}


/*---------------------------------------------------------------------------------------------
Ask the user to input the name of the dataset.
---------------------------------------------------------------------------------------------*/
static error<err_code>
InputName(idx2_file* Idx2)
{
  while (true)
  {
    printf("Name the dataset (no space): ");
    FGets(&Idx2->Name);
    LOOP:
    printf("You entered %.*s.\n", Idx2->Name.Size, Idx2->Name.Data);
    printf("Press [Enter] to continue or type 'r' [Enter] to re-enter.\n");
    char C = getchar();
    if (C == 'r')
    {
      FlushStdIn();
      continue;
    }
    else if (C == '\n')
    {
      break;
    }
    else
    {
      FlushStdIn();
      goto LOOP;
    }
  }

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Ask for the user to enter the fields.
---------------------------------------------------------------------------------------------*/
static error<err_code>
InputFields(idx2_file* Idx2)
{
  dimension_info Fields;
  Fields.ShortName = 'f';
  while (true)
  {
    name_str Name;
    int NComponents = 1;
    dtype Type = dtype::__Invalid__;
    type_str TypeStr;
    while (true)
    {
      printf("Add a field (e.g., pressure float64, color 3uint8, temperature float32, velocity 2uint32, etc): ");
      input_str InputStr;
      bool FGetOk = false;
      FGetOk = fgets(InputStr.Data, InputStr.Capacity(), stdin);
      if (!FGetOk)
        goto ERR;

      if (3 == sscanf(InputStr.Data, "%s %d%s", Name.Data, &NComponents, TypeStr.Data))
      {
        Name.Size = (i8)strnlen(Name.Data, Name.Capacity() - 1);
        Type = StringToDType(stref(TypeStr.Data));
        if (Name.Size > 0 && NComponents > 0 && Type != dtype::__Invalid__)
          break;
      }
      else if (2 == sscanf(InputStr.Data, "%s %s", Name.Data, TypeStr.Data))
      {
        Name.Size = (i8)strnlen(Name.Data, Name.Capacity() - 1);
        Type = StringToDType(stref(TypeStr.Data));
        if (Name.Size > 0 && Type != dtype::__Invalid__)
          break;
      }

      ERR:
      printf("Invalid input. Check that the base type is one of:\n");
      printf("int8, uint8, int16, uint16, int32, uint32, int64, uint64, float32, float64\n");
    }
    LOOP:
    printf("You entered field name: %s, ", Name.Data);
    printf("of type %s, ", ToString(Type));
    printf("with %d components.\n", NComponents);
    printf("- Press [Enter] to stop adding fields,\n"
           "- Type 'r' [Enter] to re-enter, or\n"
           "- Type 'n' [Enter] to add another field.\n");
    char C = getchar();
    if (C == 'n' || C == '\n')
    {
      PushBack(&Fields.FieldNames, Name);
      PushBack(&Fields.FiledTypes, composite_type{Type, (i8)NComponents});
      if (C == '\n')
        break;
      FlushStdIn();
      continue;
    }
    else
    {
      FlushStdIn();
      if (C == 'r')
        continue;
      goto LOOP;
    }
  }

  i8 D = (i8)Size(Idx2->DimensionInfo);
  Idx2->Dims[D] = (i32)Size(Fields.FieldNames);
  Idx2->DimensionMap[Fields.ShortName - 'a'] = D;
  Idx2->DimensionMapInverse[D] = Fields.ShortName;
  PushBack(&Idx2->DimensionInfo, Fields);

  printf("The following %d fields have been added:\n", (i32)Size(Fields));
  idx2_ForEach (Field, Fields.FieldNames)
    printf("%.*s ", Field->Size, Field->Data);
  printf("\n\n");

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Ask for the user to enter the dimensions.
---------------------------------------------------------------------------------------------*/
static error<err_code>
InputDimensions(idx2_file* Idx2)
{
  while (true)
  {
    dimension_info Dimension;
    printf("Add a dimension (e.g., x 512; dimension cannot be 'f'): ");
    int Result = 0;
    while (Result != 2)
    {
      Result = scanf("%c %d", &Dimension.ShortName, &Dimension.UpperLimit);
      FlushStdIn();
      // TODO NEXT: check the short name and the limit
    }
  LOOP:
    printf("You entered %c %d.\n"
           "- Press [Enter] to stop adding dimensions,\n"
           "- Type 'r' [Enter] to re-enter, or\n"
           "- Type 'n' [Enter] to add another dimension.\n", Dimension.ShortName, Dimension.UpperLimit);
    char C = getchar();
    if (C == 'n' || C == '\n')
    {
      i8 D = (i8)Size(Idx2->DimensionInfo);
      Idx2->DimensionMap[Dimension.ShortName - 'a'] = D;
      Idx2->Dims[D] = Dimension.UpperLimit;
      Idx2->DimensionMapInverse[D] = Dimension.ShortName;
      PushBack(&Idx2->DimensionInfo, Dimension);
      if (C == '\n')
        break;
      FlushStdIn();
      continue;
    }
    else
    {
      FlushStdIn();
      if (C == 'r')
        continue;
      goto LOOP;
    }
  }

  printf("%d dimensions have been added (f stands for fields): ", (i32)Size(Idx2->DimensionInfo));
  idx2_ForEach (Dim, Idx2->DimensionInfo)
  {
    if (Dim->ShortName != 'f')
      printf("%c %d, ", Dim->ShortName, Dim->UpperLimit);
    else
      printf("%c %d, ", Dim->ShortName, (i32)Size(Dim->FieldNames));
  }
  printf("\n\n");

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Perform the --create action.
---------------------------------------------------------------------------------------------*/
static error<err_code>
InputTransformTemplate(idx2_file* Idx2)
{
  template_hint TemplateHint = template_hint::Isotropic;
  while (true)
  {
    auto Template = GuessTransformTemplate(*Idx2, TemplateHint);
    printf("Provide a transform template: default = %s\n", Template.Data);
    LOOP:
    printf("- Press [Enter] to accept the template above,\n"
           "- Type 'n' [Enter] to cycle through suggested templates, or\n"
           "- Type 'r' [Enter] to type a custom template.\n");
    char C = getchar();
    if (C == 'n')
    {
      TemplateHint = template_hint((int(TemplateHint) + 1) % int(template_hint::Size));
      FlushStdIn();
      continue;
    }
    else if (C == '\n')
    {
      Idx2->Template.Original = Template;
      break;
    }
    else if (C == 'r')
    {
      FlushStdIn();
      printf("Enter your desired template: ");
      FGets(&Template);
      goto LOOP;
    }
    else
    {
      FlushStdIn();
      goto LOOP;
    }
  }

  // TODO NEXT: verify the template
  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Write --name to the metadata file. This is the first thing that is written in the file.
---------------------------------------------------------------------------------------------*/
static error<err_code>
WriteName(const idx2_file& Idx2)
{
  idx2_RAII(FILE*, Fp, Fp = fopen(idx2_PrintScratch("%s.idx2", Idx2.Name.Data), "w"), if (Fp) fclose(Fp));
  if (!Fp)
    return idx2_Error(err_code::FileCreateFailed, "Cannot create file %s\n", Idx2.Name.Data);

  fprintf(Fp, "--name %s\n", Idx2.Name.Data);
  printf("File %s.idx2 updated\n", Idx2.Name.Data);

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Write --fields to the metadata file.
---------------------------------------------------------------------------------------------*/
static error<err_code>
WriteFields(const idx2_file& Idx2)
{
  idx2_RAII(FILE*, Fp, Fp = fopen(idx2_PrintScratch("%s.idx2", Idx2.Name.Data), "a"), if (Fp) fclose(Fp));
  if (!Fp)
    return idx2_Error(err_code::FileOpenFailed, "Cannot open file %s\n", Idx2.Name.Data);

  dimension_info& Fields = Idx2.DimensionInfo[Idx2.DimensionMap['f' - 'a']];
  fprintf(Fp, "--fields %d  ", (i32)Size(Fields.FieldNames));
  idx2_ForEach (Name, Fields.FieldNames)
    fprintf(Fp, "%.*s ", Name->Size, Name->Data);
  fprintf(Fp, "\n");

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Write --dimensions to the metadata file.
---------------------------------------------------------------------------------------------*/
static error<err_code>
WriteDimensions(const idx2_file& Idx2)
{
  idx2_RAII(FILE*, Fp, Fp = fopen(idx2_PrintScratch("%s.idx2", Idx2.Name.Data), "a"), if (Fp) fclose(Fp));
  if (!Fp)
    return idx2_Error(err_code::FileOpenFailed, "Cannot open file %s\n", Idx2.Name.Data);

  // NOTE: since this function is called before InputFields, the DimensionInfo has yet to
  // contain the fields dimension (otherwise we need Size(DimensionInfo) - 1)
  fprintf(Fp, "--dimensions %d  ", (i32)Size(Idx2.DimensionInfo));
  idx2_ForEach (Dimension, Idx2.DimensionInfo)
  {
    if (Dimension->ShortName == 'f')
      continue;

    fprintf(Fp, "%c %d  ", Dimension->ShortName, Dimension->UpperLimit);
  }
  fprintf(Fp, "\n");

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Perform the --create action.
---------------------------------------------------------------------------------------------*/
static error<err_code>
DoCreate()
{
  idx2_file Idx2;
  idx2_PropagateIfError(InputName(&Idx2));
  idx2_PropagateIfError(WriteName(Idx2));
  idx2_PropagateIfError(InputDimensions(&Idx2));
  idx2_PropagateIfError(WriteDimensions(Idx2));
  idx2_PropagateIfError(InputFields(&Idx2));
  idx2_PropagateIfError(WriteFields(Idx2));
  idx2_PropagateIfError(InputTransformTemplate(&Idx2));
  //Finalize(&Idx2);

  return idx2_Error(err_code::NoError);
}


/*---------------------------------------------------------------------------------------------
Perform the --encode action.
---------------------------------------------------------------------------------------------*/
static void
DoEncode()
{
}


/*---------------------------------------------------------------------------------------------
Perform the --decode action.
---------------------------------------------------------------------------------------------*/
static void
DoDecode()
{
}


/*---------------------------------------------------------------------------------------------
Ask the user for the action they want to perform.
---------------------------------------------------------------------------------------------*/
static void
ChooseAction()
{
  /* prompt for the action */
  stack_string<10> Action;
  while (true)
  {
    printf("Choose action:\n");
    printf("  1. Create dataset (create)\n");
    printf("  2. Encode a portion or the entirety of a dataset (encode)\n");
    printf("  3. Decode a portion or the entirety of a dataset (decode)\n");
    printf("Enter your choice (create, encode, or decode): ");
    FGets(&Action);
    LOOP:
    printf("You entered %.*s.\n", Action.Size, Action.Data);
    printf("Press [Enter] to continue or type 'r' [Enter] to re-enter.\n");
    char C = getchar();
    if (C == 'r')
    {
      FlushStdIn();
      continue;
    }
    else if (C == '\n')
    {
      break;
    }
    else
    {
      FlushStdIn();
      goto LOOP;
    }
  }

  /* perform the action */
  if (StrEqual(Action.Data, "create"))
  {
    DoCreate();
  }
  else if (StrEqual(Action.Data, "encode"))
  {
    DoEncode();
  }
  else if (StrEqual(Action.Data, "decode"))
  {
    DoDecode();
  }
  else
  {
    idx2_Exit("Unknown action entered (not one of --create, --encode, --decode)\n");
  }
}


/*---------------------------------------------------------------------------------------------
Main function.
---------------------------------------------------------------------------------------------*/
int
main(int Argc, cstr* Argv)
{
  SetHandleAbortSignals();
  ChooseAction();
  return 0;

  CheckForUnsupportedOpt(Argc,
                         Argv,
                         "--encode "
                         "--decode "
                         "--dimensions "
                         "--template "
                         "--fields "
                         "--name"
                         "--bits_per_brick"
                         "--brick_bits_per_chunk"
                         "--chunk_bits_per_file"
                         "--file_bits_per_dir"
                         "--file_bits_per_meta");

  return 0;
}

