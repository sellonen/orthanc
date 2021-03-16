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

#include "IDatabaseWrapper.h"

#include "../../../OrthancFramework/Sources/SQLite/Connection.h"

#include <boost/thread/mutex.hpp>

namespace Orthanc
{
  /**
   * This class manages an instance of the Orthanc SQLite database. It
   * translates low-level requests into SQL statements. Mutual
   * exclusion MUST be implemented at a higher level.
   **/
  class SQLiteDatabaseWrapper : public IDatabaseWrapper
  {
  private:
    class TransactionBase;
    class SignalFileDeleted;
    class SignalResourceDeleted;
    class SignalRemainingAncestor;
    class ReadOnlyTransaction;
    class ReadWriteTransaction;
    class LookupFormatter;

    boost::mutex              mutex_;
    SQLite::Connection        db_;
    TransactionBase*          activeTransaction_;
    SignalRemainingAncestor*  signalRemainingAncestor_;
    unsigned int              version_;

    void GetChangesInternal(std::list<ServerIndexChange>& target,
                            bool& done,
                            SQLite::Statement& s,
                            uint32_t maxResults);

    void GetExportedResourcesInternal(std::list<ExportedResource>& target,
                                      bool& done,
                                      SQLite::Statement& s,
                                      uint32_t maxResults);

  public:
    SQLiteDatabaseWrapper(const std::string& path);

    SQLiteDatabaseWrapper();

    virtual ~SQLiteDatabaseWrapper();

    virtual void Open() ORTHANC_OVERRIDE;

    virtual void Close() ORTHANC_OVERRIDE;

    virtual IDatabaseWrapper::ITransaction* StartTransaction(TransactionType type,
                                                             IDatabaseListener& listener)
      ORTHANC_OVERRIDE;

    virtual void FlushToDisk() ORTHANC_OVERRIDE;

    virtual bool HasFlushToDisk() const ORTHANC_OVERRIDE
    {
      return true;
    }

    virtual unsigned int GetDatabaseVersion() ORTHANC_OVERRIDE
    {
      return version_;
    }

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea) ORTHANC_OVERRIDE;



    /**
     * The "StartTransaction()" method is guaranteed to return a class
     * derived from "UnitTestsTransaction". The methods of
     * "UnitTestsTransaction" give access to additional information
     * about the underlying SQLite database to be used in unit tests.
     **/
    class UnitTestsTransaction : public ITransaction
    {
    protected:
      SQLite::Connection& db_;
      
    public:
      UnitTestsTransaction(SQLite::Connection& db) :
        db_(db)
      {
      }
      
      void GetChildren(std::list<std::string>& childrenPublicIds,
                       int64_t id);

      int64_t GetTableRecordCount(const std::string& table);
    
      bool GetParentPublicId(std::string& target,
                             int64_t id);

      int64_t CreateResource(const std::string& publicId,
                             ResourceType type);

      void AttachChild(int64_t parent,
                       int64_t child);

      void SetIdentifierTag(int64_t id,
                            const DicomTag& tag,
                            const std::string& value);

      void SetMainDicomTag(int64_t id,
                           const DicomTag& tag,
                           const std::string& value);
    };
  };
}
