/*****************************************************************************
 * plugin.cpp: Free ActiveX based on ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2008 http://unick-soft.xost.ru
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 * Modification: Oleg <soft_support@list.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "plugin.h"

#include "helpers/oleobject.h"
#include "helpers/olecontrol.h"
#include "helpers/oleinplaceobject.h"
#include "helpers/oleinplaceactiveobject.h"
#include "helpers/persistpropbag.h"
#include "helpers/persiststreaminit.h"
#include "helpers/persiststorage.h"
#include "helpers/provideclassinfo.h"
#include "helpers/connectioncontainer.h"
#include "helpers/objectsafety.h"
//#include "vlccontrol.h"
#include "vlccontrol2.h"
#include "helpers/viewobject.h"
#include "helpers/dataobject.h"
#include "helpers/supporterrorinfo.h"

#include "misc/utils.h"
#include "misc/setting.h"

#ifndef WITH_OUT_MFC
    #include "resource/resource.h"
#endif //WITH_OUT_MFC

#include <string.h>
#include <winreg.h>
#include <winuser.h>
#include <servprov.h>
#include <shlwapi.h>
#include <wininet.h>

#define USE_AURA
#include "atom_main_delegate.h"
#include "sandbox_types.h"
#include "atom_main_args.h"
#include "content_main.h"
#include "content/public/app/startup_helper_win.h"

using namespace std;

////////////////////////////////////////////////////////////////////////
//class factory      
static LRESULT CALLBACK VLCInPlaceClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VLCPlugin *p_instance = reinterpret_cast<VLCPlugin *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_ERASEBKGND:
            return 1L;

        case WM_PAINT:
            PAINTSTRUCT ps;
            RECT pr;
            if( GetUpdateRect(hWnd, &pr, FALSE) )
            {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                BeginPaint(hWnd, &ps);
                p_instance->onPaint(ps.hdc, bounds, pr);
                EndPaint(hWnd, &ps);
            }
            return 0L;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
};

#ifndef WITH_OUT_MFC

static INT_PTR CALLBACK AXDialogBixWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VLCPlugin *p_instance = reinterpret_cast<VLCPlugin *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_ERASEBKGND:
            return 1L;

        case WM_PAINT:
            PAINTSTRUCT ps;
            RECT pr;
            if( GetUpdateRect(hWnd, &pr, FALSE) )
            {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                BeginPaint(hWnd, &ps);
                p_instance->onPaint(ps.hdc, bounds, pr);
                EndPaint(hWnd, &ps);
            }
            return 0L;

        case WM_COMMAND:
        {
            switch( wParam )
            {
                case IDC_BUTTON_CLEAR:
                {
                    p_instance->clearEditBox();
                    break;
                }

                case IDC_BUTTON_URL:
                {
                    ShellExecuteA( hWnd, NULL, PAGE_OF_PROGRAM,
						NULL, NULL, SW_SHOW);
                    break;
                }
            }
            return 0L;
        }        

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
};
#endif

VLCPluginClass::VLCPluginClass(LONG *p_class_ref, HINSTANCE hInstance, REFCLSID rclsid) :
    _p_class_ref(p_class_ref),
    _hinstance(hInstance),
    _classid(rclsid),
    _inplace_picture(NULL)
{
    WNDCLASS wClass;

    if( ! GetClassInfo(hInstance, getInPlaceWndClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE|CS_DBLCLKS;
        wClass.lpfnWndProc    = VLCInPlaceClassWndProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = hInstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = NULL;
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getInPlaceWndClassName();

        _inplace_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _inplace_wndclass_atom = 0;
    }

    HBITMAP hbitmap = (HBITMAP)LoadImage(getHInstance(), TEXT("INPLACE-PICT"), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
    if( NULL != hbitmap )
    {
        PICTDESC pictDesc;

        pictDesc.cbSizeofstruct = sizeof(PICTDESC);
        pictDesc.picType        = PICTYPE_BITMAP;
        pictDesc.bmp.hbitmap    = hbitmap;
        pictDesc.bmp.hpal       = NULL;

        if( FAILED(OleCreatePictureIndirect(&pictDesc, IID_IPicture, TRUE, reinterpret_cast<LPVOID*>(&_inplace_picture))) )
            _inplace_picture = NULL;
    }
    AddRef();
};

VLCPluginClass::~VLCPluginClass()
{
    if( 0 != _inplace_wndclass_atom )
        UnregisterClass(MAKEINTATOM(_inplace_wndclass_atom), _hinstance);

    if( NULL != _inplace_picture )
        _inplace_picture->Release();
};

STDMETHODIMP VLCPluginClass::QueryInterface(REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_INVALIDARG;

    if( (IID_IUnknown == riid)
     || (IID_IClassFactory == riid) )
    {
        AddRef();
        *ppv = reinterpret_cast<LPVOID>(this);

        return NOERROR;
    }

    *ppv = NULL;

    return E_NOINTERFACE;
};

STDMETHODIMP_(ULONG) VLCPluginClass::AddRef(void)
{
    return InterlockedIncrement(_p_class_ref);
};

STDMETHODIMP_(ULONG) VLCPluginClass::Release(void)
{
    ULONG refcount = InterlockedDecrement(_p_class_ref);
    if( 0 == refcount )
    {
        delete this;
        return 0;
    }
    return refcount;
};

STDMETHODIMP VLCPluginClass::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_POINTER;

    *ppv = NULL;

    if( (NULL != pUnkOuter) && (IID_IUnknown != riid) ) {
        return CLASS_E_NOAGGREGATION;
    }

    VLCPlugin *plugin = new VLCPlugin(this, pUnkOuter);
    if( NULL != plugin )
    {
        HRESULT hr = plugin->QueryInterface(riid, ppv);
        // the following will destroy the object if QueryInterface() failed
        plugin->Release();
        return hr;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCPluginClass::LockServer(BOOL fLock)
{
    if( fLock )
        AddRef();
    else
        Release();

    return S_OK;
};

////////////////////////////////////////////////////////////////////////

VLCPlugin::VLCPlugin(VLCPluginClass *p_class, LPUNKNOWN pUnkOuter) :
    _inplacewnd(NULL),
    _p_class(p_class),
    _i_ref(1UL),
    _i_codepage(CP_ACP),
#ifndef WITH_OUT_MFC
    _b_usermode(TRUE)
#else
    _b_usermode(FALSE) 
#endif
{
    p_class->AddRef();

    vlcOleControl = new VLCOleControl(this);
    vlcOleInPlaceObject = new VLCOleInPlaceObject(this);
    vlcOleInPlaceActiveObject = new VLCOleInPlaceActiveObject(this);
    vlcPersistStorage = new VLCPersistStorage(this);
    vlcPersistStreamInit = new VLCPersistStreamInit(this);
    vlcPersistPropertyBag = new VLCPersistPropertyBag(this);
    vlcProvideClassInfo = new VLCProvideClassInfo(this);
    vlcConnectionPointContainer = new VLCConnectionPointContainer(this);
    vlcObjectSafety = new VLCObjectSafety(this);
    vlcControl2 = new AXControl(this);
    vlcViewObject = new VLCViewObject(this);
    vlcDataObject = new VLCDataObject(this);
    vlcOleObject = new VLCOleObject(this);
    vlcSupportErrorInfo = new VLCSupportErrorInfo(this);

    // configure controlling IUnknown interface for implemented interfaces
    this->pUnkOuter = (NULL != pUnkOuter) ? pUnkOuter : dynamic_cast<LPUNKNOWN>(this);

    // default picure
    _p_pict = p_class->getInPlacePict();

    // make sure that persistable properties are initialized
    onInit();
};

VLCPlugin::~VLCPlugin()
{
    AddRef(); 

    delete vlcSupportErrorInfo;
    delete vlcOleObject;
    delete vlcDataObject;
    delete vlcViewObject;
    delete vlcControl2;    
    delete vlcConnectionPointContainer;
    delete vlcProvideClassInfo;
    delete vlcPersistPropertyBag;
    delete vlcPersistStreamInit;
    delete vlcPersistStorage;
    delete vlcOleInPlaceActiveObject;
    delete vlcOleInPlaceObject;
    delete vlcObjectSafety;

    delete vlcOleControl;
    if( _p_pict )
        _p_pict->Release();

    _p_class->Release();
};

STDMETHODIMP VLCPlugin::QueryInterface(REFIID riid, void **ppv)
{
	BOOL log = false;

    if( NULL == ppv )
        return E_INVALIDARG;
	if (IID_IUnknown == riid)
	{
		if(log) OutputDebugStringA("IID_IUnknown");
		*ppv = reinterpret_cast<LPVOID>(this);
	}
	else if (IID_IOleObject == riid)
	{
		if(log) OutputDebugStringA("IID_IOleObject");
		*ppv = reinterpret_cast<LPVOID>(vlcOleObject);
	}
	else if (IID_IOleControl == riid)
	{
		if(log) OutputDebugStringA("IID_IOleControl");
		*ppv = reinterpret_cast<LPVOID>(vlcOleControl);
	}
	else if (IID_IOleWindow == riid)
	{
		if(log) OutputDebugStringA("IID_IOleWindow");
		*ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceObject);
	}
	else if (IID_IOleInPlaceObject == riid)
	{
		if(log) OutputDebugStringA("IID_IOleInPlaceObject");
		*ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceObject);
	}
	else if (IID_IOleInPlaceActiveObject == riid)
	{
		if(log) OutputDebugStringA("IID_IOleInPlaceActiveObject");
		*ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceActiveObject);
	}
	else if (IID_IPersist == riid)
	{
		if(log) OutputDebugStringA("IID_IPersist");
		*ppv = reinterpret_cast<LPVOID>(vlcPersistStreamInit);
	}
	else if (IID_IPersistStreamInit == riid)
	{
		if(log) OutputDebugStringA("IID_IPersistStreamInit");
		*ppv = reinterpret_cast<LPVOID>(vlcPersistStreamInit);
	}
	else if (IID_IPersistStorage == riid)
	{
		if(log) OutputDebugStringA("IID_IPersistStorage");
		*ppv = reinterpret_cast<LPVOID>(vlcPersistStorage);
	}
	else if (IID_IPersistPropertyBag == riid)
	{
		if(log) OutputDebugStringA("IID_IPersistPropertyBag");
		*ppv = reinterpret_cast<LPVOID>(vlcPersistPropertyBag);
	}
	else if (IID_IProvideClassInfo == riid)
	{
		if(log) OutputDebugStringA("IID_IProvideClassInfo");
		*ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
	}
	else if (IID_IProvideClassInfo2 == riid)
	{
		if(log) OutputDebugStringA("IID_IProvideClassInfo2");
		*ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
		*ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
	}
    else if( IID_IConnectionPointContainer == riid )
	{
		if(log) OutputDebugStringA("IID_IConnectionPointContainer");
        *ppv = reinterpret_cast<LPVOID>(vlcConnectionPointContainer);
	}
	else if (IID_IObjectSafety == riid)
	{
		if(log) OutputDebugStringA("IID_IObjectSafety");
		*ppv = reinterpret_cast<LPVOID>(vlcObjectSafety);
	}
	else if (IID_IDispatch == riid)
	{
		if(log) OutputDebugStringA("IID_IDispatch");
		*ppv = reinterpret_cast<LPVOID>(vlcControl2);
	}
	else if (IID_IAXControl == riid)
	{
		if(log) OutputDebugStringA("IID_IAXControl");
		*ppv = reinterpret_cast<LPVOID>(vlcControl2);
	}
	else if (IID_IViewObject == riid)
	{
		if(log) OutputDebugStringA("IID_IViewObject");
		*ppv = reinterpret_cast<LPVOID>(vlcViewObject);
	}
	else if (IID_IViewObject2 == riid)
	{
		if(log) OutputDebugStringA("IID_IViewObject2");
		*ppv = reinterpret_cast<LPVOID>(vlcViewObject);
	}
	else if (IID_IDataObject == riid)
	{
		if(log) OutputDebugStringA("IID_IDataObject");
		*ppv = reinterpret_cast<LPVOID>(vlcDataObject);
	}
	else if (IID_ISupportErrorInfo == riid)
	{
		if(log) OutputDebugStringA("IID_ISupportErrorInfo");
		*ppv = reinterpret_cast<LPVOID>(vlcSupportErrorInfo);
	}
    else
    {
		if (log)
		{
			char buff[200];
			sprintf(buff, "Not implemented {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], 
				riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
			OutputDebugStringA(buff);

			OLECHAR* bstrGuid;
			StringFromCLSID(riid, &bstrGuid);
			sprintf(buff, "Not implemented {%S}", bstrGuid);
			OutputDebugStringA(buff);
		}
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
};

STDMETHODIMP_(ULONG) VLCPlugin::AddRef(void)
{
    return InterlockedIncrement((LONG *)&_i_ref);
};

STDMETHODIMP_(ULONG) VLCPlugin::Release(void)
{
    if( ! InterlockedDecrement((LONG *)&_i_ref) )
    {
        delete this;
        return 0;
    }
    return _i_ref;
};

//////////////////////////////////////

HRESULT VLCPlugin::onInit(void)
{
     //test variable init
    _bstr_testString = NULL;

     _i_backcolor = RGB(200, 40, 0);
     _bstr_testString = NULL;
     // set default/preferred size (320x240) pixels in HIMETRIC
     HDC hDC = CreateDevDC(NULL);
     _extent.cx = 320;
     _extent.cy = 240;
     HimetricFromDP(hDC, (LPPOINT)&_extent, 1);
     DeleteDC(hDC);

     return S_OK;
};

HRESULT VLCPlugin::onLoad(void)
{
    if( true )
    {
        /*
        ** try to retreive the base URL using the client site moniker, which for Internet Explorer
        ** is the URL of the page the plugin is embedded into.
        */
        LPOLECLIENTSITE pClientSite;
        if( SUCCEEDED(vlcOleObject->GetClientSite(&pClientSite)) && (NULL != pClientSite) )
        {
            IBindCtx *pBC = 0;
            if( SUCCEEDED(CreateBindCtx(0, &pBC)) )
            {
                LPMONIKER pContMoniker = NULL;
                if( SUCCEEDED(pClientSite->GetMoniker(OLEGETMONIKER_ONLYIFTHERE,
                                OLEWHICHMK_CONTAINER, &pContMoniker)) )
                {
                    LPOLESTR base_url;
                    if( SUCCEEDED(pContMoniker->GetDisplayName(pBC, NULL, &base_url)) )
                    {

                    }
                }
            }
        }
    }
    setDirty(FALSE);
    return S_OK;
};

void VLCPlugin::setErrorInfo(REFIID riid, const char *description)
{
    vlcSupportErrorInfo->setErrorInfo( 
        OLESTR("FreeActiveX.AXPlugin.2"),
        riid, description );
};

HRESULT VLCPlugin::onAmbientChanged(LPUNKNOWN pContainer, DISPID dispID)
{
    VARIANT v;
    switch( dispID )
    {
        case DISPID_AMBIENT_BACKCOLOR:
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setBackColor(V_I4(&v));
            }
            break;
        case DISPID_AMBIENT_DISPLAYNAME:
            break;
        case DISPID_AMBIENT_FONT:
            break;
        case DISPID_AMBIENT_FORECOLOR:
            break;
        case DISPID_AMBIENT_LOCALEID:
            break;
        case DISPID_AMBIENT_MESSAGEREFLECT:
            break;
        case DISPID_AMBIENT_SCALEUNITS:
            break;
        case DISPID_AMBIENT_TEXTALIGN:
            break;
        case DISPID_AMBIENT_USERMODE:
            VariantInit(&v);
            V_VT(&v) = VT_BOOL;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                //setUserMode(V_BOOL(&v) != VARIANT_FALSE);
            }
            break;
        case DISPID_AMBIENT_UIDEAD:
            break;
        case DISPID_AMBIENT_SHOWGRABHANDLES:
            break;
        case DISPID_AMBIENT_SHOWHATCHING:
            break;
        case DISPID_AMBIENT_DISPLAYASDEFAULT:
            break;
        case DISPID_AMBIENT_SUPPORTSMNEMONICS:
            break;
        case DISPID_AMBIENT_AUTOCLIP:
            break;
        case DISPID_AMBIENT_APPEARANCE:
            break;
        case DISPID_AMBIENT_CODEPAGE:
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setCodePage(V_I4(&v));
            }
            break;
        case DISPID_AMBIENT_PALETTE:
            break;
        case DISPID_AMBIENT_CHARSET:
            break;
        case DISPID_AMBIENT_RIGHTTOLEFT:
            break;
        case DISPID_AMBIENT_TOPTOBOTTOM:
            break;
        case DISPID_UNKNOWN:
            /*
            ** multiple property change, look up the ones we are interested in
            */
            VariantInit(&v);
            V_VT(&v) = VT_BOOL;
            if( SUCCEEDED(GetObjectProperty(pContainer, DISPID_AMBIENT_USERMODE, v)) )
            {
                //setUserMode(V_BOOL(&v) != VARIANT_FALSE);
            }
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, DISPID_AMBIENT_CODEPAGE, v)) )
            {
                setCodePage(V_I4(&v));
            }
            break;
    }
    return S_OK;
};

HRESULT VLCPlugin::onClose(DWORD dwSaveOption)
{
    if( isInPlaceActive() )
    {        
        onInPlaceDeactivate();        
    }
    vlcDataObject->onClose();
    return S_OK;
};

BOOL VLCPlugin::isInPlaceActive(void)
{
    return (NULL != _inplacewnd);
};

extern HINSTANCE h_instance;
extern HMODULE DllGetModule;
HWND VLCPlugin::initElectron()
{
	int argc = 1;
	char* argv[1];
	//char location[] = "C:\\Development\\FreeActiveX\\vendor\\electron\\debug\tscon32.exe";
	char location[] = "C:\\Program Files\\Microsoft Office 15\\root\\office15\\powerpnt.exe";

	argv[0] = location;

	sandbox::SandboxInterfaceInfo sandbox_info = { 0 };
	content::InitializeSandboxInfo(&sandbox_info);
	atom::AtomMainDelegate appDelegate;

	content::ContentMainParams params(&appDelegate);
	params.instance = h_instance;
	params.sandbox_info = &sandbox_info;

	atom::AtomCommandLine::Init(argc, argv);
	//content::ContentMain(params);
	content::ContentMain(params);
	return NULL;
}

HRESULT VLCPlugin::onActivateInPlace(LPMSG lpMesg, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{
	OutputDebugStringA("Activate in place");
    RECT clipRect = *lprcClipRect;

    /*
    ** record keeping of control geometry within container
    */
    _posRect = *lprcPosRect;

    /*
    ** Create a window for in place activated control.
    ** the window geometry matches the control viewport
    ** within container so that embedded video is always
    ** properly displayed.
    */
    // How VLC create window
#ifdef WITH_OUT_MFC
    _inplacewnd = CreateWindow(_p_class->getInPlaceWndClassName(),
            TEXT("ActiveX Plugin In-Place Window"),
            WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
            lprcPosRect->left,
            lprcPosRect->top,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            hwndParent,
            0,
            _p_class->getHInstance(),
            NULL
           );
#else
    //Diaog box from resourc
	_inplacewnd = CreateDialog(
                      _p_class->getHInstance(),
                      MAKEINTRESOURCE(IDD_DIALOGBAR),
                      hwndParent,
                      AXDialogBixWndProc);

#endif //WITH_OUT_MFC
    
    //param work
    if( NULL == _inplacewnd )
        return E_FAIL;

    SetWindowLongPtr(_inplacewnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    /* change cliprect coordinates system relative to window bounding rect */
    OffsetRect(&clipRect, -lprcPosRect->left, -lprcPosRect->top);

    HRGN clipRgn = CreateRectRgnIndirect(&clipRect);
    SetWindowRgn(_inplacewnd, clipRgn, TRUE);

    ShowWindow(_inplacewnd, SW_SHOW);

    return S_OK;
};

HRESULT VLCPlugin::onInPlaceDeactivate(void)
{
	OutputDebugStringA("Deactivate inplace");
    {
        fireOnStopEvent();
    }

    DestroyWindow(_inplacewnd);
    _inplacewnd = NULL;

    return S_OK;
};

void VLCPlugin::setBackColor(OLE_COLOR backcolor)
{
	OutputDebugStringA("set background color");
    if( _i_backcolor != backcolor )
    {
        _i_backcolor = backcolor;
        if( isInPlaceActive() )
        {

        }
        setDirty(TRUE);
    }
};

void VLCPlugin::setFocus(BOOL fFocus)
{
	OutputDebugStringA("set focus");
    if( fFocus )
        SetActiveWindow(_inplacewnd);
};

BOOL VLCPlugin::hasFocus(void)
{
	OutputDebugStringA("has focus");
    return GetActiveWindow() == _inplacewnd;
};

void VLCPlugin::onDraw(DVTARGETDEVICE * ptd, HDC hicTargetDev,
        HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds)
{
	char buff[100];
	sprintf(buff, "Draw");
	OutputDebugStringA(buff);
    if( true )
    {
        long width = lprcBounds->right-lprcBounds->left;
        long height = lprcBounds->bottom-lprcBounds->top;

        RECT bounds = { lprcBounds->left, lprcBounds->top, lprcBounds->right, lprcBounds->bottom };

        if( isUserMode() )
        {
            /* VLC is in user mode, just draw background color */
            COLORREF colorref = RGB(250, 50, 0);
            OleTranslateColor(_i_backcolor, (HPALETTE)GetStockObject(DEFAULT_PALETTE), &colorref);
            if( colorref != RGB(250, 50, 0) )
            {
                /* custom background */
                HBRUSH colorbrush = CreateSolidBrush(colorref);
                FillRect(hdcDraw, &bounds, colorbrush);
                DeleteObject((HANDLE)colorbrush);
				
				OutputDebugStringA("Paint background");
            }
            else
            {
                /* black background */
                //FillRect(hdcDraw, &bounds, (HBRUSH)GetStockObject(BLACK_BRUSH));
            }
        }
        else
        {
            /* VLC is in design mode, draw the VLC logo */
            FillRect(hdcDraw, &bounds, (HBRUSH)GetStockObject(WHITE_BRUSH));

            LPPICTURE pict = getPicture();
            if( NULL != pict )
            {
                OLE_XSIZE_HIMETRIC picWidth;
                OLE_YSIZE_HIMETRIC picHeight;

                pict->get_Width(&picWidth);
                pict->get_Height(&picHeight);

                SIZEL picSize = { picWidth, picHeight };

                if( NULL != hicTargetDev )
                {
                    DPFromHimetric(hicTargetDev, (LPPOINT)&picSize, 1);
                }
                else if( NULL != (hicTargetDev = CreateDevDC(ptd)) )
                {
                    DPFromHimetric(hicTargetDev, (LPPOINT)&picSize, 1);
                    DeleteDC(hicTargetDev);
                }

                if( picSize.cx > width-4 )
                    picSize.cx = width-4;
                if( picSize.cy > height-4 )
                    picSize.cy = height-4;

                LONG dstX = lprcBounds->left+(width-picSize.cx)/2;
                LONG dstY = lprcBounds->top+(height-picSize.cy)/2;

                if( NULL != lprcWBounds )
                {
                    RECT wBounds = { lprcWBounds->left, lprcWBounds->top, lprcWBounds->right, lprcWBounds->bottom };
                    pict->Render(hdcDraw, dstX, dstY, picSize.cx, picSize.cy,
                            0L, picHeight, picWidth, -picHeight, &wBounds);
                }
                else
                    pict->Render(hdcDraw, dstX, dstY, picSize.cx, picSize.cy,
                            0L, picHeight, picWidth, -picHeight, NULL);

                pict->Release();
            }

            SelectObject(hdcDraw, GetStockObject(BLACK_BRUSH));

            MoveToEx(hdcDraw, bounds.left, bounds.top, NULL);
            LineTo(hdcDraw, bounds.left+width-1, bounds.top);
            LineTo(hdcDraw, bounds.left+width-1, bounds.top+height-1);
            LineTo(hdcDraw, bounds.left, bounds.top+height-1);
            LineTo(hdcDraw, bounds.left, bounds.top);
        }
    }
};

void VLCPlugin::onPaint(HDC hdc, const RECT &bounds, const RECT &clipRect)
{
    if( true )
    {
		OutputDebugStringA("Paint the object");
        /* if VLC is in design mode, draw control logo */
        HDC hdcDraw = CreateCompatibleDC(hdc);
        if( NULL != hdcDraw )
        {
            SIZEL size = getExtent();
            DPFromHimetric(hdc, (LPPOINT)&size, 1);
            RECTL posRect = { 0, 0, size.cx, size.cy };

            int width = bounds.right-bounds.left;
            int height = bounds.bottom-bounds.top;

            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
            if( NULL != hBitmap )
            {
                HBITMAP oldBmp = (HBITMAP)SelectObject(hdcDraw, hBitmap);

                if( (size.cx != width) || (size.cy != height) )
                {
                    // needs to scale canvas
                    SetMapMode(hdcDraw, MM_ANISOTROPIC);
                    SetWindowExtEx(hdcDraw, size.cx, size.cy, NULL);
                    SetViewportExtEx(hdcDraw, width, height, NULL);
                }

                onDraw(NULL, hdc, hdcDraw, &posRect, NULL);

                SetMapMode(hdcDraw, MM_TEXT);
                BitBlt(hdc, bounds.left, bounds.top,
                        width, height,
                        hdcDraw, 0, 0,
                        SRCCOPY);

                SelectObject(hdcDraw, oldBmp);
                DeleteObject(hBitmap);
            }
            DeleteDC(hdcDraw);
        }
    }
};

void VLCPlugin::onPositionChange(LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{

	OutputDebugStringA("Position Changed");
    RECT clipRect = *lprcClipRect;

    /*
    ** record keeping of control geometry within container
    */
    _posRect = *lprcPosRect;

    /*
    ** change in-place window geometry to match clipping region
    */
    SetWindowPos(_inplacewnd, NULL,
            lprcPosRect->left,
            lprcPosRect->top,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            SWP_NOACTIVATE|
            SWP_NOCOPYBITS|
            SWP_NOZORDER|
            SWP_NOOWNERZORDER );

    /* change cliprect coordinates system relative to window bounding rect */
    OffsetRect(&clipRect, -lprcPosRect->left, -lprcPosRect->top);
    HRGN clipRgn = CreateRectRgnIndirect(&clipRect);
    SetWindowRgn(_inplacewnd, clipRgn, FALSE);
};

void VLCPlugin::freezeEvents(BOOL freeze)
{
    vlcConnectionPointContainer->freezeEvents(freeze);
};

void VLCPlugin::firePropChangedEvent(DISPID dispid)
{
    vlcConnectionPointContainer->firePropChangedEvent(dispid);
};

void VLCPlugin::fireOnPlayEvent(void)
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
};

void VLCPlugin::fireOnPauseEvent(void)
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
};

void VLCPlugin::fireOnStopEvent(void)
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
};


void VLCPlugin::setTestStringEditBox()
{
#ifndef WITH_OUT_MFC
    SetWindowText(GetDlgItem(_inplacewnd, IDC_EDIT_TEST), getTestString());
#endif
}

void VLCPlugin::getTestStringEditBox()
{
#ifndef WITH_OUT_MFC
    TCHAR text[MAX_PATH] = {0};    
    GetWindowText(GetDlgItem(_inplacewnd, IDC_EDIT_TEST), text, MAX_PATH);    
    SysFreeString(_bstr_testString);
    _bstr_testString = SysAllocStringLen(text, MAX_PATH);     
#endif
}

void VLCPlugin::show()
{
   ShowWindow(_inplacewnd, SW_SHOWNORMAL);    
}

void VLCPlugin::hide()
{
   ShowWindow(_inplacewnd, SW_HIDE);        
}

void VLCPlugin::clearEditBox()
{
#ifndef WITH_OUT_MFC
   SetWindowText(GetDlgItem(_inplacewnd, IDC_EDIT_TEST), NULL);
#endif
}