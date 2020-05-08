/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "MemoryBufferTranscoder.h"

#include "../OrthancException.h"
#include "FromDcmtkBridge.h"

namespace Orthanc
{
  MemoryBufferTranscoder::MemoryBufferTranscoder()
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    useDcmtk_ = true;
#else
    useDcmtk_ = false;
#endif
  }


  void MemoryBufferTranscoder::SetDcmtkUsed(bool used)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING != 1
    if (useDcmtk)
    {
      throw OrthancException(ErrorCode_NotImplemented,
                             "Orthanc was built without support for DMCTK transcoding");
    }
#endif    

    useDcmtk_ = used;
  }


  bool MemoryBufferTranscoder::TranscodeToBuffer(std::string& target,
                                                 bool& hasSopInstanceUidChanged,
                                                 const void* buffer,
                                                 size_t size,
                                                 const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                                 bool allowNewSopInstanceUid)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (useDcmtk_)
    {
      if (dcmtk_.TranscodeToBuffer(target, hasSopInstanceUidChanged, buffer,
                                   size, allowedSyntaxes, allowNewSopInstanceUid))
      {
        return true;
      }
    }
#endif

    DicomTransferSyntax sourceSyntax, targetSyntax;
    return Transcode(target, sourceSyntax, targetSyntax, hasSopInstanceUidChanged, buffer, size,
                     allowedSyntaxes, allowNewSopInstanceUid);
  }

  
  DcmFileFormat* MemoryBufferTranscoder::TranscodeToParsed(bool& hasSopInstanceUidChanged,
                                                           const void* buffer,
                                                           size_t size,
                                                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                                           bool allowNewSopInstanceUid)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (useDcmtk_)
    {
      std::unique_ptr<DcmFileFormat> transcoded(
        dcmtk_.TranscodeToParsed(hasSopInstanceUidChanged, buffer, size,
                                 allowedSyntaxes, allowNewSopInstanceUid));
      if (transcoded.get() != NULL)
      {
        return transcoded.release();
      }
    }
#endif

    std::string transcoded;
    DicomTransferSyntax sourceSyntax, targetSyntax;
    if (Transcode(transcoded, sourceSyntax, targetSyntax, hasSopInstanceUidChanged,
                  buffer, size, allowedSyntaxes, allowNewSopInstanceUid))
    {
      return FromDcmtkBridge::LoadFromMemoryBuffer(
        transcoded.empty() ? NULL : transcoded.c_str(), transcoded.size());
    }
    else
    {
      return NULL;
    }
  }


  bool MemoryBufferTranscoder::HasInplaceTranscode(
    DicomTransferSyntax inputSyntax,
    const std::set<DicomTransferSyntax>& outputSyntaxes) const
  {
    /**
     * Inplace transcoding is only possible if DCMTK is enabled, and
     * if DCMTK supports all the requested transfer
     * syntaxes. Otherwise, one has to call the "buffer-to-buffer"
     * transcoder.
     **/
    
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (useDcmtk_)
    {
      if (!DcmtkTranscoder::IsSupported(inputSyntax))
      {
        return false;
      }
      
      for (std::set<DicomTransferSyntax>::const_iterator
             it = outputSyntaxes.begin(); it != outputSyntaxes.end(); ++it)
      {
        if (!DcmtkTranscoder::IsSupported(*it))
        {
          return false;
        }
      }

      return true;
    }
    else
#endif
    {
      return false;
    }
  }
    

  bool MemoryBufferTranscoder::InplaceTranscode(bool& hasSopInstanceUidChanged,
                                                DcmFileFormat& dicom,
                                                const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                                bool allowNewSopInstanceUid)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DicomTransferSyntax inputSyntax;
    if (useDcmtk_ &&
        FromDcmtkBridge::LookupOrthancTransferSyntax(inputSyntax, dicom) &&
        HasInplaceTranscode(inputSyntax, allowedSyntaxes))
    {
      return dcmtk_.InplaceTranscode(hasSopInstanceUidChanged, dicom, allowedSyntaxes, allowNewSopInstanceUid);
    }
    else
#endif
    {
      // "HasInplaceTranscode()" should have been called
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }



  
  bool MemoryBufferTranscoder::TranscodeParsedToBuffer(
    std::string& target /* out */,
    DicomTransferSyntax& sourceSyntax /* out */,
    DicomTransferSyntax& targetSyntax /* out */,
    bool& hasSopInstanceUidChanged /* out */,
    DcmFileFormat& dicom /* in, possibly modified */,
    const std::set<DicomTransferSyntax>& allowedSyntaxes,
    bool allowNewSopInstanceUid)
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    std::string source;
    FromDcmtkBridge::SaveToMemoryBuffer(source, *dicom.getDataset());

    const void* data = source.empty() ? NULL : source.c_str();

    bool success = Transcode(target, sourceSyntax, targetSyntax, hasSopInstanceUidChanged,
                             data, source.size(), allowedSyntaxes, allowNewSopInstanceUid);

#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (useDcmtk_ &&
        dcmtk_.TranscodeParsedToBuffer(
          target, sourceSyntax, targetSyntax,hasSopInstanceUidChanged,
          dicom, allowedSyntaxes, allowNewSopInstanceUid))
    {
      success = true;
    }
#endif

    return success;
  }
  

  IDicomTranscoder::TranscodedDicom* MemoryBufferTranscoder::TranscodeToParsed2(
    DcmFileFormat& dicom /* in, possibly modified */,
    const void* buffer /* in, same DICOM file as "dicom" */,
    size_t size,
    const std::set<DicomTransferSyntax>& allowedSyntaxes,
    bool allowNewSopInstanceUid)
  {
    DicomTransferSyntax sourceSyntax, targetSyntax;
    bool hasSopInstanceUidChanged;
    
    std::string target;
    if (Transcode(target, sourceSyntax, targetSyntax, hasSopInstanceUidChanged,
                  buffer, size, allowedSyntaxes, allowNewSopInstanceUid))
    {
      const void* data = target.empty() ? NULL : target.c_str();
      return IDicomTranscoder::TranscodedDicom::CreateFromInternal(
        FromDcmtkBridge::LoadFromMemoryBuffer(data, target.size()), hasSopInstanceUidChanged);
    }
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    else if (useDcmtk_)
    {
      return dcmtk_.TranscodeToParsed2(dicom, buffer, size, allowedSyntaxes, allowNewSopInstanceUid);
    }
#endif
    else
    {
      return NULL;
    }
  }
}
