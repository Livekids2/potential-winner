/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class declaration
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineWindowNormal_h__
#define __UIMachineWindowNormal_h__

/* Local includes: */
#include "UIMachineWindow.h"

/* Forward declarations: */
class CMediumAttachment;
class UIIndicatorsPool;

/* Normal machine-window implementation: */
class UIMachineWindowNormal : public UIMachineWindow
{
    Q_OBJECT;

protected:

    /* Constructor: */
    UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId);

private slots:

#ifdef Q_WS_X11
    /** X11: Performs machine-window async geometry normalization. */
    void sltNormalizeGeometry() { normalizeGeometry(true); }
#endif /* Q_WS_X11 */

    /* Session event-handlers: */
    void sltMachineStateChanged();
    void sltMediumChange(const CMediumAttachment &attachment);
    void sltUSBControllerChange();
    void sltUSBDeviceStateChange();
    void sltNetworkAdapterChange();
    void sltSharedFolderChange();
    void sltVideoCaptureChange();
    void sltCPUExecutionCapChange();

    /** Handles indicator context-menu-request: */
    void sltHandleIndicatorContextMenuRequest(IndicatorType indicatorType, const QPoint &position);

private:

    /* Prepare helpers: */
    void prepareSessionConnections();
    void prepareMenu();
    void prepareStatusBar();
    void prepareVisualState();
    void loadSettings();

    /* Cleanup helpers: */
    void saveSettings();
    //coid cleanupVisualState() {}
    //void cleanupStatusBar() {}
    //void cleanupMenu() {}
    //void cleanupConsoleConnections() {}

    /* Show stuff: */
    void showInNecessaryMode();

    /* Helper: Machine-window geometry stuff: */
    void normalizeGeometry(bool fAdjustPosition);

    /* Update stuff: */
    void updateAppearanceOf(int aElement);

    /* Event handler: */
    bool event(QEvent *pEvent);

    /* Helpers: */
    bool isMaximizedChecked();

    /** Holds the indicator-pool instance. */
    UIIndicatorsPool *m_pIndicatorsPool;

    /** Holds current window geometry. */
    QRect m_normalGeometry;

    /* Factory support: */
    friend class UIMachineWindow;
};

#endif // __UIMachineWindowNormal_h__

