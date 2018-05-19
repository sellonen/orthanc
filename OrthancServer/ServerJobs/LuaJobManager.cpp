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


#include "../PrecompiledHeadersServer.h"
#include "LuaJobManager.h"

#include "DeleteResourceOperation.h"
#include "StorePeerOperation.h"
#include "StoreScuOperation.h"
#include "SystemCallOperation.h"
#include "../../Core/JobsEngine/Operations/LogJobOperation.h"

#include "DicomInstanceOperationValue.h"
#include "../../Core/JobsEngine/Operations/NullOperationValue.h"
#include "../../Core/JobsEngine/Operations/StringOperationValue.h"

namespace Orthanc
{
  void LuaJobManager::ConnectionTimeoutThread(LuaJobManager* manager)
  {
    while (manager->continue_)
    {
      manager->connectionManager_.CheckTimeout();
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }

    
  void LuaJobManager::SignalDone(const SequenceOfOperationsJob& job)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (&job == currentJob_)
    {
      currentId_.clear();
      currentJob_ = NULL;
    }
  }


  LuaJobManager::LuaJobManager() :
    currentJob_(NULL),
    maxOperations_(1000),
    priority_(0),
    continue_(true)
  {
    connectionTimeoutThread_ = boost::thread(ConnectionTimeoutThread, this);
  }


  LuaJobManager::~LuaJobManager()
  {
    continue_ = false;

    if (connectionTimeoutThread_.joinable())
    {
      connectionTimeoutThread_.join();
    }
  }


  void LuaJobManager::SetMaxOperationsPerJob(size_t count)
  {
    boost::mutex::scoped_lock lock(mutex_);
    maxOperations_ = count;
  }


  void LuaJobManager::SetPriority(int priority)
  {
    boost::mutex::scoped_lock lock(mutex_);
    priority_ = priority;
  }


  void LuaJobManager::SetTrailingOperationTimeout(unsigned int timeout)
  {
    boost::mutex::scoped_lock lock(mutex_);
    trailingTimeout_ = timeout;
  }


  LuaJobManager::Lock::Lock(LuaJobManager& that,
                            JobsEngine& engine) :
    that_(that),
    lock_(that.mutex_),
    engine_(engine)
  {
    if (that_.currentJob_ == NULL)
    {
      isNewJob_ = true;
    }
    else
    {
      jobLock_.reset(new SequenceOfOperationsJob::Lock(*that_.currentJob_));

      if (jobLock_->IsDone() ||
          jobLock_->GetOperationsCount() >= that_.maxOperations_)
      {
        jobLock_.reset(NULL);
        isNewJob_ = true;
      }
      else
      {
        isNewJob_ = false;
      }
    }

    if (isNewJob_)
    {
      // Need to create a new job, as the previous one is either
      // finished, or is getting too long
      that_.currentJob_ = new SequenceOfOperationsJob;
      jobLock_.reset(new SequenceOfOperationsJob::Lock(*that_.currentJob_));
      jobLock_->SetTrailingOperationTimeout(that_.trailingTimeout_);
    }

    assert(jobLock_.get() != NULL);
  }


  LuaJobManager::Lock::~Lock()
  {
    assert(jobLock_.get() != NULL);
    jobLock_.reset(NULL);

    if (isNewJob_)
    {
      engine_.GetRegistry().Submit(that_.currentId_, that_.currentJob_, that_.priority_);
    }
  }


  size_t LuaJobManager::Lock::AddDeleteResourceOperation(ServerContext& context)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new DeleteResourceOperation(context));
  }


  size_t LuaJobManager::Lock::AddLogOperation()
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new LogJobOperation);
  }


  size_t LuaJobManager::Lock::AddStoreScuOperation(const std::string& localAet,
                                                   const RemoteModalityParameters& modality)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation
      (new StoreScuOperation(localAet, modality, that_.connectionManager_));    
  }


  size_t LuaJobManager::Lock::AddStorePeerOperation(const WebServiceParameters& peer)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new StorePeerOperation(peer));    
  }


  size_t LuaJobManager::Lock::AddSystemCallOperation(const std::string& command)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation(new SystemCallOperation(command));    
  }
    

  size_t LuaJobManager::Lock::AddSystemCallOperation
  (const std::string& command,
   const std::vector<std::string>& preArguments,
   const std::vector<std::string>& postArguments)
  {
    assert(jobLock_.get() != NULL);
    return jobLock_->AddOperation
      (new SystemCallOperation(command, preArguments, postArguments));
  }


  void LuaJobManager::Lock::AddNullInput(size_t operation)
  {
    assert(jobLock_.get() != NULL);
    jobLock_->AddInput(operation, NullOperationValue());
  }


  void LuaJobManager::Lock::AddStringInput(size_t operation,
                                           const std::string& content)
  {
    assert(jobLock_.get() != NULL);
    jobLock_->AddInput(operation, StringOperationValue(content));
  }


  void LuaJobManager::Lock::AddDicomInstanceInput(size_t operation,
                                                  ServerContext& context,
                                                  const std::string& instanceId)
  {
    assert(jobLock_.get() != NULL);
    jobLock_->AddInput(operation, DicomInstanceOperationValue(context, instanceId));
  }


  void LuaJobManager::Lock::Connect(size_t operation1,
                                    size_t operation2)
  {
    assert(jobLock_.get() != NULL);
    jobLock_->Connect(operation1, operation2);
  }
}
