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
#include "OrthancRestApi.h"

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/Logging.h"
#include "../../Core/SerializationToolbox.h"
#include "../OrthancInitialization.h"
#include "../QueryRetrieveHandler.h"
#include "../ServerJobs/DicomModalityStoreJob.h"
#include "../ServerJobs/DicomMoveScuJob.h"
#include "../ServerJobs/OrthancPeerStoreJob.h"
#include "../ServerToolbox.h"


namespace Orthanc
{
  /***************************************************************************
   * DICOM C-Echo SCU
   ***************************************************************************/

  static void DicomEcho(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    try
    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      
      if (connection.Echo())
      {
        // Echo has succeeded
        call.GetOutput().AnswerBuffer("{}", "application/json");
        return;
      }
    }
    catch (OrthancException&)
    {
    }

    // Echo has failed
    call.GetOutput().SignalError(HttpStatus_500_InternalServerError);
  }



  /***************************************************************************
   * DICOM C-Find SCU => DEPRECATED!
   ***************************************************************************/

  static bool MergeQueryAndTemplate(DicomMap& result,
                                    const char* postData,
                                    size_t postSize)
  {
    Json::Value query;
    Json::Reader reader;

    if (!reader.parse(postData, postData + postSize, query) ||
        query.type() != Json::objectValue)
    {
      return false;
    }

    Json::Value::Members members = query.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      DicomTag t = FromDcmtkBridge::ParseTag(members[i]);
      result.SetValue(t, query[members[i]].asString(), false);
    }

    return true;
  }


  static void FindPatient(DicomFindAnswers& result,
                          DicomUserConnection& connection,
                          const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the patient
    DicomMap s;
    fields.ExtractPatientInformation(s);
    connection.Find(result, ResourceType_Patient, s);
  }


  static void FindStudy(DicomFindAnswers& result,
                        DicomUserConnection& connection,
                        const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the study
    DicomMap s;
    fields.ExtractStudyInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_MODALITIES_IN_STUDY);

    connection.Find(result, ResourceType_Study, s);
  }

  static void FindSeries(DicomFindAnswers& result,
                         DicomUserConnection& connection,
                         const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the series
    DicomMap s;
    fields.ExtractSeriesInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_STUDY_INSTANCE_UID);

    connection.Find(result, ResourceType_Series, s);
  }

  static void FindInstance(DicomFindAnswers& result,
                           DicomUserConnection& connection,
                           const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the instance
    DicomMap s;
    fields.ExtractInstanceInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_STUDY_INSTANCE_UID);
    s.CopyTagIfExists(fields, DICOM_TAG_SERIES_INSTANCE_UID);

    connection.Find(result, ResourceType_Instance, s);
  }


  static void DicomFindPatient(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindPatientTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call.GetBodyData(), call.GetBodySize()))
    {
      return;
    }

    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    
    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindPatient(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindStudy(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindStudyTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call.GetBodyData(), call.GetBodySize()))
    {
      return;
    }

    if (fields.GetValue(DICOM_TAG_ACCESSION_NUMBER).GetContent().size() <= 2 &&
        fields.GetValue(DICOM_TAG_PATIENT_ID).GetContent().size() <= 2)
    {
      return;
    }        
      
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindStudy(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindSeries(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindSeriesTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call.GetBodyData(), call.GetBodySize()))
    {
      return;
    }

    if ((fields.GetValue(DICOM_TAG_ACCESSION_NUMBER).GetContent().size() <= 2 &&
         fields.GetValue(DICOM_TAG_PATIENT_ID).GetContent().size() <= 2) ||
        fields.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent().size() <= 2)
    {
      return;
    }        
         
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindSeries(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindInstance(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindInstanceTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call.GetBodyData(), call.GetBodySize()))
    {
      return;
    }

    if ((fields.GetValue(DICOM_TAG_ACCESSION_NUMBER).GetContent().size() <= 2 &&
         fields.GetValue(DICOM_TAG_PATIENT_ID).GetContent().size() <= 2) ||
        fields.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent().size() <= 2 ||
        fields.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).GetContent().size() <= 2)
    {
      return;
    }        
         
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindInstance(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }


  static void CopyTagIfExists(DicomMap& target,
                              ParsedDicomFile& source,
                              const DicomTag& tag)
  {
    std::string tmp;
    if (source.GetTagValue(tmp, tag))
    {
      target.SetValue(tag, tmp, false);
    }
  }


  static void DicomFind(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call.GetBodyData(), call.GetBodySize()))
    {
      return;
    }
 
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomUserConnection connection(localAet, remote);
    connection.Open();
    
    DicomFindAnswers patients(false);
    FindPatient(patients, connection, m);

    // Loop over the found patients
    Json::Value result = Json::arrayValue;
    for (size_t i = 0; i < patients.GetSize(); i++)
    {
      Json::Value patient;
      patients.ToJson(patient, i, true);

      DicomMap::SetupFindStudyTemplate(m);
      if (!MergeQueryAndTemplate(m, call.GetBodyData(), call.GetBodySize()))
      {
        return;
      }

      CopyTagIfExists(m, patients.GetAnswer(i), DICOM_TAG_PATIENT_ID);

      DicomFindAnswers studies(false);
      FindStudy(studies, connection, m);

      patient["Studies"] = Json::arrayValue;
      
      // Loop over the found studies
      for (size_t j = 0; j < studies.GetSize(); j++)
      {
        Json::Value study;
        studies.ToJson(study, j, true);

        DicomMap::SetupFindSeriesTemplate(m);
        if (!MergeQueryAndTemplate(m, call.GetBodyData(), call.GetBodySize()))
        {
          return;
        }

        CopyTagIfExists(m, studies.GetAnswer(j), DICOM_TAG_PATIENT_ID);
        CopyTagIfExists(m, studies.GetAnswer(j), DICOM_TAG_STUDY_INSTANCE_UID);

        DicomFindAnswers series(false);
        FindSeries(series, connection, m);

        // Loop over the found series
        study["Series"] = Json::arrayValue;
        for (size_t k = 0; k < series.GetSize(); k++)
        {
          Json::Value series2;
          series.ToJson(series2, k, true);
          study["Series"].append(series2);
        }

        patient["Studies"].append(study);
      }

      result.append(patient);
    }
    
    call.GetOutput().AnswerJson(result);
  }



  /***************************************************************************
   * DICOM C-Find and C-Move SCU => Recommended since Orthanc 0.9.0
   ***************************************************************************/

  static void DicomQuery(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    Json::Value request;

    if (call.ParseJsonRequest(request) &&
        request.type() == Json::objectValue &&
        request.isMember("Level") && request["Level"].type() == Json::stringValue &&
        (!request.isMember("Query") || request["Query"].type() == Json::objectValue))
    {
      std::auto_ptr<QueryRetrieveHandler>  handler(new QueryRetrieveHandler(context));

      handler->SetModality(call.GetUriComponent("id", ""));
      handler->SetLevel(StringToResourceType(request["Level"].asCString()));

      if (request.isMember("Query"))
      {
        Json::Value::Members tags = request["Query"].getMemberNames();
        for (size_t i = 0; i < tags.size(); i++)
        {
          handler->SetQuery(FromDcmtkBridge::ParseTag(tags[i].c_str()),
                            request["Query"][tags[i]].asString());
        }
      }

      handler->Run();

      std::string s = context.GetQueryRetrieveArchive().Add(handler.release());
      Json::Value result = Json::objectValue;
      result["ID"] = s;
      result["Path"] = "/queries/" + s;
      call.GetOutput().AnswerJson(result);
    }
  }


  static void ListQueries(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::list<std::string> queries;
    context.GetQueryRetrieveArchive().List(queries);

    Json::Value result = Json::arrayValue;
    for (std::list<std::string>::const_iterator
           it = queries.begin(); it != queries.end(); ++it)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }


  namespace
  {
    class QueryAccessor
    {
    private:
      ServerContext&            context_;
      SharedArchive::Accessor   accessor_;
      QueryRetrieveHandler&     handler_;

    public:
      QueryAccessor(RestApiCall& call) :
        context_(OrthancRestApi::GetContext(call)),
        accessor_(context_.GetQueryRetrieveArchive(), call.GetUriComponent("id", "")),
        handler_(dynamic_cast<QueryRetrieveHandler&>(accessor_.GetItem()))
      {
      }                     

      QueryRetrieveHandler& GetHandler() const
      {
        return handler_;
      }
    };

    static void AnswerDicomMap(RestApiCall& call,
                               const DicomMap& value,
                               bool simplify)
    {
      Json::Value full = Json::objectValue;
      FromDcmtkBridge::ToJson(full, value, simplify);
      call.GetOutput().AnswerJson(full);
    }
  }


  static void ListQueryAnswers(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    size_t count = query.GetHandler().GetAnswersCount();

    Json::Value result = Json::arrayValue;
    for (size_t i = 0; i < count; i++)
    {
      result.append(boost::lexical_cast<std::string>(i));
    }

    call.GetOutput().AnswerJson(result);
  }


  static void GetQueryOneAnswer(RestApiGetCall& call)
  {
    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));

    QueryAccessor query(call);

    DicomMap map;
    query.GetHandler().GetAnswer(map, index);

    AnswerDicomMap(call, map, call.HasArgument("simplify"));
  }


  static void SubmitRetrieveJob(RestApiPostCall& call,
                                bool allAnswers,
                                size_t index)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string targetAet;
    
    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      targetAet = SerializationToolbox::ReadString(body, "TargetAet");
    }
    else
    {
      body = Json::objectValue;
      call.BodyToString(targetAet);
    }
    
    std::auto_ptr<DicomMoveScuJob> job(new DicomMoveScuJob(context));
    
    {
      QueryAccessor query(call);
      job->SetTargetAet(targetAet);
      job->SetLocalAet(query.GetHandler().GetLocalAet());
      job->SetRemoteModality(query.GetHandler().GetRemoteModality());

      LOG(WARNING) << "Driving C-Move SCU on remote modality "
                   << query.GetHandler().GetRemoteModality().GetApplicationEntityTitle()
                   << " to target modality " << targetAet;

      if (allAnswers)
      {
        for (size_t i = 0; i < query.GetHandler().GetAnswersCount(); i++)
        {
          job->AddFindAnswer(query.GetHandler(), i);
        }
      }
      else
      {
        job->AddFindAnswer(query.GetHandler(), index);
      }
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, body);
  }
  

  static void RetrieveOneAnswer(RestApiPostCall& call)
  {
    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));
    SubmitRetrieveJob(call, false, index);
  }


  static void RetrieveAllAnswers(RestApiPostCall& call)
  {
    SubmitRetrieveJob(call, true, 0);
  }


  static void GetQueryArguments(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    AnswerDicomMap(call, query.GetHandler().GetQuery(), call.HasArgument("simplify"));
  }


  static void GetQueryLevel(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    call.GetOutput().AnswerBuffer(EnumerationToString(query.GetHandler().GetLevel()), "text/plain");
  }


  static void GetQueryModality(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    call.GetOutput().AnswerBuffer(query.GetHandler().GetModalitySymbolicName(), "text/plain");
  }


  static void DeleteQuery(RestApiDeleteCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    context.GetQueryRetrieveArchive().Remove(call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", "text/plain");
  }


  static void ListQueryOperations(RestApiGetCall& call)
  {
    // Ensure that the query of interest does exist
    QueryAccessor query(call);  

    RestApi::AutoListChildren(call);
  }


  static void ListQueryAnswerOperations(RestApiGetCall& call)
  {
    // Ensure that the query of interest does exist
    QueryAccessor query(call);

    // Ensure that the answer of interest does exist
    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));

    DicomMap map;
    query.GetHandler().GetAnswer(map, index);

    RestApi::AutoListChildren(call);
  }




  /***************************************************************************
   * DICOM C-Store SCU
   ***************************************************************************/

  static bool GetInstancesToExport(Json::Value& otherArguments,
                                   SetOfInstancesJob& job,
                                   const std::string& remote,
                                   RestApiPostCall& call)
  {
    otherArguments = Json::objectValue;
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (Toolbox::IsSHA1(call.GetBodyData(), call.GetBodySize()))
    {
      std::string s;
      call.BodyToString(s);

      // This is for compatibility with Orthanc <= 0.5.1.
      request = Json::arrayValue;
      request.append(Toolbox::StripSpaces(s));
    }
    else if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      return false;
    }

    if (request.isString())
    {
      std::string item = request.asString();
      request = Json::arrayValue;
      request.append(item);
    }

    const Json::Value* resources;
    if (request.isArray())
    {
      resources = &request;
    }
    else
    {
      if (request.type() != Json::objectValue ||
          !request.isMember("Resources"))
      {
        return false;
      }

      resources = &request["Resources"];
      if (!resources->isArray())
      {
        return false;
      }

      // Copy the remaining arguments
      Json::Value::Members members = request.getMemberNames();
      for (Json::Value::ArrayIndex i = 0; i < members.size(); i++)
      {
        otherArguments[members[i]] = request[members[i]];
      }
    }

    for (Json::Value::ArrayIndex i = 0; i < resources->size(); i++)
    {
      if (!(*resources) [i].isString())
      {
        return false;
      }

      std::string stripped = Toolbox::StripSpaces((*resources) [i].asString());
      if (!Toolbox::IsSHA1(stripped))
      {
        return false;
      }

      if (Configuration::GetGlobalBoolParameter("LogExportedResources", false))
      {
        context.GetIndex().LogExportedResource(stripped, remote);
      }

      context.AddChildInstances(job, stripped);
    }

    return true;
  }


  static void SubmitJob(RestApiPostCall& call,
                        const Json::Value& request,
                        SetOfInstancesJob* jobRaw)
  {
    std::auto_ptr<SetOfInstancesJob> job(jobRaw);
    
    if (job.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    bool permissive = Toolbox::GetJsonBooleanField(request, "Permissive", false);
    bool asynchronous = Toolbox::GetJsonBooleanField(request, "Asynchronous", false);
    int priority = Toolbox::GetJsonIntegerField(request, "Priority", 0);

    job->SetPermissive(permissive);
    
    Json::Value publicContent;

    if (asynchronous)
    {
      // Asynchronous mode: Submit the job, but don't wait for its completion
      std::string id;
      context.GetJobsEngine().GetRegistry().Submit(id, job.release(), priority);

      Json::Value v;
      v["ID"] = id;
      call.GetOutput().AnswerJson(v);
    }
    else if (context.GetJobsEngine().GetRegistry().SubmitAndWait
             (publicContent, job.release(), priority))
    {
      // Synchronous mode: We have submitted and waited for completion
      call.GetOutput().AnswerBuffer("{}", "application/json");
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_500_InternalServerError);
    }
  }


  static void DicomStore(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    Json::Value request;
    std::auto_ptr<DicomModalityStoreJob> job(new DicomModalityStoreJob(context));

    if (GetInstancesToExport(request, *job, remote, call))
    {
      std::string localAet = Toolbox::GetJsonStringField
        (request, "LocalAet", context.GetDefaultLocalApplicationEntityTitle());
      std::string moveOriginatorAET = Toolbox::GetJsonStringField
        (request, "MoveOriginatorAet", context.GetDefaultLocalApplicationEntityTitle());
      int moveOriginatorID = Toolbox::GetJsonIntegerField
        (request, "MoveOriginatorID", 0 /* By default, not a C-MOVE */);

      RemoteModalityParameters p = Configuration::GetModalityUsingSymbolicName(remote);

      job->SetDescription("REST API");
      job->SetLocalAet(localAet);
      job->SetRemoteModality(p);

      if (moveOriginatorID != 0)
      {
        job->SetMoveOriginator(moveOriginatorAET, moveOriginatorID);
      }

      SubmitJob(call, request, job.release());
    }
  }


  /***************************************************************************
   * DICOM C-Move SCU
   ***************************************************************************/
  
  static void DicomMove(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;

    static const char* RESOURCES = "Resources";
    static const char* LEVEL = "Level";

    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue ||
        !request.isMember(RESOURCES) ||
        !request.isMember(LEVEL) ||
        request[RESOURCES].type() != Json::arrayValue ||
        request[LEVEL].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    ResourceType level = StringToResourceType(request["Level"].asCString());
    
    std::string localAet = Toolbox::GetJsonStringField
      (request, "LocalAet", context.GetDefaultLocalApplicationEntityTitle());
    std::string targetAet = Toolbox::GetJsonStringField
      (request, "TargetAet", context.GetDefaultLocalApplicationEntityTitle());

    const RemoteModalityParameters source =
      Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomUserConnection connection(localAet, source);
    connection.Open();
    
    for (Json::Value::ArrayIndex i = 0; i < request[RESOURCES].size(); i++)
    {
      DicomMap resource;
      FromDcmtkBridge::FromJson(resource, request[RESOURCES][i]);
      
      connection.Move(targetAet, level, resource);
    }

    // Move has succeeded
    call.GetOutput().AnswerBuffer("{}", "application/json");
  }



  /***************************************************************************
   * Orthanc Peers => Store client
   ***************************************************************************/

  static bool IsExistingPeer(const OrthancRestApi::SetOfStrings& peers,
                             const std::string& id)
  {
    return peers.find(id) != peers.end();
  }

  static void ListPeers(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings peers;
    Configuration::GetListOfOrthancPeers(peers);

    if (call.HasArgument("expand"))
    {
      Json::Value result = Json::objectValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = peers.begin(); it != peers.end(); ++it)
      {
        WebServiceParameters peer;
        
        if (Configuration::GetOrthancPeer(peer, *it))
        {
          Json::Value jsonPeer = Json::objectValue;
          // only return the minimum information to identify the destination, do not include "security" information like passwords
          jsonPeer["Url"] = peer.GetUrl();
          if (!peer.GetUsername().empty())
          {
            jsonPeer["Username"] = peer.GetUsername();
          }
          result[*it] = jsonPeer;
        }
      }
      call.GetOutput().AnswerJson(result);
    }
    else // if expand is not present, keep backward compatibility and return an array of peers
    {
      Json::Value result = Json::arrayValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = peers.begin(); it != peers.end(); ++it)
      {
        result.append(*it);
      }

      call.GetOutput().AnswerJson(result);
    }
  }

  static void ListPeerOperations(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings peers;
    Configuration::GetListOfOrthancPeers(peers);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingPeer(peers, id))
    {
      RestApi::AutoListChildren(call);
    }
  }

  static void PeerStore(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    Json::Value request;
    std::auto_ptr<OrthancPeerStoreJob> job(new OrthancPeerStoreJob(context));

    if (GetInstancesToExport(request, *job, remote, call))
    {
      WebServiceParameters peer;
      if (Configuration::GetOrthancPeer(peer, remote))
      {
        job->SetDescription("REST API");
        job->SetPeer(peer);    
        SubmitJob(call, request, job.release());
      }
      else
      {
        LOG(ERROR) << "No peer with symbolic name: " << remote;
        throw OrthancException(ErrorCode_UnknownResource);
      }
    }
  }


  // DICOM bridge -------------------------------------------------------------

  static bool IsExistingModality(const OrthancRestApi::SetOfStrings& modalities,
                                 const std::string& id)
  {
    return modalities.find(id) != modalities.end();
  }

  static void ListModalities(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings modalities;
    Configuration::GetListOfDicomModalities(modalities);

    if (call.HasArgument("expand"))
    {
      Json::Value result = Json::objectValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = modalities.begin(); it != modalities.end(); ++it)
      {
        Json::Value modality;
        Configuration::GetModalityUsingSymbolicName(*it).ToJson(modality);

        result[*it] = modality;
      }
      call.GetOutput().AnswerJson(result);
    }
    else // if expand is not present, keep backward compatibility and return an array of modalities ids
    {
      Json::Value result = Json::arrayValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = modalities.begin(); it != modalities.end(); ++it)
      {
        result.append(*it);
      }
      call.GetOutput().AnswerJson(result);
    }
  }


  static void ListModalityOperations(RestApiGetCall& call)
  {
    OrthancRestApi::SetOfStrings modalities;
    Configuration::GetListOfDicomModalities(modalities);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingModality(modalities, id))
    {
      RestApi::AutoListChildren(call);
    }
  }


  static void UpdateModality(RestApiPutCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    Json::Reader reader;
    if (reader.parse(call.GetBodyData(), call.GetBodyData() + call.GetBodySize(), json))
    {
      RemoteModalityParameters modality;
      modality.FromJson(json);
      Configuration::UpdateModality(context, call.GetUriComponent("id", ""), modality);
      call.GetOutput().AnswerBuffer("", "text/plain");
    }
  }


  static void DeleteModality(RestApiDeleteCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Configuration::RemoveModality(context, call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", "text/plain");
  }


  static void UpdatePeer(RestApiPutCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    Json::Reader reader;
    if (reader.parse(call.GetBodyData(), call.GetBodyData() + call.GetBodySize(), json))
    {
      WebServiceParameters peer;
      peer.Unserialize(json);
      Configuration::UpdatePeer(context, call.GetUriComponent("id", ""), peer);
      call.GetOutput().AnswerBuffer("", "text/plain");
    }
  }


  static void DeletePeer(RestApiDeleteCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Configuration::RemovePeer(context, call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", "text/plain");
  }


  static void DicomFindWorklist(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    if (call.ParseJsonRequest(json))
    {
      const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
      RemoteModalityParameters remote =
        Configuration::GetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

      std::auto_ptr<ParsedDicomFile> query(ParsedDicomFile::CreateFromJson(json, static_cast<DicomFromJsonFlags>(0)));

      DicomFindAnswers answers(true);

      {
        DicomUserConnection connection(localAet, remote);
        connection.Open();
        connection.FindWorklist(answers, *query);
      }

      Json::Value result;
      answers.ToJson(result, true);
      call.GetOutput().AnswerJson(result);
    }
  }


  void OrthancRestApi::RegisterModalities()
  {
    Register("/modalities", ListModalities);
    Register("/modalities/{id}", ListModalityOperations);
    Register("/modalities/{id}", UpdateModality);
    Register("/modalities/{id}", DeleteModality);
    Register("/modalities/{id}/echo", DicomEcho);
    Register("/modalities/{id}/find-patient", DicomFindPatient);
    Register("/modalities/{id}/find-study", DicomFindStudy);
    Register("/modalities/{id}/find-series", DicomFindSeries);
    Register("/modalities/{id}/find-instance", DicomFindInstance);
    Register("/modalities/{id}/find", DicomFind);
    Register("/modalities/{id}/store", DicomStore);
    Register("/modalities/{id}/move", DicomMove);

    // For Query/Retrieve
    Register("/modalities/{id}/query", DicomQuery);
    Register("/queries", ListQueries);
    Register("/queries/{id}", DeleteQuery);
    Register("/queries/{id}", ListQueryOperations);
    Register("/queries/{id}/answers", ListQueryAnswers);
    Register("/queries/{id}/answers/{index}", ListQueryAnswerOperations);
    Register("/queries/{id}/answers/{index}/content", GetQueryOneAnswer);
    Register("/queries/{id}/answers/{index}/retrieve", RetrieveOneAnswer);
    Register("/queries/{id}/level", GetQueryLevel);
    Register("/queries/{id}/modality", GetQueryModality);
    Register("/queries/{id}/query", GetQueryArguments);
    Register("/queries/{id}/retrieve", RetrieveAllAnswers);

    Register("/peers", ListPeers);
    Register("/peers/{id}", ListPeerOperations);
    Register("/peers/{id}", UpdatePeer);
    Register("/peers/{id}", DeletePeer);
    Register("/peers/{id}/store", PeerStore);

    Register("/modalities/{id}/find-worklist", DicomFindWorklist);
  }
}
