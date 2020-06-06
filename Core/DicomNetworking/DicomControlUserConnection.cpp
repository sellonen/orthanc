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
#include "DicomControlUserConnection.h"

#include "../Compatibility.h"
#include "../DicomFormat/DicomArray.h"
#include "../DicomParsing/FromDcmtkBridge.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "DicomAssociation.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmnet/diutil.h>

namespace Orthanc
{
  static void TestAndCopyTag(DicomMap& result,
                             const DicomMap& source,
                             const DicomTag& tag)
  {
    if (!source.HasTag(tag))
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
    else
    {
      result.SetValue(tag, source.GetValue(tag));
    }
  }


  namespace
  {
    struct FindPayload
    {
      DicomFindAnswers* answers;
      const char*       level;
      bool              isWorklist;
    };
  }


  static void FindCallback(
    /* in */
    void *callbackData,
    T_DIMSE_C_FindRQ *request,      /* original find request */
    int responseCount,
    T_DIMSE_C_FindRSP *response,    /* pending response received */
    DcmDataset *responseIdentifiers /* pending response identifiers */
    )
  {
    FindPayload& payload = *reinterpret_cast<FindPayload*>(callbackData);

    if (responseIdentifiers != NULL)
    {
      if (payload.isWorklist)
      {
        ParsedDicomFile answer(*responseIdentifiers);
        payload.answers->Add(answer);
      }
      else
      {
        DicomMap m;
        FromDcmtkBridge::ExtractDicomSummary(m, *responseIdentifiers);
        
        if (!m.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
        {
          m.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, payload.level, false);
        }

        payload.answers->Add(m);
      }
    }
  }


  static void NormalizeFindQuery(DicomMap& fixedQuery,
                                 ResourceType level,
                                 const DicomMap& fields)
  {
    std::set<DicomTag> allowedTags;

    // WARNING: Do not add "break" or reorder items in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Instance);

      case ResourceType_Series:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Series);

      case ResourceType_Study:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Study);

      case ResourceType_Patient:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Patient);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    switch (level)
    {
      case ResourceType_Patient:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES);
        break;

      case ResourceType_Study:
        allowedTags.insert(DICOM_TAG_MODALITIES_IN_STUDY);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES);
        allowedTags.insert(DICOM_TAG_SOP_CLASSES_IN_STUDY);
        break;

      case ResourceType_Series:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES);
        break;

      default:
        break;
    }

    allowedTags.insert(DICOM_TAG_SPECIFIC_CHARACTER_SET);

    DicomArray query(fields);
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomTag& tag = query.GetElement(i).GetTag();
      if (allowedTags.find(tag) == allowedTags.end())
      {
        LOG(WARNING) << "Tag not allowed for this C-Find level, will be ignored: " << tag;
      }
      else
      {
        fixedQuery.SetValue(tag, query.GetElement(i).GetValue());
      }
    }
  }



  static ParsedDicomFile* ConvertQueryFields(const DicomMap& fields,
                                             ModalityManufacturer manufacturer)
  {
    // Fix outgoing C-Find requests issue for Syngo.Via and its
    // solution was reported by Emsy Chan by private mail on
    // 2015-06-17. According to Robert van Ommen (2015-11-30), the
    // same fix is required for Agfa Impax. This was generalized for
    // generic manufacturer since it seems to affect PhilipsADW,
    // GEWAServer as well:
    // https://bitbucket.org/sjodogne/orthanc/issues/31/

    switch (manufacturer)
    {
      case ModalityManufacturer_GenericNoWildcardInDates:
      case ModalityManufacturer_GenericNoUniversalWildcard:
      {
        std::unique_ptr<DicomMap> fix(fields.Clone());

        std::set<DicomTag> tags;
        fix->GetTags(tags);

        for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
          // Replace a "*" wildcard query by an empty query ("") for
          // "date" or "all" value representations depending on the
          // type of manufacturer.
          if (manufacturer == ModalityManufacturer_GenericNoUniversalWildcard ||
              (manufacturer == ModalityManufacturer_GenericNoWildcardInDates &&
               FromDcmtkBridge::LookupValueRepresentation(*it) == ValueRepresentation_Date))
          {
            const DicomValue* value = fix->TestAndGetValue(*it);

            if (value != NULL && 
                !value->IsNull() &&
                value->GetContent() == "*")
            {
              fix->SetValue(*it, "", false);
            }
          }
        }

        return new ParsedDicomFile(*fix, GetDefaultDicomEncoding(),
                                   false /* be strict */, "" /* no private creator */);
      }

      default:
        return new ParsedDicomFile(fields, GetDefaultDicomEncoding(),
                                   false /* be strict */, "" /* no private creator */);
    }
  }



  void DicomControlUserConnection::SetupPresentationContexts()
  {
    assert(association_.get() != NULL);
    association_->ProposeGenericPresentationContext(UID_VerificationSOPClass);
    association_->ProposeGenericPresentationContext(UID_FINDPatientRootQueryRetrieveInformationModel);
    association_->ProposeGenericPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel);
    association_->ProposeGenericPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel);
    association_->ProposeGenericPresentationContext(UID_FINDModalityWorklistInformationModel);
  }
    

  void DicomControlUserConnection::FindInternal(DicomFindAnswers& answers,
                                                DcmDataset* dataset,
                                                const char* sopClass,
                                                bool isWorklist,
                                                const char* level)
  {
    assert(isWorklist ^ (level != NULL));
    assert(association_.get() != NULL);

    association_->Open(parameters_);

    FindPayload payload;
    payload.answers = &answers;
    payload.level = level;
    payload.isWorklist = isWorklist;

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(
      &association_->GetDcmtkAssociation(), sopClass);
    if (presID == 0)
    {
      throw OrthancException(ErrorCode_DicomFindUnavailable,
                             "Remote AET is " + parameters_.GetRemoteModality().GetApplicationEntityTitle());
    }

    T_DIMSE_C_FindRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = association_->GetDcmtkAssociation().nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;

    T_DIMSE_C_FindRSP response;
    DcmDataset* statusDetail = NULL;

#if DCMTK_VERSION_NUMBER >= 364
    int responseCount;
#endif

    OFCondition cond = DIMSE_findUser(
      &association_->GetDcmtkAssociation(), presID, &request, dataset,
#if DCMTK_VERSION_NUMBER >= 364
      responseCount,
#endif
      FindCallback, &payload,
      /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
      /*opt_dimse_timeout*/ parameters_.GetTimeout(),
      &response, &statusDetail);
    
    if (statusDetail)
    {
      delete statusDetail;
    }

    DicomAssociation::CheckCondition(cond, parameters_, "C-FIND");

    
    /**
     * New in Orthanc 1.6.0: Deal with failures during C-FIND.
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.html#table_C.4-1
     **/
    
    if (response.DimseStatus != 0x0000 &&  // Success
        response.DimseStatus != 0xFF00 &&  // Pending - Matches are continuing 
        response.DimseStatus != 0xFF01)    // Pending - Matches are continuing 
    {
      char buf[16];
      sprintf(buf, "%04X", response.DimseStatus);

      if (response.DimseStatus == STATUS_FIND_Failed_UnableToProcess)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               HttpStatus_422_UnprocessableEntity,
                               "C-FIND SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf +
                               " (unable to process - invalid query ?)");
      }
      else
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "C-FIND SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf);
      }
    }
  }

    
  void DicomControlUserConnection::MoveInternal(const std::string& targetAet,
                                                ResourceType level,
                                                const DicomMap& fields)
  {
    assert(association_.get() != NULL);
    association_->Open(parameters_);

    std::unique_ptr<ParsedDicomFile> query(
      ConvertQueryFields(fields, parameters_.GetRemoteModality().GetManufacturer()));
    DcmDataset* dataset = query->GetDcmtkObject().getDataset();

    const char* sopClass = UID_MOVEStudyRootQueryRetrieveInformationModel;
    switch (level)
    {
      case ResourceType_Patient:
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "PATIENT");
        break;

      case ResourceType_Study:
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "STUDY");
        break;

      case ResourceType_Series:
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "SERIES");
        break;

      case ResourceType_Instance:
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "IMAGE");
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(&association_->GetDcmtkAssociation(), sopClass);
    if (presID == 0)
    {
      throw OrthancException(ErrorCode_DicomMoveUnavailable,
                             "Remote AET is " + parameters_.GetRemoteModality().GetApplicationEntityTitle());
    }

    T_DIMSE_C_MoveRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = association_->GetDcmtkAssociation().nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;
    strncpy(request.MoveDestination, targetAet.c_str(), DIC_AE_LEN);

    T_DIMSE_C_MoveRSP response;
    DcmDataset* statusDetail = NULL;
    DcmDataset* responseIdentifiers = NULL;
    OFCondition cond = DIMSE_moveUser(
      &association_->GetDcmtkAssociation(), presID, &request, dataset, NULL, NULL,
      /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
      /*opt_dimse_timeout*/ parameters_.GetTimeout(),
      &association_->GetDcmtkNetwork(), NULL, NULL,
      &response, &statusDetail, &responseIdentifiers);

    if (statusDetail)
    {
      delete statusDetail;
    }

    if (responseIdentifiers)
    {
      delete responseIdentifiers;
    }

    DicomAssociation::CheckCondition(cond, parameters_, "C-MOVE");

    
    /**
     * New in Orthanc 1.6.0: Deal with failures during C-MOVE.
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.2.html#table_C.4-2
     **/
    
    if (response.DimseStatus != 0x0000 &&  // Success
        response.DimseStatus != 0xFF00)    // Pending - Sub-operations are continuing
    {
      char buf[16];
      sprintf(buf, "%04X", response.DimseStatus);

      if (response.DimseStatus == STATUS_MOVE_Failed_UnableToProcess)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               HttpStatus_422_UnprocessableEntity,
                               "C-MOVE SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf +
                               " (unable to process - resource not found ?)");
      }
      else
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "C-MOVE SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf);
      }
    }
  }
    

  DicomControlUserConnection::DicomControlUserConnection(const DicomAssociationParameters& params) :
    parameters_(params),
    association_(new DicomAssociation)
  {
    SetupPresentationContexts();
  }
    

  void DicomControlUserConnection::Close()
  {
    assert(association_.get() != NULL);
    association_->Close();
  }


  bool DicomControlUserConnection::Echo()
  {
    assert(association_.get() != NULL);
    association_->Open(parameters_);

    DIC_US status;
    DicomAssociation::CheckCondition(
      DIMSE_echoUser(&association_->GetDcmtkAssociation(),
                     association_->GetDcmtkAssociation().nextMsgID++, 
                     /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                     /*opt_dimse_timeout*/ parameters_.GetTimeout(),
                     &status, NULL),
      parameters_, "C-ECHO");
      
    return status == STATUS_Success;
  }


  void DicomControlUserConnection::Find(DicomFindAnswers& result,
                                        ResourceType level,
                                        const DicomMap& originalFields,
                                        bool normalize)
  {
    std::unique_ptr<ParsedDicomFile> query;

    if (normalize)
    {
      DicomMap fields;
      NormalizeFindQuery(fields, level, originalFields);
      query.reset(ConvertQueryFields(fields, parameters_.GetRemoteModality().GetManufacturer()));
    }
    else
    {
      query.reset(new ParsedDicomFile(originalFields, GetDefaultDicomEncoding(),
                                      false /* be strict */, "" /* no private creator */));
    }
    
    DcmDataset* dataset = query->GetDcmtkObject().getDataset();

    const char* clevel = NULL;
    const char* sopClass = NULL;

    switch (level)
    {
      case ResourceType_Patient:
        clevel = "PATIENT";
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "PATIENT");
        sopClass = UID_FINDPatientRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Study:
        clevel = "STUDY";
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "STUDY");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Series:
        clevel = "SERIES";
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "SERIES");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Instance:
        clevel = "IMAGE";
        DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "IMAGE");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }


    const char* universal;
    if (parameters_.GetRemoteModality().GetManufacturer() == ModalityManufacturer_GE)
    {
      universal = "*";
    }
    else
    {
      universal = "";
    }      
    

    // Add the expected tags for this query level.
    // WARNING: Do not reorder or add "break" in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        if (!dataset->tagExists(DCM_SOPInstanceUID))
        {
          DU_putStringDOElement(dataset, DCM_SOPInstanceUID, universal);
        }

      case ResourceType_Series:
        if (!dataset->tagExists(DCM_SeriesInstanceUID))
        {
          DU_putStringDOElement(dataset, DCM_SeriesInstanceUID, universal);
        }

      case ResourceType_Study:
        if (!dataset->tagExists(DCM_AccessionNumber))
        {
          DU_putStringDOElement(dataset, DCM_AccessionNumber, universal);
        }

        if (!dataset->tagExists(DCM_StudyInstanceUID))
        {
          DU_putStringDOElement(dataset, DCM_StudyInstanceUID, universal);
        }

      case ResourceType_Patient:
        if (!dataset->tagExists(DCM_PatientID))
        {
          DU_putStringDOElement(dataset, DCM_PatientID, universal);
        }
        
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    assert(clevel != NULL && sopClass != NULL);
    FindInternal(result, dataset, sopClass, false, clevel);
  }
    

  void DicomControlUserConnection::Move(const std::string& targetAet,
                                        ResourceType level,
                                        const DicomMap& findResult)
  {
    DicomMap move;
    switch (level)
    {
      case ResourceType_Patient:
        TestAndCopyTag(move, findResult, DICOM_TAG_PATIENT_ID);
        break;

      case ResourceType_Study:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        break;

      case ResourceType_Series:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
        break;

      case ResourceType_Instance:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SOP_INSTANCE_UID);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    MoveInternal(targetAet, level, move);
  }


  void DicomControlUserConnection::Move(const std::string& targetAet,
                                        const DicomMap& findResult)
  {
    if (!findResult.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    const std::string tmp = findResult.GetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL).GetContent();
    ResourceType level = StringToResourceType(tmp.c_str());

    Move(targetAet, level, findResult);
  }


  void DicomControlUserConnection::MovePatient(const std::string& targetAet,
                                               const std::string& patientId)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_PATIENT_ID, patientId, false);
    MoveInternal(targetAet, ResourceType_Patient, query);
  }
    

  void DicomControlUserConnection::MoveStudy(const std::string& targetAet,
                                             const std::string& studyUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    MoveInternal(targetAet, ResourceType_Study, query);
  }

    
  void DicomControlUserConnection::MoveSeries(const std::string& targetAet,
                                              const std::string& studyUid,
                                              const std::string& seriesUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
    MoveInternal(targetAet, ResourceType_Series, query);
  }


  void DicomControlUserConnection::MoveInstance(const std::string& targetAet,
                                                const std::string& studyUid,
                                                const std::string& seriesUid,
                                                const std::string& instanceUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
    query.SetValue(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, false);
    MoveInternal(targetAet, ResourceType_Instance, query);
  }


  void DicomControlUserConnection::FindWorklist(DicomFindAnswers& result,
                                                ParsedDicomFile& query)
  {
    DcmDataset* dataset = query.GetDcmtkObject().getDataset();
    const char* sopClass = UID_FINDModalityWorklistInformationModel;

    FindInternal(result, dataset, sopClass, true, NULL);
  }
}
