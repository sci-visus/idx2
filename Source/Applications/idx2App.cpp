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
        if (J >= NArgs || !ToInt(Args[J++], &Dim.Limit))
          return false;
        PushBack(Dimensions, Dim);
      }
      return true;
    }
  }
  return false;
}


/*---------------------------------------------------------------------------------------------
Parse the options for --encode from the command line.
---------------------------------------------------------------------------------------------*/
static void
ParseEncodeOptions(int Argc, cstr* Argv, idx2_file_v2* Idx2)
{
  ///* indexing --template */
  //if (!OptVal(Argc, Argv, "--template", &Idx2->IdxTemplate.Full))
  //  idx2_Exit("Provide indexing --template, e.g., zyzyxzyxzyxzyx|zyx:zyx:zyx:zyx\n");

  ///* --name */
  //cstr Name = nullptr;
  //if (OptVal(Argc, Argv, "--name", &Name))
  //  snprintf(Idx2->Name, sizeof(Idx2->Name), "%s", Name);
  //else
  //  idx2_Exit("Provide --name, e.g., miranda\n");

  ///* --fields */
  //array<name_str> Fields;
  //if (OptVal(Argc, Argv, "--fields", &Fields))
  //{
  //  dimension_info Dim;
  //  {
  //    Dim.Names = Fields;
  //    Dim.ShortName = 'f'; // TODO: hardcoding
  //  }
  //  PushBack(&Idx2->Dimensions, Dim);
  //}

  ///* --bits_per_brick */
  //OptVal(Argc, Argv, "--name", &Name);
}


/*---------------------------------------------------------------------------------------------
Flush the standard input.
---------------------------------------------------------------------------------------------*/
static void
FlushStdIn()
{
  char c;
  while ((c = fgetc(stdin)) != '\n' && c != EOF)
    ; /* Flush stdin */
}


/*---------------------------------------------------------------------------------------------
Ask the user to input the name of the dataset.
---------------------------------------------------------------------------------------------*/
static void
GetName(idx2_file* Idx2)
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
      continue;
    else if (C == '\n')
      break;
    else
      goto LOOP;
  }
}


/*---------------------------------------------------------------------------------------------
Ask for the user to enter the fields.
---------------------------------------------------------------------------------------------*/
void
GetFields(idx2_file* Idx2)
{
  dimension_info Fields;
  Fields.ShortName = 'f';
  while (true)
  {
    name_str Name;
    printf("Add a field (no space): ");
    FGets(&Name);
    LOOP:
    printf("You entered %s.\n"
            "- Press [Enter] to stop adding fields,\n"
            "- Type 'r' [Enter] to re-enter, or\n"
            "- Type 'n' [Enter] to add another field.\n", Name.Data);
    char C = getchar();
    if (C == 'r')
    {
      continue;
    }
    else if (C == 'n')
    {
      PushBack(&Fields.Names, Name);
      continue;
    }
    else if (C == '\n')
    {
      PushBack(&Fields.Names, Name);
      break;
    }
    else
    {
      goto LOOP;
    }
  }

  PushBack(&Idx2->DimensionInfo, Fields);

  printf("The following %d fields have been added: ", (i32)Size(Fields));
  idx2_ForEach (Field, Fields.Names)
    printf("%s ", Field->Data);
}


/*---------------------------------------------------------------------------------------------
Ask for the user to enter the dimensions.
---------------------------------------------------------------------------------------------*/
static void
GetDimensions(idx2_file* Idx2)
{
  while (true)
  {
    dimension_info Dimension;
    printf("Add a dimension (e.g., x 512): ");
    Dimension.ShortName = getchar();
    scanf("%d", &Dimension.Limit);
    FlushStdIn();
    LOOP:
    printf("You entered %c %d.\n"
           "- Press [Enter] to stop adding dimensions,\n"
           "- Type 'r' [Enter] to re-enter, or\n"
           "- Type 'n' [Enter] to add another dimension.\n", Dimension.ShortName, Dimension.Limit);
    char C = getchar();
    FlushStdIn();
    if (C == 'r')
    {
      continue;
    }
    else if (C == 'n')
    {
      PushBack(&Idx2->DimensionInfo, Dimension);
      continue;
    }
    else if (C == '\n')
    {
      PushBack(&Idx2->DimensionInfo, Dimension);
      break;
    }
    else
    {
      goto LOOP;
    }
  }

  printf("The following %d dimensions have been added: ", (i32)Size(Idx2->DimensionInfo));
  idx2_ForEach (Dim, Idx2->DimensionInfo)
    printf("%c %d  ", Dim->ShortName, Dim->Limit);
}


/*---------------------------------------------------------------------------------------------
Perform the --create action.
---------------------------------------------------------------------------------------------*/
static void
GetTransformTemplate(idx2_file* Idx2)
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
    FlushStdIn();
    if (C == 'n')
    {
      TemplateHint = template_hint((int(TemplateHint) + 1) % int(template_hint::Size));
      continue;
    }
    else if (C == '\n')
    {
      Idx2->Template.Original = Template;
      break;
    }
    else if (C == 'r')
    {
      printf("Enter your desired template: ");
      scanf("%96s", Template.Data);
      FlushStdIn();
      goto LOOP;
    }
    else
    {
      goto LOOP;
    }
  }

  // TODO NEXT: verify the template
}


/*---------------------------------------------------------------------------------------------
Perform the --create action.
---------------------------------------------------------------------------------------------*/
static void
DoCreate()
{
  idx2_file Idx2;
  GetName(&Idx2);
  GetFields(&Idx2);
  GetDimensions(&Idx2);
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
    printf("  1. Create dataset (--create)\n");
    printf("  2. Encode a portion or the entirety of a dataset (--encode)\n");
    printf("  3. Decode a portion or the entirety of a dataset (--decode)\n");
    printf("Enter your choice (--create, --encode, or --decode): ");
    FGets(&Action);
    LOOP:
    printf("You entered %.*s.\n", Action.Size, Action.Data);
    printf("Press [Enter] to continue or type 'r' [Enter] to re-enter.\n");
    char C = getchar();
    if (C == 'r')
      continue;
    else if (C == '\n')
      break;
    else
      goto LOOP;
  }

  /* perform the action */
  if (StrEqual(Action.Data, "--create"))
  {
    DoCreate();
  }
  else if (StrEqual(Action.Data, "--encode"))
  {
    DoEncode();
  }
  else if (StrEqual(Action.Data, "--decode"))
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

