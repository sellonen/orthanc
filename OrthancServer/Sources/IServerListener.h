/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "DicomInstanceToStore.h"
#include "ServerIndexChange.h"

#include <json/value.h>

namespace Orthanc
{
  class IServerListener : public boost::noncopyable
  {
  public:
    virtual ~IServerListener()
    {
    }

    virtual void SignalStoredInstance(const std::string& publicId,
                                      const DicomInstanceToStore& instance,
                                      const Json::Value& simplifiedTags) = 0;
    
    virtual void SignalChange(const ServerIndexChange& change) = 0;

    virtual bool FilterIncomingInstance(const DicomInstanceToStore& instance,
                                        const Json::Value& simplified) = 0;

    virtual uint16_t FilterIncomingCStoreInstance(const DicomInstanceToStore& instance,
                                                  const Json::Value& simplified) = 0;

  };
}
