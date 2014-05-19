/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef EvBrowserPlugin_h
#define EvBrowserPlugin_h

#include <evince-document.h>
#include <evince-view.h>
#include "EvMemoryUtils.h"
#include "npapi.h"
#include "npruntime.h"

typedef union _XEvent XEvent;
struct EvBrowserPluginClass;

class EvBrowserPlugin: public NPObject {
public:
        static EvBrowserPlugin* create(NPP);

        static const char *nameString();
        static const char *descriptionString();

        // NPP API
        NPError initialize(NPMIMEType, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *);
        NPError setWindow(NPWindow *);
        NPError newStream(NPMIMEType, NPStream *, NPBool seekable, uint16_t *stype);
        NPError destroyStream(NPStream *, NPReason);
        void streamAsFile(NPStream *, const char *fname);
        int32_t writeReady(NPStream *);
        int32_t write(NPStream *, int32_t offset, int32_t len, void *buffer);
        void print(NPPrint *);
        int16_t handleEvent(XEvent *);
        void urlNotify(const char *url, NPReason, void *notifyData);

        // Viewer API
        EvDocumentModel *model() const { return m_model; }
        unsigned currentPage() const;
        unsigned pageCount() const;
        double zoom() const;
        void setZoom(double scale);
        void goToPreviousPage();
        void goToNextPage();
        void goToPage(unsigned page);
        void goToPage(const char *pageLabel);
        void activateLink(EvLink *);
        bool isContinuous() const;
        void setContinuous(bool);
        void toggleContinuous();
        bool isDual() const;
        void setDual(bool);
        void toggleDual();
        void zoomIn();
        void zoomOut();
        EvSizingMode sizingMode() const;
        void setSizingMode(EvSizingMode);
        void download() const;
        void print() const;
        bool toolbarVisible() const;
        void setToolbarVisible(bool);

        bool canDownload() const;

private:
        EvBrowserPlugin(NPP);
        virtual ~EvBrowserPlugin();

        // Scripting interface
        static NPObject *allocate(NPP, NPClass *);
        static void deallocate(NPObject *);
        static void invalidate(NPObject *);
        static bool hasMethod(NPObject *, NPIdentifier name);
        static bool invoke(NPObject *, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
        static bool hasProperty(NPObject *, NPIdentifier name);
        static bool getProperty(NPObject *, NPIdentifier name, NPVariant *);
        static bool setProperty(NPObject *, NPIdentifier name, const NPVariant *);

        NPP m_NPP;
        GtkWidget *m_window;
        EvDocumentModel *m_model;
        EvView *m_view;
        GtkWidget *m_toolbar;
        unique_gptr<char> m_url;

        static EvBrowserPluginClass s_pluginClass;
};

#endif // EvBrowserPlugin_h
