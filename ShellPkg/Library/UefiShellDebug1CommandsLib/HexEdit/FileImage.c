/** @file
  Functions to deal with file buffer.

  Copyright (c) 2005 - 2011, Intel Corporation. All rights reserved. <BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "HexEditor.h"

extern EFI_HANDLE                 HImageHandleBackup;
extern HEFI_EDITOR_BUFFER_IMAGE   HBufferImage;

extern BOOLEAN                    HBufferImageNeedRefresh;
extern BOOLEAN                    HBufferImageOnlyLineNeedRefresh;
extern BOOLEAN                    HBufferImageMouseNeedRefresh;

extern HEFI_EDITOR_GLOBAL_EDITOR  HMainEditor;

HEFI_EDITOR_FILE_IMAGE            HFileImage;
HEFI_EDITOR_FILE_IMAGE            HFileImageBackupVar;

//
// for basic initialization of HFileImage
//
HEFI_EDITOR_BUFFER_IMAGE          HFileImageConst = {
  NULL,
  0,
  FALSE
};

EFI_STATUS
HFileImageInit (
  VOID
  )
/*++

Routine Description: 

  Initialization function for HFileImage

Arguments:  

  None

Returns:  

  EFI_SUCCESS
  EFI_LOAD_ERROR

--*/
{
  //
  // basically initialize the HFileImage
  //
  CopyMem (&HFileImage, &HFileImageConst, sizeof (HFileImage));

  CopyMem (
    &HFileImageBackupVar,
    &HFileImageConst,
    sizeof (HFileImageBackupVar)
    );

  return EFI_SUCCESS;
}

EFI_STATUS
HFileImageBackup (
  VOID
  )
/*++

Routine Description: 

  Backup function for HFileImage
  Only a few fields need to be backup. 
  This is for making the file buffer refresh 
  as few as possible.

Arguments:  

  None

Returns:  

  EFI_SUCCESS
  EFI_OUT_OF_RESOURCES

--*/
{
  SHELL_FREE_NON_NULL (HFileImageBackupVar.FileName);
  HFileImageBackupVar.FileName = CatSPrint(NULL, L"%s", HFileImage.FileName);
  if (HFileImageBackupVar.FileName == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
HFileImageCleanup (
  VOID
  )
/*++

Routine Description: 

  Cleanup function for HFileImage

Arguments:  

  None

Returns:  

  EFI_SUCCESS

--*/
{

  SHELL_FREE_NON_NULL (HFileImage.FileName);
  SHELL_FREE_NON_NULL (HFileImageBackupVar.FileName);

  return EFI_SUCCESS;
}

EFI_STATUS
HFileImageSetFileName (
  IN CONST CHAR16 *Str
  )
/*++

Routine Description: 

  Set FileName field in HFileImage

Arguments:  

  Str -- File name to set

Returns:  

  EFI_SUCCESS
  EFI_OUT_OF_RESOURCES

--*/
{
  UINTN Size;
  UINTN Index;

  //
  // free the old file name
  //
  SHELL_FREE_NON_NULL (HFileImage.FileName);

  Size                = StrLen (Str);

  HFileImage.FileName = AllocateZeroPool (2 * (Size + 1));
  if (HFileImage.FileName == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < Size; Index++) {
    HFileImage.FileName[Index] = Str[Index];
  }

  HFileImage.FileName[Size] = L'\0';

  return EFI_SUCCESS;
}

EFI_STATUS
HFileImageGetFileInfo (
  IN  EFI_FILE_HANDLE Handle,
  IN  CHAR16          *FileName,
  OUT EFI_FILE_INFO   **InfoOut
  )
/*++

Routine Description: 

  Get this file's information

Arguments:  

  Handle   - in NT32 mode Directory handle, in other mode File Handle
  FileName - The file name
  InfoOut  - parameter to pass file information out

Returns:  

  EFI_SUCCESS
  EFI_OUT_OF_RESOURCES
  EFI_LOAD_ERROR

--*/
{

  VOID        *Info;
  UINTN       Size;
  EFI_STATUS  Status;

  Size  = SIZE_OF_EFI_FILE_INFO + 1024;
  Info  = AllocateZeroPool (Size);
  if (!Info) {
    return EFI_OUT_OF_RESOURCES;
  }
  //
  // get file information
  //
  Status = Handle->GetInfo (Handle, &gEfiFileInfoGuid, &Size, Info);
  if (EFI_ERROR (Status)) {
    return EFI_LOAD_ERROR;
  }

  *InfoOut = (EFI_FILE_INFO *) Info;

  return EFI_SUCCESS;

}

EFI_STATUS
HFileImageRead (
  IN CONST CHAR16  *FileName,
  IN BOOLEAN Recover
  )
/*++

Routine Description: 

  Read a file from disk into HBufferImage

Arguments:  

  FileName -- filename to read
  Recover -- if is for recover, no information print

Returns:  

  EFI_SUCCESS
  EFI_LOAD_ERROR
  EFI_OUT_OF_RESOURCES
  
--*/
{
  HEFI_EDITOR_LINE                *Line;
  UINT8                           *Buffer;
  CHAR16                          *UnicodeBuffer;
  EFI_STATUS                      Status;

  //
  // variable initialization
  //
  Line                    = NULL;

  //
  // in this function, when you return error ( except EFI_OUT_OF_RESOURCES )
  // you should set status string
  // since this function maybe called before the editorhandleinput loop
  // so any error will cause editor return
  // so if you want to print the error status
  // you should set the status string
  //
  Status = ReadFileIntoBuffer (FileName, (VOID**)&Buffer, &HFileImage.Size, &HFileImage.ReadOnly);
  if (EFI_ERROR(Status)) {
    UnicodeBuffer = CatSPrint(NULL, L"Read error on file &s: %r", FileName, Status);
    if (UnicodeBuffer == NULL) {
      SHELL_FREE_NON_NULL(Buffer);
      return EFI_OUT_OF_RESOURCES;
    }

    StatusBarSetStatusString (UnicodeBuffer);
    FreePool (UnicodeBuffer);
  }

  HFileImageSetFileName (FileName);

  //
  // free the old lines
  //
  HBufferImageFree ();

  Status = HBufferImageBufferToList (Buffer, HFileImage.Size);
  SHELL_FREE_NON_NULL (Buffer);
  if (EFI_ERROR (Status)) {
    StatusBarSetStatusString (L"Error parsing file.");
    return Status;
  }

  HBufferImage.DisplayPosition.Row    = 2;
  HBufferImage.DisplayPosition.Column = 10;
  HBufferImage.MousePosition.Row      = 2;
  HBufferImage.MousePosition.Column   = 10;
  HBufferImage.LowVisibleRow          = 1;
  HBufferImage.HighBits               = TRUE;
  HBufferImage.BufferPosition.Row     = 1;
  HBufferImage.BufferPosition.Column  = 1;

  if (!Recover) {
    UnicodeBuffer = CatSPrint(NULL, L"%d Lines Read", HBufferImage.NumLines);
    if (UnicodeBuffer == NULL) {
      SHELL_FREE_NON_NULL(Buffer);
      return EFI_OUT_OF_RESOURCES;
    }

    StatusBarSetStatusString (UnicodeBuffer);
    FreePool (UnicodeBuffer);

    HMainEditor.SelectStart = 0;
    HMainEditor.SelectEnd   = 0;
  }

  //
  // has line
  //
  if (HBufferImage.Lines != 0) {
    HBufferImage.CurrentLine = CR (HBufferImage.ListHead->ForwardLink, HEFI_EDITOR_LINE, Link, EFI_EDITOR_LINE_LIST);
  } else {
    //
    // create a dummy line
    //
    Line = HBufferImageCreateLine ();
    if (Line == NULL) {
      SHELL_FREE_NON_NULL(Buffer);
      return EFI_OUT_OF_RESOURCES;
    }

    HBufferImage.CurrentLine = Line;
  }

  HBufferImage.Modified           = FALSE;
  HBufferImageNeedRefresh         = TRUE;
  HBufferImageOnlyLineNeedRefresh = FALSE;
  HBufferImageMouseNeedRefresh    = TRUE;

  return EFI_SUCCESS;
}

EFI_STATUS
HFileImageSave (
  IN CHAR16 *FileName
  )
/*++

Routine Description: 

  Save lines in HBufferImage to disk

Arguments:  

  FileName - The file name

Returns:  

  EFI_SUCCESS
  EFI_LOAD_ERROR
  EFI_OUT_OF_RESOURCES

--*/
{

  LIST_ENTRY                      *Link;
  HEFI_EDITOR_LINE                *Line;
  CHAR16                          *Str;
  EFI_STATUS                      Status;
  UINTN                           NumLines;
  SHELL_FILE_HANDLE                 FileHandle;
  UINTN                           TotalSize;
  UINT8                           *Buffer;
  UINT8                           *Ptr;
  EDIT_FILE_TYPE                  BufferTypeBackup;

  BufferTypeBackup        = HBufferImage.BufferType;
  HBufferImage.BufferType = FileTypeFileBuffer;

  //
  // if is the old file
  //
  if (StrCmp (FileName, HFileImage.FileName) == 0) {
    //
    // check whether file exists on disk
    //
    if (ShellIsFile(FileName) == EFI_SUCCESS) {
      //
      // current file exists on disk
      // so if not modified, then not save
      //
      if (HBufferImage.Modified == FALSE) {
        return EFI_SUCCESS;
      }
      //
      // if file is read-only, set error
      //
      if (HFileImage.ReadOnly == TRUE) {
        StatusBarSetStatusString (L"Read Only File Can Not Be Saved");
        return EFI_SUCCESS;
      }
    }
  }

   if (ShellIsDirectory(FileName) == EFI_SUCCESS) {
    StatusBarSetStatusString (L"Directory Can Not Be Saved");
    return EFI_LOAD_ERROR;
  }

  Status = ShellOpenFileByName (FileName, &FileHandle, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE, 0);

  if (!EFI_ERROR (Status)) {
    //
    // the file exits, delete it
    //
    Status = ShellDeleteFile (&FileHandle);
    if (EFI_ERROR (Status) || Status == EFI_WARN_DELETE_FAILURE) {
      StatusBarSetStatusString (L"Write File Failed");
      return EFI_LOAD_ERROR;
    }
 }

  //
  // write all the lines back to disk
  //
  NumLines  = 0;
  TotalSize = 0;
  for (Link = HBufferImage.ListHead->ForwardLink; Link != HBufferImage.ListHead; Link = Link->ForwardLink) {
    Line = CR (Link, HEFI_EDITOR_LINE, Link, EFI_EDITOR_LINE_LIST);

    if (Line->Size != 0) {
      TotalSize += Line->Size;
    }
    //
    // end of if Line -> Size != 0
    //
    NumLines++;
  }
  //
  // end of for Link
  //
  Buffer = AllocateZeroPool (TotalSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ptr = Buffer;
  for (Link = HBufferImage.ListHead->ForwardLink; Link != HBufferImage.ListHead; Link = Link->ForwardLink) {
    Line = CR (Link, HEFI_EDITOR_LINE, Link, EFI_EDITOR_LINE_LIST);

    if (Line->Size != 0) {
      CopyMem (Ptr, Line->Buffer, Line->Size);
      Ptr += Line->Size;
    }
    //
    // end of if Line -> Size != 0
    //
  }


  Status = ShellOpenFileByName (FileName, &FileHandle, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE, 0);

  if (EFI_ERROR (Status)) {
    StatusBarSetStatusString (L"Create File Failed");
    return EFI_LOAD_ERROR;
  }

  Status = ShellWriteFile (FileHandle, &TotalSize, Buffer);
  FreePool (Buffer);
  if (EFI_ERROR (Status)) {
    ShellDeleteFile (&FileHandle);
    return EFI_LOAD_ERROR;
  }

  ShellCloseFile(&FileHandle);

  HBufferImage.Modified = FALSE;

  //
  // set status string
  //
  Str = CatSPrint(NULL, L"%d Lines Wrote", NumLines);
  StatusBarSetStatusString (Str);
  FreePool (Str);

  //
  // now everything is ready , you can set the new file name to filebuffer
  //
  if (BufferTypeBackup != FileTypeFileBuffer || StringNoCaseCompare (&FileName, &HFileImage.FileName) != 0) {
    //
    // not the same
    //
    HFileImageSetFileName (FileName);
    if (HFileImage.FileName == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  HFileImage.ReadOnly = FALSE;

  return EFI_SUCCESS;
}
