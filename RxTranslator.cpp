/*
	DESCRIPTION

	This file contains the implementation of the class that offers the first contact with
	Autodesk Inventor (R).

*/

#include "stdafx.h"
#include <map>
#include "rxtranslator.h"
#include "translator.h"
#include "MassProp.h"
#include "MoAssembly.h"
#include "MoBody.h"
#include "MoBodyFrame.h"
#include "moJoint.h"

// Thumbnail utils

std::string toBase64 (BYTE const * bytes, size_t length)
// converts the bytes array into a base64 string
{
	std::string output;

	// NOTE: If your implementation of base64 must use a slightly different alphabet, you must
	// change it here and in base64decode.
	static const char *const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		
	output.clear();
	output.reserve((4 * length + 2) / 3); // Any changes to the algorithm would likely change this as well
	int tmp;
	const unsigned char *cur = (const unsigned char *)bytes;
	const unsigned char *end = cur + length;
	while(end - cur >= 3)
	{
		output += alphabet[(*cur >> 2) & 0x3F];
		tmp = (*cur++ << 4) & 0x30;
		output += alphabet[tmp | ((*cur >> 4) & 0x0F)];
		tmp = (*cur++ << 2) & 0x3C;
		output += alphabet[tmp | ((*cur >> 6) & 0x03)];
		output += alphabet[*cur++ & 0x3F];
	}

	if(end - cur)
	{
		output += alphabet[(*cur >> 2) & 0x3F];
		if(end - cur > 1)
		{
			tmp = (*cur++ << 4) & 0x30;
			output += alphabet[tmp | ((*cur >> 4) & 0x0F)];
			if(end - cur > 1)
			{
				tmp = (*cur++ << 2) & 0x3C;
				output += alphabet[tmp | ((*cur >> 6) & 0x03)];
				output += alphabet[*cur & 0x3F];
			}
			else
				output += alphabet[(*cur << 2) & 0x3C];
		}
		else
		{
			output += alphabet[(*cur << 4) & 0x30];
		}
	}

	// Apply padding the way that windows base 64 encoder does...
	size_t encodedSize = output.size();
	while(encodedSize % 4)
	{
		output += "=";
		encodedSize++;
	}

	return output;
}

HRESULT render(IPicture* piPicture, BYTE*&bytes, size_t &len )
{
	HRESULT hr = NOERROR;

	// Set up the bitmap info...
	LONG dwWidth = 200, dwHeight = 200;
	BITMAPINFO bminfo;
	bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bminfo.bmiHeader.biWidth = dwWidth;
	bminfo.bmiHeader.biHeight = dwHeight;
	bminfo.bmiHeader.biPlanes = 1;
	bminfo.bmiHeader.biBitCount = 24;
	bminfo.bmiHeader.biCompression = BI_RGB;
	bminfo.bmiHeader.biSizeImage = 0; // 0 for BI_RGB compression
	bminfo.bmiHeader.biXPelsPerMeter = 2835 ;
	bminfo.bmiHeader.biYPelsPerMeter = 2835 ;
	bminfo.bmiHeader.biClrUsed = 0;
	bminfo.bmiHeader.biClrImportant = 0;

	// Create the storage stream...
	IStream* piStream = NULL;
	hr = ::CreateStreamOnHGlobal( NULL, TRUE, &piStream );

	// Render it...
	LPVOID pBits=NULL;
	HBITMAP hBitmap = ::CreateDIBSection(NULL, &bminfo, DIB_RGB_COLORS, &pBits, NULL, 0);
	CDC memDC;
	VERIFY(memDC.CreateCompatibleDC(NULL));
	CBitmap bitmap;
	bitmap.Attach(hBitmap);
	CBitmap *pOldBitmap = memDC.SelectObject(&bitmap);
	memDC.SetMapMode(MM_TEXT);
	OLE_YSIZE_HIMETRIC height;
	OLE_XSIZE_HIMETRIC width;
	hr = piPicture->get_Height(&height);
	ASSERT(SUCCEEDED(hr));
	hr = piPicture->get_Width(&width);
	ASSERT(SUCCEEDED(hr));
	hr = piPicture->Render(memDC.m_hDC, 0, dwWidth, dwHeight, -dwWidth, 0,0, width, height,NULL);
	ASSERT(SUCCEEDED(hr));
	memDC.SelectObject(pOldBitmap);
	
	// Create the bitmap header...
	PICTDESC pictDesc;
	pictDesc.cbSizeofstruct = sizeof(PICTDESC);
	pictDesc.bmp.hbitmap = (HBITMAP)bitmap.m_hObject;
	pictDesc.bmp.hpal = NULL;
	pictDesc.picType = PICTYPE_BITMAP;

	// Write it to the stream...
	IUnknownPtr punkPicture;
	hr = OleCreatePictureIndirect(&pictDesc,IID_IUnknown,FALSE,(LPVOID*)&punkPicture);
	ASSERT(SUCCEEDED(hr));
	IPicturePtr spiNewPicture = punkPicture;
	hr = spiNewPicture->SaveAsFile(piStream,TRUE,NULL);
	ASSERT(SUCCEEDED(hr));

	// Read the bytes in the stream...
	STATSTG stgStat;
	LARGE_INTEGER liZero;
	liZero.QuadPart = 0;
	piStream->Seek( liZero,  STREAM_SEEK_SET, NULL );
	piStream->Stat( &stgStat, STATFLAG_NONAME );
	bytes = new BYTE[(ULONG)stgStat.cbSize.QuadPart];
	ULONG ulBytesRead = 0;
	piStream->Read( (LPVOID)bytes, (ULONG)stgStat.cbSize.QuadPart, &ulBytesRead );
	len = ulBytesRead;

	return NOERROR;
}

HRESULT thumbnail(const IPictureDispPtr& pictureDisp, std::string& thumbnailBase64)
{
	if (IPicturePtr pict = pictureDisp)
	{
		BYTE* bytes = nullptr;
		size_t len = 0;
		HRESULT hr = render(pict, bytes, len);
		OnErrorReturn(FAILED(hr), hr);

		thumbnailBase64 = toBase64(bytes, len);
		delete[] bytes;
	}

	return thumbnailBase64.empty() ? E_FAIL : S_OK;
}

HRESULT documentThumbnail(DocumentPtr doc, std::string& thumbnailBase64)
{
	if (doc)
	{
		if (IPictureDispPtr pictDisp = doc->GetThumbnail())
		{
			return thumbnail(pictDisp, thumbnailBase64);
		}
	}
	return E_FAIL;
}

// The addin wizard adds command id definitions here.

/*--------------------- IUnknown-interface related implementation  --------------------------------*/

/*
 * All of the standard IUnknown interfaces are taken care of in the CUnknown
 * class. The implementation in the CUnknown class relies on this override that
 * should end up looking like a non-delegating QueryInterface, except for the fact that QI
 * for the IUnknown is also taken care of by CUnknown (control will never reach here if
 * QI-ed for IUnknown).
 */

HRESULT CRxTranslator::InternalQueryInterface (REFIID riid, void **ppv)
{
	OnErrorReturn(!ppv, E_INVALIDARG);
	*ppv = NULL;

	if (IsEqualIID(riid, __uuidof(IRxApplicationAddInServer)) ||
		IsEqualIID(riid, __uuidof(IRxTranslatorAddInServer)) ||
		IsEqualIID(riid, __uuidof(IRxTranslatorAddInServer2)))
			*ppv = static_cast<IRxTranslatorAddInServer2*>(this);

	if (*ppv)
		static_cast<IUnknown*>(*ppv)->AddRef();

	return *ppv ? S_OK : E_NOINTERFACE;
}

/*---------------------- Constructor(s), initializers and destructor ---------------------------------*/

CRxTranslator::CRxTranslator () : CUnknown (NULL)
{
	m_pSite = NULL;

	::IncrementObjectCount();
}

CRxTranslator::~CRxTranslator()
{
	if (m_pApplication)
			m_pApplication = NULL;

	if (m_pSite)
		m_pSite = NULL;

	::DecrementObjectCount();
}

/*---------------------- IRxApplicationAddInServer interface ---------------------------------*/

HRESULT CRxTranslator::Activate (IRxApplicationAddInSite *pAddInSite, BooleanType bFirstTime)
{
	AFX_MANAGE_STATE (AfxGetStaticModuleState());

	// High-level interfaces supplied directly and implicitly by Autodesk Inventor (R) to the
	// AddIn. These may be held right up until the directive to shutdown (Deactivate) is received.

	m_pSite = pAddInSite;

	CComPtr<IUnknown> pAppUnk;
	HRESULT Result = pAddInSite->get_Application (&pAppUnk);
	OnErrorReturn(FAILED(Result), Result);

	Result = pAppUnk.QueryInterface(&m_pApplication);
	OnErrorReturn(FAILED(Result), Result);

	return S_OK;
}

HRESULT CRxTranslator::Deactivate ()
{
	AFX_MANAGE_STATE (AfxGetStaticModuleState());

	m_pApplication.Release();

	m_pSite.Release();

	return S_OK;
}


HRESULT CRxTranslator::ExecuteCommand (long CommandID)
{
	return E_NOTIMPL;
}

HRESULT CRxTranslator::get_Automation (IUnknown **ppUnk)
{
	return E_NOINTERFACE;
}

/*---------------------- IRxTranslatorAddInServer interface ---------------------------------*/

HRESULT CRxTranslator::get_HasOpenOptions(DataMedium* pSourceData, TranslationContext* pContext,
	NameValueMap* pDefaultOptions, char* pHasOptionsUI)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!pHasOptionsUI)
		return E_INVALIDARG;

	// TODO:
	// If this AddIn wanted to enable the open options button, set pEnableOpenOptions to true
	// and ShowOpenOptions will be called if the user selects the Options button.
	// If an options map is provided and if we wish to expose public options for this operation,
	// we can fill the name-value map with options and their default or current values.

	*pHasOptionsUI = false;

	return S_OK;
}


HRESULT CRxTranslator::Open(DataMedium* pSourceData, TranslationContext* pContext,
	NameValueMap* pOptions, IUnknown** ppTargetObject)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return E_NOTIMPL;
}

HRESULT CRxTranslator::ShowOpenOptions(DataMedium* pSourceData, TranslationContext* pContext,
	NameValueMap* pChosenOptions)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// TODO:
	// If this AddIn wanted to enable open options and set pHasOptionsUI to true
	// in get_HasOpenOptions, display it's open options dialog here.
	// If an options map is provided and if we wish to expose public options for this operation,
	// we can fill the name-value map with the options chosen through our options dialog.

	return E_NOTIMPL;
}

HRESULT CRxTranslator::get_HasSaveCopyAsOptions(IUnknown* pSourceObject, TranslationContext* pContext,
	NameValueMap* pDefaultOptions, char* pHasOptionsUI)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!pHasOptionsUI)
		return E_INVALIDARG;

	// TODO:
	// If this AddIn wanted to enable the save options button, set pEnableOptions to true
	// and ShowSaveCopyAsOptions will be called if the user selects the Options button.
	// If an options map is provided and if we wish to expose public options for this operation,
	// we can fill the name-value map with options and their default or current values.

	*pHasOptionsUI = true;

	return S_OK;
}

HRESULT CRxTranslator::ShowSaveCopyAsOptions(IUnknown* pSourceObject, TranslationContext* pContext,
	NameValueMap* pChosenOptions)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// TODO:
	// If this AddIn wanted to enable save options and set pEnableOptions to true
	// in get_HasSaveCopyAsOptions, display it's save options dialog here.
	// If an options map is provided and if we wish to expose public options for this operation,
	// we can fill the name-value map with the options chosen through our options dialog.

	return E_NOTIMPL;
}

HRESULT CRxTranslator::CreateModelicaAssembly(FILE *pFile, AssemblyDocument* pDoc, MoAssemblyPtr& moAssembly)
{
	AssemblyComponentDefinitionPtr pAssemblyDefPtr;
	HRESULT hr = pDoc->get_ComponentDefinition(&pAssemblyDefPtr);
	if (FAILED(hr))
		return hr;

	NameValueMapPtr pGlobalOptions = nullptr;
	m_pApplication->TransientObjects->CreateNameValueMap( &pGlobalOptions );
	if ( pGlobalOptions )
	{
		pGlobalOptions->Add( L"SuperfluousDOF", (_variant_t) VARIANT_TRUE );
		pGlobalOptions->Add( L"UseMassDifference", (_variant_t) VARIANT_FALSE );
		pGlobalOptions->Add( L"DoubleBearing",  (_variant_t) VARIANT_TRUE );
	}

	ObjectCollectionPtr pObjectCollection = nullptr;
	hr = m_pApplication->TransientObjects->CreateObjectCollection( vtMissing, &pObjectCollection );
	if ( !pObjectCollection ) return hr;

	VARIANT occVar;
	occVar.vt = VT_DISPATCH;
	occVar.pdispVal = pObjectCollection;
	CComVariant pOccurrences(occVar);
	COleVariant variantBehavior;

	RigidBodyResultsPtr rigidBodyResults = pAssemblyDefPtr->MethodRigidBodyAnalysis( pGlobalOptions, pOccurrences, variantBehavior );
	if (!rigidBodyResults)
		return E_FAIL;

	moAssembly = std::make_shared<MoAssembly>();
	std::string assemblyThumbnail;
	if (SUCCEEDED(documentThumbnail(pDoc, assemblyThumbnail)))
	{
		moAssembly->thumbnail(assemblyThumbnail);
	}

	std::map<ULONG, MoBodyPtr> moBodies;

	RigidBodyGroupsPtr rigidBodyGroups = rigidBodyResults->GetRigidBodyGroups();
	if (rigidBodyGroups)
	{
		for (long i = 0 ; i < rigidBodyGroups->Count ; ++i)
		{
			// Set Cre data in pGroupsTmp[i]
			RigidBodyGroupPtr rigidBodyGroup = rigidBodyGroups->GetItem( i+1 );
			if (!rigidBodyGroup)
				continue; 

			auto moBody = std::make_shared<MoBody>();
			moBodies[rigidBodyGroup->GetGroupID()] = moBody;
			moAssembly->addBody(moBody);

			bool grounded = rigidBodyGroup->GetGrounded() == VARIANT_TRUE ? true : false;

			// An Occurrence is a path through the assembly tree.
			ComponentOccurrencesEnumeratorPtr componentOccs ;
			CSafeArrayLong componentStates;
			rigidBodyGroup->GetOccurrencesAndStates( &componentOccs, &componentStates );

			moBody->grounded(grounded);
			
			ComponentOccurrencePtr bestRepresentative;
			double largestMass = 0.0;
			bool foundThumb = false;
			// Components can have multiple instances; we need to make sure we look at each one.
			for ( int j = 0 ; j < componentOccs->Count ; ++j )
			{
				ComponentOccurrencePtr componentOcc = componentOccs->GetItem( j+1 );
				if (!componentOcc) continue;

				MassPropertiesPtr massProps = componentOcc->GetMassProperties();
				if (!massProps) continue;

				auto pt = massProps->GetCenterOfMass();
				AcGePoint3d cg(pt->GetX(), pt->GetY(), pt->GetZ());
				double moments[3], products[3];
				double mass = massProps->GetMass();
				hr = massProps->XYZMomentsOfInertia(&moments[0], &moments[1], &moments[2], &products[0], &products[1], &products[2]);
				if (hr == S_OK)
				{
					moBody->addMass(massProps->GetMass(), cg, MIxInertiaTensor(moments, products));
					if (mass >= largestMass)
					{
						bestRepresentative = componentOcc;
						largestMass = mass;
					}
				}
			}
				
			if (bestRepresentative)
			{
				std::string thumbnailBase64;
				if (SUCCEEDED(documentThumbnail(bestRepresentative->GetDefinition()->GetDocument(), thumbnailBase64)))
					moBody->thumbnail(thumbnailBase64);
			}
		}
	}

	std::vector< std::shared_ptr<MoJoint> > moJoints;
	RigidBodyJointsPtr rigidBodyJoints = rigidBodyResults->GetRigidBodyJoints();
	if (rigidBodyJoints)
	{
		for (long i = 0 ; i < rigidBodyJoints->Count ; ++i)
		{
			RigidBodyJointPtr rigidBodyJoint = rigidBodyJoints->GetItem( i+1 );
			if (!rigidBodyJoint)
				continue; 
			switch (rigidBodyJoint->GetJointType())
			{
			case kConCentricCircleCircleJoint:
				{
					std::shared_ptr<MoBody> b1, b2;
					auto rg1 = rigidBodyJoint->GetGroupOne();
					if (rg1)
					{
						auto i = moBodies.find(rg1->GetGroupID());
						if (i == moBodies.end())
							continue;
						b1 = i->second;
					}
					
					auto rg2 = rigidBodyJoint->GetGroupTwo();
					if (rg2)
					{
						auto i = moBodies.find(rg2->GetGroupID());
						if (i == moBodies.end())
							continue;
						b2 = i->second;
					}

					if (b1 == b2)
						continue;

					IDispatchPtr pGeometry1 = nullptr;
					IDispatchPtr pGeometry2 = nullptr;
					NameValueMapPtr pAdditionalInfo = nullptr;
					m_pApplication->TransientObjects->CreateNameValueMap( &pAdditionalInfo );

					hr = rigidBodyJoint->GetJointData(&pGeometry1, &pGeometry2, &pAdditionalInfo);
					if (FAILED(hr))
						continue;

					double rad;

					CirclePtr cir1( pGeometry1);
					CSafeArrayDouble saPt, saVec ;
					hr = cir1->GetCircleData( &saPt, &saVec, &rad ) ;
					if (FAILED(hr))
						continue;
					AcGePoint3d origin1(saPt[0], saPt[1], saPt[2]);
					AcGeVector3d zAxis1(saVec[0], saVec[1], saVec[2]);
					zAxis1.normalize();

					CirclePtr cir2( pGeometry2);
					CSafeArrayDouble saPt2, saVec2 ;
					hr = cir2->GetCircleData( &saPt2, &saVec2, &rad ) ;
					if (FAILED(hr))
						continue;
					AcGePoint3d origin2(saPt2[0], saPt2[1], saPt2[2]);
					AcGeVector3d zAxis2(saVec2[0], saVec2[1], saVec2[2]);
					zAxis2.normalize();

					AcGeVector3d xAxis1 = zAxis1.perpVector();
					AcGeVector3d xAxis2 = zAxis2.perpVector();

					bool fromJoint = pAdditionalInfo->GetValue(L"FromJoint");
					if (fromJoint)
					{
						xAxis1.x = pAdditionalInfo->GetValue(L"Vector1aX");
						xAxis1.y = pAdditionalInfo->GetValue(L"Vector1aY");
						xAxis1.z = pAdditionalInfo->GetValue(L"Vector1aZ");

						xAxis2.x = pAdditionalInfo->GetValue(L"Vector2aX");
						xAxis2.y = pAdditionalInfo->GetValue(L"Vector2aY");
						xAxis2.z = pAdditionalInfo->GetValue(L"Vector2aZ");

						if (!xAxis1.isZeroLength() && !xAxis2.isZeroLength())
						{
							xAxis1.normalize();
							xAxis2.normalize();
						}
					}

					AcGeVector3d yAxis1 = zAxis1.crossProduct(xAxis1);
					AcGeVector3d yAxis2 = zAxis2.crossProduct(xAxis2);
					AcGeMatrix3d transform1, transform2;
					transform1.setCoordSystem(origin1, xAxis1, yAxis1, zAxis1);
					transform2.setCoordSystem(origin2, xAxis2, yAxis2, zAxis2);

					auto joint = std::make_shared<MoJoint>();
					joint->type(MoJoint::eRevolute);
					MoBodyFramePtr bf1 = std::make_shared<MoBodyFrame>(b1, transform1);
					MoBodyFramePtr bf2 = std::make_shared<MoBodyFrame>(b2, transform1);
					b1->addBodyFrame(bf1);
					b2->addBodyFrame(bf2);
					joint->frame1(bf1);
					joint->frame2(bf2);

					moJoints.push_back(joint);
					moAssembly->addJoint(joint);
				}
			}
		}
	}

	BSTR displayName;
	hr = pDoc->get_DisplayName(&displayName);
	UTxString dispName = displayName;
	size_t i = dispName.find_last_of(L'.');
	dispName.erase(i,UTxString::npos);
	if (FAILED(hr))
		return hr;
	moAssembly->name(dispName);

	moAssembly->layout();

	return S_OK;
}

HRESULT CRxTranslator::SaveCopyAs(IUnknown* pSourceObject, TranslationContext* pContext, NameValueMap* pOptions, DataMedium* pTargetData)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!pSourceObject || !pContext || !pTargetData)
		return E_INVALIDARG;

	// Check that this is a desired translation context.  This sample supports only file dialog operations.

	IOMechanismEnum type;
	HRESULT hr = pContext->get_Type(&type);
	if (FAILED(hr) || type != kFileBrowseIOMechanism)
		return E_UNEXPECTED;

	// Obtain the file name from the data medium

	CComBSTR fileName;
	hr = pTargetData->get_FileName(&fileName);
	if (FAILED(hr) || !fileName)
		return E_INVALIDARG;

	// If our translators supports options, option values may be obtained from the optionally specified
	// options map, with a format consistent with what we specified from the Has*Options and Show*Options members.

	// Currently, only implemented to support part document types, even though the
	// translator is registered to support assembly files as well (to demonstrate
	// registering multiple base file type support).

	hr = E_FAIL;
	CComQIPtr<AssemblyDocument> pAssemblyDoc(pSourceObject);
	if (pAssemblyDoc)
	{
		USES_CONVERSION;
		FILE *pFile = nullptr;
		_tfopen_s(&pFile, fileName, L"wt");
		if (!pFile)
			return E_FAIL;

		MoAssemblyPtr moAssembly;
		hr = CreateModelicaAssembly(pFile, pAssemblyDoc, moAssembly);

		if (SUCCEEDED(hr))
		{
			if (!moAssembly->write(pFile))
			{
				hr = E_FAIL;
				AfxMessageBox(_T("Failed to Save Document."));
			}
		}
		else
			AfxMessageBox(_T("Failed to convert assembly to Modelica."));

		int ret = fclose(pFile);
		if (ret != 0)
		{
			remove(W2A(fileName));
			return E_FAIL;
		}
	}

	return S_OK;
}

STDMETHODIMP CRxTranslator::GetThumbnail(DataMedium* pSourceData, VARIANT* pThumbnail)
{
	if (!pSourceData || !pThumbnail)
		return E_INVALIDARG;

	VariantInit(pThumbnail);

	CComBSTR file;
	HRESULT hr = pSourceData->get_FileName(&file);
	if (FAILED(hr) || !file)
		return E_FAIL;

	return S_OK;
}
