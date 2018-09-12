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


#pragma once

#include "OrthancPluginException.h"

#include <orthanc/OrthancCPlugin.h>
#include <boost/noncopyable.hpp>
#include <boost/lexical_cast.hpp>
#include <json/value.h>
#include <list>
#include <set>
#include <map>



#if !defined(ORTHANC_PLUGINS_VERSION_IS_ABOVE)
#define ORTHANC_PLUGINS_VERSION_IS_ABOVE(major, minor, revision)        \
  (ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER > major ||                      \
   (ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER == major &&                    \
    (ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER > minor ||                    \
     (ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER == minor &&                  \
      ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER >= revision))))
#endif


#if ORTHANC_PLUGINS_VERSION_IS_ABOVE(1, 2, 0)
// The "OrthancPluginFindMatcher()" primitive was introduced in Orthanc 1.2.0
#  define HAS_ORTHANC_PLUGIN_FIND_MATCHER  1
#else
#  define HAS_ORTHANC_PLUGIN_FIND_MATCHER  0
#endif


#if ORTHANC_PLUGINS_VERSION_IS_ABOVE(1, 4, 2)
#  define HAS_ORTHANC_PLUGIN_PEERS  1
#  define HAS_ORTHANC_PLUGIN_JOB    1
#else
#  define HAS_ORTHANC_PLUGIN_PEERS  0
#  define HAS_ORTHANC_PLUGIN_JOB    0
#endif



namespace OrthancPlugins
{
  typedef void (*RestCallback) (OrthancPluginRestOutput* output,
                                const char* url,
                                const OrthancPluginHttpRequest* request);

  class MemoryBuffer : public boost::noncopyable
  {
  private:
    OrthancPluginContext*      context_;
    OrthancPluginMemoryBuffer  buffer_;

    void Check(OrthancPluginErrorCode code);

    bool CheckHttp(OrthancPluginErrorCode code);

  public:
    MemoryBuffer(OrthancPluginContext* context);

    ~MemoryBuffer()
    {
      Clear();
    }

    OrthancPluginMemoryBuffer* operator*()
    {
      return &buffer_;
    }

    // This transfers ownership from "other" to "this"
    void Assign(OrthancPluginMemoryBuffer& other);

    OrthancPluginMemoryBuffer Release();

    const char* GetData() const
    {
      if (buffer_.size > 0)
      {
        return reinterpret_cast<const char*>(buffer_.data);
      }
      else
      {
        return NULL;
      }
    }

    size_t GetSize() const
    {
      return buffer_.size;
    }

    void Clear();

    void ToString(std::string& target) const;

    void ToJson(Json::Value& target) const;

    bool RestApiGet(const std::string& uri,
                    bool applyPlugins);

    bool RestApiPost(const std::string& uri,
                     const char* body,
                     size_t bodySize,
                     bool applyPlugins);

    bool RestApiPut(const std::string& uri,
                    const char* body,
                    size_t bodySize,
                    bool applyPlugins);

    bool RestApiPost(const std::string& uri,
                     const Json::Value& body,
                     bool applyPlugins);

    bool RestApiPut(const std::string& uri,
                    const Json::Value& body,
                    bool applyPlugins);

    bool RestApiPost(const std::string& uri,
                     const std::string& body,
                     bool applyPlugins)
    {
      return RestApiPost(uri, body.empty() ? NULL : body.c_str(), body.size(), applyPlugins);
    }

    bool RestApiPut(const std::string& uri,
                    const std::string& body,
                    bool applyPlugins)
    {
      return RestApiPut(uri, body.empty() ? NULL : body.c_str(), body.size(), applyPlugins);
    }

    void CreateDicom(const Json::Value& tags,
                     OrthancPluginCreateDicomFlags flags);

    void ReadFile(const std::string& path);

    void GetDicomQuery(const OrthancPluginWorklistQuery* query);

    void DicomToJson(Json::Value& target,
                     OrthancPluginDicomToJsonFormat format,
                     OrthancPluginDicomToJsonFlags flags,
                     uint32_t maxStringLength);

    bool HttpGet(const std::string& url,
                 const std::string& username,
                 const std::string& password);
 
    bool HttpPost(const std::string& url,
                  const std::string& body,
                  const std::string& username,
                  const std::string& password);
 
    bool HttpPut(const std::string& url,
                 const std::string& body,
                 const std::string& username,
                 const std::string& password);

    void GetDicomInstance(const std::string& instanceId);
  };


  class OrthancString : public boost::noncopyable
  {
  private:
    OrthancPluginContext*  context_;
    char*                  str_;

    void Clear();

  public:
    OrthancString(OrthancPluginContext* context) :
      context_(context),
      str_(NULL)
    {
    }

    ~OrthancString()
    {
      Clear();
    }

    // This transfers ownership, warning: The string must have been
    // allocated by the Orthanc core
    void Assign(char* str);

    const char* GetContent() const
    {
      return str_;
    }

    void ToString(std::string& target) const;

    void ToJson(Json::Value& target) const;
  };


  class OrthancConfiguration : public boost::noncopyable
  {
  private:
    OrthancPluginContext*  context_;
    Json::Value            configuration_;  // Necessarily a Json::objectValue
    std::string            path_;

    std::string GetPath(const std::string& key) const;

  public:
    OrthancConfiguration() : context_(NULL)
    {
    }

    OrthancConfiguration(OrthancPluginContext* context);

    OrthancPluginContext* GetContext() const;

    const Json::Value& GetJson() const
    {
      return configuration_;
    }

    bool IsSection(const std::string& key) const;

    void GetSection(OrthancConfiguration& target,
                    const std::string& key) const;

    bool LookupStringValue(std::string& target,
                           const std::string& key) const;
    
    bool LookupIntegerValue(int& target,
                            const std::string& key) const;

    bool LookupUnsignedIntegerValue(unsigned int& target,
                                    const std::string& key) const;

    bool LookupBooleanValue(bool& target,
                            const std::string& key) const;

    bool LookupFloatValue(float& target,
                          const std::string& key) const;

    bool LookupListOfStrings(std::list<std::string>& target,
                             const std::string& key,
                             bool allowSingleString) const;

    bool LookupSetOfStrings(std::set<std::string>& target,
                            const std::string& key,
                            bool allowSingleString) const;

    std::string GetStringValue(const std::string& key,
                               const std::string& defaultValue) const;

    int GetIntegerValue(const std::string& key,
                        int defaultValue) const;

    unsigned int GetUnsignedIntegerValue(const std::string& key,
                                         unsigned int defaultValue) const;

    bool GetBooleanValue(const std::string& key,
                         bool defaultValue) const;

    float GetFloatValue(const std::string& key,
                        float defaultValue) const;

    void GetDictionary(std::map<std::string, std::string>& target,
                       const std::string& key) const;
  };

  class OrthancImage : public boost::noncopyable
  {
  private:
    OrthancPluginContext*  context_;
    OrthancPluginImage*    image_;

    void Clear();

    void CheckImageAvailable();

  public:
    OrthancImage(OrthancPluginContext*  context);

    OrthancImage(OrthancPluginContext*  context,
                 OrthancPluginImage*    image);

    OrthancImage(OrthancPluginContext*     context,
                 OrthancPluginPixelFormat  format,
                 uint32_t                  width,
                 uint32_t                  height);

    ~OrthancImage()
    {
      Clear();
    }

    void UncompressPngImage(const void* data,
                            size_t size);

    void UncompressJpegImage(const void* data,
                             size_t size);

    void DecodeDicomImage(const void* data,
                          size_t size,
                          unsigned int frame);

    OrthancPluginPixelFormat GetPixelFormat();

    unsigned int GetWidth();

    unsigned int GetHeight();

    unsigned int GetPitch();
    
    const void* GetBuffer();

    void CompressPngImage(MemoryBuffer& target);

    void CompressJpegImage(MemoryBuffer& target,
                           uint8_t quality);

    void AnswerPngImage(OrthancPluginRestOutput* output);

    void AnswerJpegImage(OrthancPluginRestOutput* output,
                         uint8_t quality);
  };


#if HAS_ORTHANC_PLUGIN_FIND_MATCHER == 1
  class FindMatcher : public boost::noncopyable
  {
  private:
    OrthancPluginContext*              context_;
    OrthancPluginFindMatcher*          matcher_;
    const OrthancPluginWorklistQuery*  worklist_;

    void SetupDicom(OrthancPluginContext*  context,
                    const void*            query,
                    uint32_t               size);

  public:
    FindMatcher(OrthancPluginContext*              context,
                const OrthancPluginWorklistQuery*  worklist);

    FindMatcher(OrthancPluginContext*  context,
                const void*            query,
                uint32_t               size)
    {
      SetupDicom(context, query, size);
    }

    FindMatcher(OrthancPluginContext*  context,
                const MemoryBuffer&    dicom)
    {
      SetupDicom(context, dicom.GetData(), dicom.GetSize());
    }

    ~FindMatcher();

    bool IsMatch(const void*  dicom,
                 uint32_t     size) const;

    bool IsMatch(const MemoryBuffer& dicom) const
    {
      return IsMatch(dicom.GetData(), dicom.GetSize());
    }
  };
#endif


  bool RestApiGet(Json::Value& result,
                  OrthancPluginContext* context,
                  const std::string& uri,
                  bool applyPlugins);

  bool RestApiPost(Json::Value& result,
                   OrthancPluginContext* context,
                   const std::string& uri,
                   const char* body,
                   size_t bodySize,
                   bool applyPlugins);

  bool RestApiPost(Json::Value& result,
                   OrthancPluginContext* context,
                   const std::string& uri,
                   const Json::Value& body,
                   bool applyPlugins);

  inline bool RestApiPost(Json::Value& result,
                          OrthancPluginContext* context,
                          const std::string& uri,
                          const std::string& body,
                          bool applyPlugins)
  {
    return RestApiPost(result, context, uri, body.empty() ? NULL : body.c_str(), 
                       body.size(), applyPlugins);
  }

  bool RestApiPut(Json::Value& result,
                  OrthancPluginContext* context,
                  const std::string& uri,
                  const char* body,
                  size_t bodySize,
                  bool applyPlugins);

  bool RestApiPut(Json::Value& result,
                  OrthancPluginContext* context,
                  const std::string& uri,
                  const Json::Value& body,
                  bool applyPlugins);

  inline bool RestApiPut(Json::Value& result,
                         OrthancPluginContext* context,
                         const std::string& uri,
                         const std::string& body,
                         bool applyPlugins)
  {
    return RestApiPut(result, context, uri, body.empty() ? NULL : body.c_str(), 
                      body.size(), applyPlugins);
  }

  bool RestApiDelete(OrthancPluginContext* context,
                     const std::string& uri,
                     bool applyPlugins);

  bool HttpDelete(OrthancPluginContext* context,
                  const std::string& url,
                  const std::string& username,
                  const std::string& password);

  inline void LogError(OrthancPluginContext* context,
                       const std::string& message)
  {
    if (context != NULL)
    {
      OrthancPluginLogError(context, message.c_str());
    }
  }

  inline void LogWarning(OrthancPluginContext* context,
                         const std::string& message)
  {
    if (context != NULL)
    {
      OrthancPluginLogWarning(context, message.c_str());
    }
  }

  inline void LogInfo(OrthancPluginContext* context,
                      const std::string& message)
  {
    if (context != NULL)
    {
      OrthancPluginLogInfo(context, message.c_str());
    }
  }

  void ReportMinimalOrthancVersion(OrthancPluginContext* context,
                                   unsigned int major,
                                   unsigned int minor,
                                   unsigned int revision);
  
  bool CheckMinimalOrthancVersion(OrthancPluginContext* context,
                                  unsigned int major,
                                  unsigned int minor,
                                  unsigned int revision);


  namespace Internals
  {
    template <RestCallback Callback>
    OrthancPluginErrorCode Protect(OrthancPluginRestOutput* output,
                                   const char* url,
                                   const OrthancPluginHttpRequest* request)
    {
      try
      {
        Callback(output, url, request);
        return OrthancPluginErrorCode_Success;
      }
      catch (ORTHANC_PLUGINS_EXCEPTION_CLASS& e)
      {
        return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
      }
      catch (boost::bad_lexical_cast&)
      {
        return OrthancPluginErrorCode_BadFileFormat;
      }
      catch (...)
      {
        return OrthancPluginErrorCode_Plugin;
      }
    }
  }

  
  template <RestCallback Callback>
  void RegisterRestCallback(OrthancPluginContext* context,
                            const std::string& uri,
                            bool isThreadSafe)
  {
    if (isThreadSafe)
    {
      OrthancPluginRegisterRestCallbackNoLock(context, uri.c_str(), Internals::Protect<Callback>);
    }
    else
    {
      OrthancPluginRegisterRestCallback(context, uri.c_str(), Internals::Protect<Callback>);
    }
  }


#if HAS_ORTHANC_PLUGIN_PEERS == 1
  class OrthancPeers : public boost::noncopyable
  {
  private:
    typedef std::map<std::string, uint32_t>   Index;

    OrthancPluginContext *context_;
    OrthancPluginPeers   *peers_;
    Index                 index_;
    uint32_t              timeout_;

  public:
    OrthancPeers(OrthancPluginContext* context);

    ~OrthancPeers();

    uint32_t GetTimeout() const
    {
      return timeout_;
    }

    void SetTimeout(uint32_t timeout)
    {
      timeout_ = timeout;
    }

    bool LookupName(size_t& target,
                    const std::string& name) const;

    std::string GetPeerName(size_t index) const;

    std::string GetPeerUrl(size_t index) const;

    std::string GetPeerUrl(const std::string& name) const;

    size_t GetPeersCount() const
    {
      return index_.size();
    }

    bool DoGet(MemoryBuffer& target,
               size_t index,
               const std::string& uri) const;

    bool DoGet(MemoryBuffer& target,
               const std::string& name,
               const std::string& uri) const;

    bool DoGet(Json::Value& target,
               size_t index,
               const std::string& uri) const;
      
    bool DoGet(Json::Value& target,
               const std::string& name,
               const std::string& uri) const;
      
    bool DoPost(MemoryBuffer& target,
                size_t index,
                const std::string& uri,
                const std::string& body) const;

    bool DoPost(MemoryBuffer& target,
                const std::string& name,
                const std::string& uri,
                const std::string& body) const;

    bool DoPost(Json::Value& target,
                size_t index,
                const std::string& uri,
                const std::string& body) const;
      
    bool DoPost(Json::Value& target,
                const std::string& name,
                const std::string& uri,
                const std::string& body) const;

    bool DoPut(size_t index,
               const std::string& uri,
               const std::string& body) const;

    bool DoPut(const std::string& name,
               const std::string& uri,
               const std::string& body) const;

    bool DoDelete(size_t index,
                  const std::string& uri) const;

    bool DoDelete(const std::string& name,
                  const std::string& uri) const;
  };
#endif



#if HAS_ORTHANC_PLUGIN_JOB == 1
  class OrthancJob : public boost::noncopyable
  {
  private:
    std::string   jobType_;
    std::string   content_;
    bool          hasSerialized_;
    std::string   serialized_;
    float         progress_;

    static void CallbackFinalize(void* job);

    static float CallbackGetProgress(void* job);

    static const char* CallbackGetContent(void* job);

    static const char* CallbackGetSerialized(void* job);

    static OrthancPluginJobStepStatus CallbackStep(void* job);

    static OrthancPluginErrorCode CallbackStop(void* job,
                                               OrthancPluginJobStopReason reason);

    static OrthancPluginErrorCode CallbackReset(void* job);

  protected:
    void ClearContent();

    void UpdateContent(const Json::Value& content);

    void ClearSerialized();

    void UpdateSerialized(const Json::Value& serialized);

    void UpdateProgress(float progress);
    
  public:
    OrthancJob(const std::string& jobType);
    
    virtual ~OrthancJob()
    {
    }

    virtual OrthancPluginJobStepStatus Step() = 0;

    virtual void Stop(OrthancPluginJobStopReason reason) = 0;
    
    virtual void Reset() = 0;

    static OrthancPluginJob* Create(OrthancPluginContext* context,
                                    OrthancJob* job /* takes ownership */);

    static std::string Submit(OrthancPluginContext* context,
                              OrthancJob* job /* takes ownership */,
                              int priority);
  };
#endif
}
