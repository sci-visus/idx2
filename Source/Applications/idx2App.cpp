#include "../idx2_v2.h"


using namespace idx2;

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
        if (J >= NArgs || !ToInt(Args[J++], &Dim.Limit))
          return false;
        PushBack(Dimensions, Dim);
      }
      return true;
    }
  }
  return false;
}


static void
ParseEncodeOptions(int Argc, cstr* Argv, idx2_file_v2* Idx2)
{
  /* indexing --template */
  if (!OptVal(Argc, Argv, "--template", &Idx2->IdxTemplate.Full))
    idx2_Exit("Provide indexing --template, e.g., zyzyxzyxzyxzyx|zyx:zyx:zyx:zyx\n");

  /* --name */
  cstr Name = nullptr;
  if (OptVal(Argc, Argv, "--name", &Name))
    snprintf(Idx2->Name, sizeof(Idx2->Name), "%s", Name);
  else
    idx2_Exit("Provide --name, e.g., miranda\n");

  /* --fields */
  array<stref> Fields;
  if (OptVal(Argc, Argv, "--fields", &Fields))
  {
    dimension_info Dim;
    {
      Dim.Names = Fields;
      Dim.ShortName = 'f'; // TODO: hardcoding
    }
    PushBack(&Idx2->Dimensions, Dim);
  }

  /* --bits_per_brick */
  OptVal(Argc, Argv, "--name", &Name);
}


static void
FlushStdIn()
{
  char c;
  while ((c = fgetc(stdin)) != '\n' && c != EOF)
    ; /* Flush stdin */
}

static stack_string<65>
GetName()
{
  stack_string<65> Name;
  while (true)
  {
    printf("Name the dataset (no space): ");
    scanf("%64s", Name.Data);
    FlushStdIn();
    LOOP:
    printf("You entered %s. Press [Enter] to continue or type 'r' [Enter] to re-enter.\n", Name.Data);
    char C = getchar();
    if (C == 'r')
      continue;
    else if (C == '\n')
      break;
    else
      goto LOOP;
  }
  return Name;
}

array<stack_string<65>>
GetFields()
{
  array<stack_string<65>> AllFields;
  while (true)
  {
    stack_string<65> Field;
    printf("Add a field (no space): ");
    scanf("%64s", Field.Data);
    FlushStdIn();
    LOOP:
    printf("You entered %s. Press [Enter] to stop adding fields, type 'r' [Enter] to re-enter, or 'n' [Enter] to add another field.\n", Field.Data);
    char C = getchar();
    if (C == 'r')
    {
      continue;
    }
    else if (C == 'n')
    {
      PushBack(&AllFields, Field);
      continue;
    }
    else if (C == '\n')
    {
      PushBack(&AllFields, Field);
      break;
    }
    else
    {
      goto LOOP;
    }
  }

  printf("The following %d fields have been added: ", (i32)Size(AllFields));
  idx2_ForEach (Field, AllFields)
    printf("%s ", Field->Data);

  return AllFields;
}


static array<dimension_info>
GetDimensions()
{
  array<dimension_info> AllDimensions;
  while (true)
  {
    dimension_info Dimension;
    printf("Add a dimension (e.g., x 512): ");
    Dimension.ShortName = getchar();
    scanf("%d", &Dimension.Limit);
    FlushStdIn();
    LOOP:
    printf("You entered %c %d. Press [Enter] to stop adding fields, type 'r' [Enter] to re-enter, or 'n' [Enter] to add another field.\n", Dimension.ShortName, Dimension.Limit);
    char C = getchar();
    FlushStdIn();
    if (C == 'r')
    {
      continue;
    }
    else if (C == 'n')
    {
      PushBack(&AllDimensions, Dimension);
      continue;
    }
    else if (C == '\n')
    {
      PushBack(&AllDimensions, Dimension);
      break;
    }
    else
    {
      goto LOOP;
    }
  }

  printf("The following %d dimensions have been added: ", (i32)Size(AllDimensions));
  idx2_ForEach (Dimension, AllDimensions)
    printf("%c %d  ", Dimension->ShortName, Dimension->Limit);

  return AllDimensions;
}


static void
DoCreate(i32 Argc, cstr* Argv)
{
  auto Name = GetName();
  auto Fields = GetFields();
  auto Dimensions = GetDimensions();
}


static void
DoEncode(i32 Argc, cstr* Argv, cstr InputFile)
{
  idx2_RAII(idx2_file_v2, Idx2);
  ParseEncodeOptions(Argc, Argv, &Idx2);
  idx2_ExitIfError(Finalize(&Idx2));
}


static void
DoDecode(i32 Argc, cstr* Argv, cstr InputFile)
{
}


static stack_string<9>
ChooseAction(i32 Argc, cstr* Argv)
{
  stack_string<9> Action;
  while (true)
  {
    printf("Choose action:\n");
    printf("  1. Create dataset (--create)\n");
    printf("  2. Encode a portion or the entirety of a dataset (--encode)\n");
    printf("  3. Decode a portion or the entirety of a dataset (--decode)\n");
    printf("Enter your choice (--create, --encode, or --decode): ");
    scanf("%8s", Action.Data);
    FlushStdIn();
    LOOP:
    printf("You entered %s. Press [Enter] to continue or type 'r' [Enter] to re-enter.\n", Action.Data);
    char C = getchar();
    if (C == 'r')
      continue;
    else if (C == '\n')
      break;
    else
      goto LOOP;
    if (strcmp(Action.Data, "--create") == 0 ||
        strcmp(Action.Data, "--encode") == 0 ||
        strcmp(Action.Data, "--decode") == 0)
    {
      break;
    }
  }
  return Action;
}


int
main(int Argc, cstr* Argv)
{
  SetHandleAbortSignals();

  auto Action = ChooseAction(Argc, Argv);
  if (strcmp(Action.Data, "--create") == 0)
  {
    DoCreate(Argc, Argv);
  }
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

  /* Parse the action (--encode or --decode) */
  cstr InputFile = nullptr;
  if (OptVal(Argc, Argv, "--encode", &InputFile))
    DoEncode(Argc, Argv, InputFile);
  else if (OptVal(Argc, Argv, "--decode", &InputFile))
    DoDecode(Argc, Argv, InputFile);
  else
    idx2_Exit("Provide either --encode or --decode, followed by a file name\n");

  return 0;
}

