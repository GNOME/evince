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

#include "config.h"

#include "EvBrowserPlugin.h"
#include "EvMemoryUtils.h"
#include "npfunctions.h"
#include "npruntime.h"

static NPNetscapeFuncs *browser;
static unique_gptr<char> mimeDescription;

static EvBrowserPlugin *pluginForInstance(NPP instance)
{
        if (!instance)
                return nullptr;

        return static_cast<EvBrowserPlugin *>(instance->pdata);
}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *savedData)
{
        if (!instance)
                return NPERR_INVALID_INSTANCE_ERROR;

        return EvBrowserPlugin::create(instance)->initialize(pluginType, mode, argc, argn, argv, savedData);
}

NPError NPP_Destroy(NPP instance, NPSavedData **saveData)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return NPERR_INVALID_INSTANCE_ERROR;

        browser->releaseobject(static_cast<NPObject *>(plugin));
        return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow *window)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return NPERR_INVALID_INSTANCE_ERROR;

        return plugin->setWindow(window);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream, NPBool seekable, uint16_t *stype)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return NPERR_INVALID_INSTANCE_ERROR;

        return plugin->newStream(type, stream, seekable, stype);
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return NPERR_INVALID_INSTANCE_ERROR;

        return plugin->destroyStream(stream, reason);
}

void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return;

        return plugin->streamAsFile(stream, fname);
}

int32_t NPP_WriteReady(NPP instance, NPStream *stream)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return -1;

        return plugin->writeReady(stream);
}

int32_t NPP_Write(NPP instance, NPStream *stream, int32_t offset, int32_t len, void *buffer)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return -1;

        return plugin->write(stream, offset, len, buffer);
}

void NPP_Print(NPP instance, NPPrint *platformPrint)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return;

        return plugin->print(platformPrint);
}

int16_t NPP_HandleEvent(NPP instance, void *event)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return 0;

        return plugin->handleEvent(static_cast<XEvent *>(event));
}

void NPP_URLNotify(NPP instance, const char *url, NPReason reason, void *notifyData)
{
        EvBrowserPlugin *plugin = pluginForInstance(instance);
        if (!plugin)
                return;

        return plugin->urlNotify(url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
        switch (variable) {
        case NPPVpluginNameString:
                *((char **)value) = const_cast<char *>(EvBrowserPlugin::nameString());
                return NPERR_NO_ERROR;
        case NPPVpluginDescriptionString:
                *((char **)value) = const_cast<char *>(EvBrowserPlugin::descriptionString());
                return NPERR_NO_ERROR;
        case NPPVpluginNeedsXEmbed:
                *((NPBool *)value) = TRUE;
                return NPERR_NO_ERROR;
        case NPPVpluginScriptableNPObject: {
                EvBrowserPlugin *plugin = pluginForInstance(instance);
                if (!plugin)
                        return NPERR_INVALID_PLUGIN_ERROR;

                browser->retainobject(static_cast<NPObject *>(plugin));
                *((NPObject **)value) = plugin;
                return NPERR_NO_ERROR;
        }
        default:
                return NPERR_INVALID_PARAM;
        }

        return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP, NPNVariable, void *)
{
        return NPERR_NO_ERROR;
}

static void initializePluginFuncs(NPPluginFuncs *pluginFuncs)
{
        pluginFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
        pluginFuncs->size = sizeof(pluginFuncs);
        pluginFuncs->newp = NPP_New;
        pluginFuncs->destroy = NPP_Destroy;
        pluginFuncs->setwindow = NPP_SetWindow;
        pluginFuncs->newstream = NPP_NewStream;
        pluginFuncs->destroystream = NPP_DestroyStream;
        pluginFuncs->asfile = NPP_StreamAsFile;
        pluginFuncs->writeready = NPP_WriteReady;
        pluginFuncs->write = NPP_Write;
        pluginFuncs->print = NPP_Print;
        pluginFuncs->event = NPP_HandleEvent;
        pluginFuncs->urlnotify = NPP_URLNotify;
        pluginFuncs->getvalue = NPP_GetValue;
        pluginFuncs->setvalue = NPP_SetValue;
}

NPError NP_Initialize(NPNetscapeFuncs *browserFuncs, NPPluginFuncs *pluginFuncs)
{
        if (!browserFuncs || !pluginFuncs)
                return NPERR_INVALID_FUNCTABLE_ERROR;

        if ((browserFuncs->version >> 8) > NP_VERSION_MAJOR)
                return NPERR_INCOMPATIBLE_VERSION_ERROR;

        if (!ev_init())
                return NPERR_GENERIC_ERROR;

        gtk_init(nullptr, nullptr);

        browser = browserFuncs;
        initializePluginFuncs(pluginFuncs);

        GBytes *resourceData = g_resources_lookup_data("/org/gnome/evince/browser/ui/evince-browser.css", G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
        if (resourceData) {
            GtkCssProvider *cssProvider = gtk_css_provider_new();

            gtk_css_provider_load_from_data(cssProvider, static_cast<const gchar *>(g_bytes_get_data(resourceData, nullptr)), g_bytes_get_size(resourceData), nullptr);
            g_bytes_unref(resourceData);

            gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            g_object_unref(cssProvider);
        }

        return NPERR_NO_ERROR;
}

NPError NP_Shutdown()
{
        ev_shutdown();
        return NPERR_NO_ERROR;
}

static const struct {
        const char *mime;
        const char *extensions;
} mimeExtensions[] = {
        { "application/postscript", "ps" },
        { "application/x-ext-ps", "ps" },
        { "application/x-bzpostscript", "ps.bz2" },
        { "application/x-gzpostscript", "ps.gz" },
        { "image/x-eps", "eps,epsi,epsf" },
        { "application/x-ext-eps", "eps,epsi,epsf" },
        { "image/x-bzeps", "eps.bz2,epsi.bz2,epsf.bz2" },
        { "image/x-gzeps", "eps.gz,epsi.gz,epsf.gz" },
        { "image/tiff", "tif,tiff" },
        { "application/pdf", "pdf" },
        { "application/x-ext-pdf", "pdf" },
        { "application/x-bzpdf", "pdf.bz2" },
        { "application/x-gzpdf", "pdf.gz" },
        { "application/x-xzpdf", "pdf.xz" },
        { "application/x-dvi", "dvi" },
        { "application/x-ext-dvi", "dvi" },
        { "application/x-bzdvi", "dvi.bz2" },
        { "application/x-gzdvi", "dvi.gz" },
        { "application/x-cbr", "cbr" },
        { "application/x-ext-cbr", "cbr" },
        { "application/x-cbz", "cbz" },
        { "application/x-ext-cbz", "cbz" },
        { "application/x-cb7", "cb7" },
        { "application/x-ext-cb7", "cb7" },
        { "application/x-cbt", "cbt" },
        { "application/x-ext-cbt", "cbt" },
        { "image/vnd.djvu", "djvu,djv" },
        { "application/x-ext-djv", "djv" },
        { "application/x-ext-djvu", "djvu" },
        { "application/oxps", "xps,oxps" },
        { "application/vnd.ms-xpsdocument", "xps,oxps" }
};

const char *NP_GetMIMEDescription()
{
        if (mimeDescription)
                return mimeDescription.get();

        if (!ev_init())
                return nullptr;

        GString *mimeDescriptionStr = g_string_new(nullptr);

        GList *typesInfo = ev_backends_manager_get_all_types_info();
        for (GList *l = typesInfo; l; l = g_list_next(l)) {
                EvTypeInfo *info = static_cast<EvTypeInfo *>(l->data);

                for (unsigned i = 0; info->mime_types[i]; ++i) {
                        const char *extensions = [](const char *mime) -> const char * {
                                for (unsigned i = 0; i < G_N_ELEMENTS(mimeExtensions); ++i) {
                                        if (g_ascii_strcasecmp(mimeExtensions[i].mime, mime) == 0)
                                                return mimeExtensions[i].extensions;
                                }

                                return nullptr;
                        }(info->mime_types[i]);

                        if (!extensions)
                                continue;

                        g_string_append_printf(mimeDescriptionStr, "%s:%s:%s;",
                                               info->mime_types[i],
                                               extensions,
                                               info->desc);
                }
        }
        g_list_free(typesInfo);

        mimeDescription.reset(g_string_free(mimeDescriptionStr, FALSE));

        ev_shutdown();

        return mimeDescription.get();
}

NPError NP_GetValue(void *, NPPVariable variable, void *value)
{
        return NPP_GetValue(nullptr, variable, value);
}

NPObject *NPN_CreateObject(NPP instance, NPClass *npClass)
{
        return browser->createobject(instance, npClass);
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
        browser->getstringidentifiers(names, nameCount, identifiers);
}

NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
        return browser->geturl(instance, url, target);
}

const char *NPN_UserAgent(NPP instance)
{
        return browser->uagent(instance);
}

void *NPN_MemAlloc(uint32_t size)
{
        return browser->memalloc(size);
}

void NPN_MemFree(void* ptr)
{
        if (ptr)
                browser->memfree(ptr);
}

static void __attribute__((constructor)) browserPluginConstructor()
{
        ev_init();
}

static void __attribute__((destructor)) browserPluginDestructor()
{
        ev_shutdown();
}
