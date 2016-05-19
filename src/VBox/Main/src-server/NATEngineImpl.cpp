/* $Id$ */
/** @file
 * Implementation of INATEngine in VBoxSVC.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "NATEngineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"
#include "MachineImpl.h"
#include "GuestOSTypeImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>
#include <VBox/com/array.h>

struct NATEngineData
{
    NATEngineData()
    {}

    settings::NAT s;
};

struct NATEngine::Data
{
    Backupable<NATEngineData> m;
};


// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

NATEngine::NATEngine():mData(NULL), mParent(NULL), mAdapter(NULL) {}
NATEngine::~NATEngine(){}

HRESULT NATEngine::FinalConstruct()
{
    return BaseFinalConstruct();
}

void NATEngine::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}


HRESULT NATEngine::init(Machine *aParent, INetworkAdapter *aAdapter)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    autoInitSpan.setSucceeded();
    mData = new Data();
    mData->m.allocate();
    mData->m->s.strNetwork.setNull();
    mData->m->s.strBindIP.setNull();
    unconst(mParent) = aParent;
    unconst(mAdapter) = aAdapter;
    return S_OK;
}

HRESULT NATEngine::init(Machine *aParent, INetworkAdapter *aAdapter, NATEngine *aThat)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    Log(("init that:%p this:%p\n", aThat, this));

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

    mData = new Data();
    mData->m.share(aThat->mData->m);
    unconst(mParent) = aParent;
    unconst(mAdapter) = aAdapter;
    unconst(mPeer) = aThat;
    autoInitSpan.setSucceeded();
    return S_OK;
}

HRESULT NATEngine::initCopy(Machine *aParent, INetworkAdapter *aAdapter, NATEngine *aThat)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    Log(("initCopy that:%p this:%p\n", aThat, this));

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

    mData = new Data();
    mData->m.attachCopy(aThat->mData->m);
    unconst(mAdapter) = aAdapter;
    unconst(mParent) = aParent;
    autoInitSpan.setSucceeded();

    return S_OK;
}


void NATEngine::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mData->m.free();
    delete mData;
    mData = NULL;
    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

bool NATEngine::i_isModified()
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    bool fModified = mData->m.isBackedUp();
    return fModified;
}

void NATEngine::i_rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m.rollback();
}

void NATEngine::i_commit()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(mPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);
    if (mData->m.isBackedUp())
    {
        mData->m.commit();
        if (mPeer)
            mPeer->mData->m.attach(mData->m);
    }
}

void NATEngine::i_copyFrom(NATEngine *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    mData->m.assignCopy(aThat->mData->m);
}

HRESULT NATEngine::getNetworkSettings(ULONG *aMtu, ULONG *aSockSnd, ULONG *aSockRcv, ULONG *aTcpWndSnd, ULONG *aTcpWndRcv)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (aMtu)
        *aMtu = mData->m->s.u32Mtu;
    if (aSockSnd)
        *aSockSnd = mData->m->s.u32SockSnd;
    if (aSockRcv)
        *aSockRcv = mData->m->s.u32SockRcv;
    if (aTcpWndSnd)
        *aTcpWndSnd = mData->m->s.u32TcpSnd;
    if (aTcpWndRcv)
        *aTcpWndRcv = mData->m->s.u32TcpRcv;

    return S_OK;
}

HRESULT NATEngine::setNetworkSettings(ULONG aMtu, ULONG aSockSnd, ULONG aSockRcv, ULONG aTcpWndSnd, ULONG aTcpWndRcv)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (   aMtu || aSockSnd || aSockRcv
        || aTcpWndSnd || aTcpWndRcv)
    {
        mData->m.backup();
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    if (aMtu)
        mData->m->s.u32Mtu = aMtu;
    if (aSockSnd)
        mData->m->s.u32SockSnd = aSockSnd;
    if (aSockRcv)
        mData->m->s.u32SockRcv = aSockSnd;
    if (aTcpWndSnd)
        mData->m->s.u32TcpSnd = aTcpWndSnd;
    if (aTcpWndRcv)
        mData->m->s.u32TcpRcv = aTcpWndRcv;

    return S_OK;
}


HRESULT NATEngine::getRedirects(std::vector<com::Utf8Str> &aRedirects)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aRedirects.resize(mData->m->s.mapRules.size());
    size_t i = 0;
    settings::NATRulesMap::const_iterator it;
    for (it = mData->m->s.mapRules.begin(); it != mData->m->s.mapRules.end(); ++it, ++i)
    {
        settings::NATRule r = it->second;
        aRedirects[i] = Utf8StrFmt("%s,%d,%s,%d,%s,%d",
                                   r.strName.c_str(),
                                   r.proto,
                                   r.strHostIP.c_str(),
                                   r.u16HostPort,
                                   r.strGuestIP.c_str(),
                                   r.u16GuestPort);
    }
    return S_OK;
}

HRESULT NATEngine::addRedirect(const com::Utf8Str &aName, NATProtocol_T aProto, const com::Utf8Str &aHostIP,
                               USHORT aHostPort, const com::Utf8Str &aGuestIP, USHORT aGuestPort)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str name = aName;
    settings::NATRule r;
    const char *proto;
    switch (aProto)
    {
        case NATProtocol_TCP:
            proto = "tcp";
            break;
        case NATProtocol_UDP:
            proto = "udp";
            break;
        default:
            return E_INVALIDARG;
    }
    if (name.isEmpty())
        name = Utf8StrFmt("%s_%d_%d", proto, aHostPort, aGuestPort);

    settings::NATRulesMap::iterator it;
    for (it = mData->m->s.mapRules.begin(); it != mData->m->s.mapRules.end(); ++it)
    {
        r = it->second;
        if (it->first == name)
            return setError(E_INVALIDARG,
                            tr("A NAT rule of this name already exists"));
        if (   r.strHostIP == aHostIP
            && r.u16HostPort == aHostPort
            && r.proto == aProto)
            return setError(E_INVALIDARG,
                            tr("A NAT rule for this host port and this host IP already exists"));
    }

    mData->m.backup();
    r.strName = name.c_str();
    r.proto = aProto;
    r.strHostIP = aHostIP;
    r.u16HostPort = aHostPort;
    r.strGuestIP = aGuestIP;
    r.u16GuestPort = aGuestPort;
    mData->m->s.mapRules.insert(std::make_pair(name, r));
    mParent->i_setModified(Machine::IsModified_NetworkAdapters);

    ULONG ulSlot;
    mAdapter->COMGETTER(Slot)(&ulSlot);

    alock.release();
    mParent->i_onNATRedirectRuleChange(ulSlot, FALSE, Bstr(name).raw(), aProto, Bstr(r.strHostIP).raw(),
                                       r.u16HostPort, Bstr(r.strGuestIP).raw(), r.u16GuestPort);
    return S_OK;
}

HRESULT NATEngine::removeRedirect(const com::Utf8Str &aName)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    settings::NATRulesMap::iterator it = mData->m->s.mapRules.find(aName);
    if (it == mData->m->s.mapRules.end())
        return E_INVALIDARG;
    mData->m.backup();
    /*
     * NB: "it" may now point to the backup!  In that case it's ok to
     * get data from the backup copy of s.mapRules via it, but we can't
     * erase(it) from potentially new s.mapRules.
     */
    settings::NATRule r = it->second;
    Utf8Str strHostIP = r.strHostIP;
    Utf8Str strGuestIP = r.strGuestIP;
    NATProtocol_T proto = r.proto;
    uint16_t u16HostPort = r.u16HostPort;
    uint16_t u16GuestPort = r.u16GuestPort;
    ULONG ulSlot;
    mAdapter->COMGETTER(Slot)(&ulSlot);

    mData->m->s.mapRules.erase(aName); /* NB: erase by key, "it" may not be valid */
    mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    alock.release();
    mParent->i_onNATRedirectRuleChange(ulSlot, TRUE, Bstr(aName).raw(), proto, Bstr(strHostIP).raw(),
                                       u16HostPort, Bstr(strGuestIP).raw(), u16GuestPort);
    return S_OK;
}

HRESULT NATEngine::i_loadSettings(const settings::NAT &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    mData->m->s = data;
    return S_OK;
}


HRESULT NATEngine::i_saveSettings(settings::NAT &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = S_OK;
    data = mData->m->s;
    return rc;
}

HRESULT NATEngine::setNetwork(const com::Utf8Str &aNetwork)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (mData->m->s.strNetwork != aNetwork)
    {
        mData->m.backup();
        mData->m->s.strNetwork = aNetwork;
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}


HRESULT NATEngine::getNetwork(com::Utf8Str &aNetwork)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->m->s.strNetwork.isEmpty())
    {
        aNetwork = mData->m->s.strNetwork;
        Log(("Getter (this:%p) Network: %s\n", this, mData->m->s.strNetwork.c_str()));
    }
    return S_OK;
}

HRESULT NATEngine::setHostIP(const com::Utf8Str &aHostIP)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (mData->m->s.strBindIP != aHostIP)
    {
        mData->m.backup();
        mData->m->s.strBindIP = aHostIP;
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}

HRESULT NATEngine::getHostIP(com::Utf8Str &aBindIP)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->m->s.strBindIP.isEmpty())
        aBindIP = mData->m->s.strBindIP;
    return S_OK;
}

HRESULT NATEngine::setTFTPPrefix(const com::Utf8Str &aTFTPPrefix)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (mData->m->s.strTFTPPrefix != aTFTPPrefix)
    {
        mData->m.backup();
        mData->m->s.strTFTPPrefix = aTFTPPrefix;
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}


HRESULT NATEngine::getTFTPPrefix(com::Utf8Str &aTFTPPrefix)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->m->s.strTFTPPrefix.isEmpty())
    {
        aTFTPPrefix = mData->m->s.strTFTPPrefix;
        Log(("Getter (this:%p) TFTPPrefix: %s\n", this, mData->m->s.strTFTPPrefix.c_str()));
    }
    return S_OK;
}

HRESULT NATEngine::setTFTPBootFile(const com::Utf8Str &aTFTPBootFile)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (mData->m->s.strTFTPBootFile != aTFTPBootFile)
    {
        mData->m.backup();
        mData->m->s.strTFTPBootFile = aTFTPBootFile;
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}


HRESULT NATEngine::getTFTPBootFile(com::Utf8Str &aTFTPBootFile)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->m->s.strTFTPBootFile.isEmpty())
    {
        aTFTPBootFile = mData->m->s.strTFTPBootFile;
        Log(("Getter (this:%p) BootFile: %s\n", this, mData->m->s.strTFTPBootFile.c_str()));
    }
    return S_OK;
}


HRESULT NATEngine::setTFTPNextServer(const com::Utf8Str &aTFTPNextServer)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (mData->m->s.strTFTPNextServer != aTFTPNextServer)
    {
        mData->m.backup();
        mData->m->s.strTFTPNextServer = aTFTPNextServer;
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}

HRESULT NATEngine::getTFTPNextServer(com::Utf8Str &aTFTPNextServer)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->m->s.strTFTPNextServer.isEmpty())
    {
        aTFTPNextServer =  mData->m->s.strTFTPNextServer;
        Log(("Getter (this:%p) NextServer: %s\n", this, mData->m->s.strTFTPNextServer.c_str()));
    }
    return S_OK;
}

/* DNS */
HRESULT NATEngine::setDNSPassDomain(BOOL aDNSPassDomain)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->m->s.fDNSPassDomain != RT_BOOL(aDNSPassDomain))
    {
        mData->m.backup();
        mData->m->s.fDNSPassDomain = RT_BOOL(aDNSPassDomain);
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}

HRESULT NATEngine::getDNSPassDomain(BOOL *aDNSPassDomain)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSPassDomain = mData->m->s.fDNSPassDomain;
    return S_OK;
}


HRESULT NATEngine::setDNSProxy(BOOL aDNSProxy)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->m->s.fDNSProxy != RT_BOOL(aDNSProxy))
    {
        mData->m.backup();
        mData->m->s.fDNSProxy = RT_BOOL(aDNSProxy);
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}

HRESULT NATEngine::getDNSProxy(BOOL *aDNSProxy)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSProxy = mData->m->s.fDNSProxy;
    return S_OK;
}


HRESULT NATEngine::getDNSUseHostResolver(BOOL *aDNSUseHostResolver)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSUseHostResolver = mData->m->s.fDNSUseHostResolver;
    return S_OK;
}


HRESULT NATEngine::setDNSUseHostResolver(BOOL aDNSUseHostResolver)
{
    if (mData->m->s.fDNSUseHostResolver != RT_BOOL(aDNSUseHostResolver))
    {
        mData->m.backup();
        mData->m->s.fDNSUseHostResolver = RT_BOOL(aDNSUseHostResolver);
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}

HRESULT NATEngine::setAliasMode(ULONG aAliasMode)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    ULONG uAliasMode = (mData->m->s.fAliasUseSamePorts ? NATAliasMode_AliasUseSamePorts : 0);
    uAliasMode |= (mData->m->s.fAliasLog ? NATAliasMode_AliasLog : 0);
    uAliasMode |= (mData->m->s.fAliasProxyOnly ? NATAliasMode_AliasProxyOnly : 0);
    if (uAliasMode != aAliasMode)
    {
        mData->m.backup();
        mData->m->s.fAliasUseSamePorts = RT_BOOL(aAliasMode & NATAliasMode_AliasUseSamePorts);
        mData->m->s.fAliasLog = RT_BOOL(aAliasMode & NATAliasMode_AliasLog);
        mData->m->s.fAliasProxyOnly = RT_BOOL(aAliasMode & NATAliasMode_AliasProxyOnly);
        mParent->i_setModified(Machine::IsModified_NetworkAdapters);
    }
    return S_OK;
}

HRESULT NATEngine::getAliasMode(ULONG *aAliasMode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    ULONG uAliasMode = (mData->m->s.fAliasUseSamePorts ? NATAliasMode_AliasUseSamePorts : 0);
    uAliasMode |= (mData->m->s.fAliasLog ? NATAliasMode_AliasLog : 0);
    uAliasMode |= (mData->m->s.fAliasProxyOnly ? NATAliasMode_AliasProxyOnly : 0);
    *aAliasMode = uAliasMode;
    return S_OK;
}
