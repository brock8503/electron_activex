/*****************************************************************************
 * vlccontrol2.cpp: Free ActiveX based on ActiveX control for VLC
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
#include "vlccontrol2.h"

#include "../misc/utils.h"

#include <shlwapi.h>
#include <wininet.h>
#include <tchar.h>


using namespace std;
/*******************************************************************************/

AXControl::AXControl(VLCPlugin *p_instance) :
    _p_instance(p_instance),
    _p_typeinfo(NULL)
{
};

AXControl::~AXControl()
{
    if( _p_typeinfo )
        _p_typeinfo->Release();
};

HRESULT AXControl::loadTypeInfo(void)
{
    HRESULT hr = NOERROR;
    if( NULL == _p_typeinfo )
    {
        ITypeLib *p_typelib;

        hr = _p_instance->getTypeLib(LOCALE_USER_DEFAULT, &p_typelib);
        if( SUCCEEDED(hr) )
        {
            hr = p_typelib->GetTypeInfoOfGuid(IID_IAXControl, &_p_typeinfo);
            if( FAILED(hr) )
            {
                _p_typeinfo = NULL;
            }
            p_typelib->Release();
        }
    }
    return hr;
};

STDMETHODIMP AXControl::GetTypeInfoCount(UINT* pctInfo)
{
    if( NULL == pctInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
        *pctInfo = 1;
    else
        *pctInfo = 0;

    return NOERROR;
};

STDMETHODIMP AXControl::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo)
{
    if( NULL == ppTInfo )
        return E_INVALIDARG;

    if( SUCCEEDED(loadTypeInfo()) )
    {
        _p_typeinfo->AddRef();
        *ppTInfo = _p_typeinfo;
        return NOERROR;
    }
    *ppTInfo = NULL;
    return E_NOTIMPL;
};

STDMETHODIMP AXControl::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames,
        UINT cNames, LCID lcid, DISPID* rgDispID)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispGetIDsOfNames(_p_typeinfo, rgszNames, cNames, rgDispID);
    }
    return E_NOTIMPL;
};

STDMETHODIMP AXControl::Invoke(DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if( SUCCEEDED(loadTypeInfo()) )
    {
        return DispInvoke(this, _p_typeinfo, dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
    }
    return E_NOTIMPL;
};

HRESULT STDMETHODCALLTYPE AXControl::put_TestString(BSTR str)
{
    _p_instance->setTestString(str);
    return S_OK;
};

HRESULT STDMETHODCALLTYPE AXControl::get_TestString(BSTR *str)
{
    if( NULL == str )
        return E_POINTER;

    _p_instance->getTestStringEditBox();
    *str = SysAllocStringLen(_p_instance->getTestString(),
                SysStringLen(_p_instance->getTestString()));
    return NOERROR;
};


HRESULT STDMETHODCALLTYPE AXControl::SetVisible(VARIANT_BOOL isShow)
{
  if(isShow)
  {
    _p_instance->show();
  }
  else
  {
    _p_instance->hide();
  }
  return NOERROR;
}