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


#pragma once

#include "ImageAccessor.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

namespace Orthanc
{
  class PamReader : public ImageAccessor
  {
  private:
    void ParseContent();
    
    /**
    Whether we want to use the default malloc alignment in the image buffer,
    at the expense of an extra copy
    */
    bool enforceAligned_;

    /**
    This is actually a copy of wrappedContent_, but properly aligned.

    It is only used if the enforceAligned parameter is set to true in the
    constructor.
    */
    void* alignedImageBuffer_;
    
    /**
    Points somewhere in the content_ buffer.      
    */
    ImageAccessor wrappedContent_;

    /**
    Raw content (file bytes or answer from the server, for instance). 
    */
    std::string content_;

  public:
    /**
    See doc for field enforceAligned_
    */
    PamReader(bool enforceAligned = false) :
      enforceAligned_(enforceAligned),
      alignedImageBuffer_(NULL)
    {
    }

    virtual ~PamReader()
    {
      // freeing NULL is OK
      free(alignedImageBuffer_);
    }

#if ORTHANC_SANDBOXED == 0
    void ReadFromFile(const std::string& filename);
#endif

    void ReadFromMemory(const std::string& buffer);

    void ReadFromMemory(const void* buffer,
                        size_t size);
  };
}
