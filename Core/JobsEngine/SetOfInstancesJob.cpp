/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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
#include "SetOfInstancesJob.h"

#include "../OrthancException.h"
#include "../SerializationToolbox.h"

namespace Orthanc
{
  SetOfInstancesJob::SetOfInstancesJob() :
    started_(false),
    permissive_(false),
    position_(0)
  {
  }

    
  void SetOfInstancesJob::Reserve(size_t size)
  {
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      instances_.reserve(size);
    }
  }

    
  void SetOfInstancesJob::AddInstance(const std::string& instance)
  {
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      instances_.push_back(instance);
    }
  }


  void SetOfInstancesJob::SetPermissive(bool permissive)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      permissive_ = permissive;
    }
  }


  void SetOfInstancesJob::Reset()
  {
    if (started_)
    {
      position_ = 0;
      failedInstances_.clear();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

    
  float SetOfInstancesJob::GetProgress()
  {
    if (instances_.size() == 0)
    {
      return 0;
    }
    else
    {
      return (static_cast<float>(position_) /
              static_cast<float>(instances_.size()));
    }
  }


  const std::string& SetOfInstancesJob::GetInstance(size_t index) const
  {
    if (index > instances_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return instances_[index];
    }
  }
      

  JobStepResult SetOfInstancesJob::Step()
  {
    if (!started_)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    if (instances_.empty() &&
        position_ == 0)
    {
      // No instance to handle, we're done
      position_ = 1;
      return JobStepResult::Success();
    }

    if (position_ >= instances_.size())
    {
      // Already done
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    const std::string currentInstance = instances_[position_];
    
    bool ok;
      
    try
    {
      ok = HandleInstance(currentInstance);

      if (!ok && !permissive_)
      {
        return JobStepResult::Failure(ErrorCode_InternalError);
      }
    }
    catch (OrthancException&)
    {
      if (permissive_)
      {
        ok = false;
      }
      else
      {
        throw;
      }
    }

    if (!ok)
    {
      failedInstances_.insert(currentInstance);
    }

    position_ += 1;

    if (position_ == instances_.size())
    {
      // We're done
      return JobStepResult::Success();
    }
    else
    {
      return JobStepResult::Continue();
    }
  }

    
  void SetOfInstancesJob::GetPublicContent(Json::Value& value)
  {
    value["Description"] = GetDescription();
    value["InstancesCount"] = static_cast<uint32_t>(instances_.size());
    value["FailedInstancesCount"] = static_cast<uint32_t>(failedInstances_.size());
  }    


  bool SetOfInstancesJob::Serialize(Json::Value& value)
  {
    value = Json::objectValue;

    std::string type;
    GetJobType(type);
    value["Type"] = type;

    value["Permissive"] = permissive_;
    value["Position"] = static_cast<unsigned int>(position_);
    value["Description"] = description_;

    SerializationToolbox::WriteArrayOfStrings(value, instances_, "Instances");
    SerializationToolbox::WriteSetOfStrings(value, failedInstances_, "FailedInstances");

    return true;
  }


  SetOfInstancesJob::SetOfInstancesJob(const Json::Value& value) :
    started_(false),
    permissive_(SerializationToolbox::ReadBoolean(value, "Permissive")),
    position_(SerializationToolbox::ReadUnsignedInteger(value, "Position")),
    description_(SerializationToolbox::ReadString(value, "Description"))
  {
    SerializationToolbox::ReadArrayOfStrings(instances_, value, "Instances");
    SerializationToolbox::ReadSetOfStrings(failedInstances_, value, "FailedInstances");

    if (position_ > instances_.size())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
}
