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

#include "EvBrowserPluginToolbar.h"
#include "npfunctions.h"
#include <errno.h>
#include <gtk/gtkx.h>
#include <limits>
#include <string.h>

struct EvBrowserPluginClass {
        enum Methods {
                GoToPage,
                ToggleContinuous,
                ToggleDual,
                ZoomIn,
                ZoomOut,
                Download,
                Print,

                NumMethodIdentifiers
        };

        enum Properties {
                CurrentPage,
                PageCount,
                Zoom,
                ZoomMode,
                Continuous,
                Dual,
                Toolbar,

                NumPropertyIdentifiers
        };

        EvBrowserPlugin* createObject(NPP instance)
        {
                if (!identifiersInitialized) {
                        NPN_GetStringIdentifiers(methodIdentifierNames, NumMethodIdentifiers, methodIdentifiers);
                        NPN_GetStringIdentifiers(propertyIdentifierNames, NumPropertyIdentifiers, propertyIdentifiers);
                        identifiersInitialized = true;
                }

                return static_cast<EvBrowserPlugin *>(NPN_CreateObject(instance, &npClass));
        }

        NPClass npClass;

        const NPUTF8 *methodIdentifierNames[NumMethodIdentifiers];
        const NPUTF8 *propertyIdentifierNames[NumPropertyIdentifiers];
        bool identifiersInitialized;
        NPIdentifier methodIdentifiers[NumMethodIdentifiers];
        NPIdentifier propertyIdentifiers[NumPropertyIdentifiers];
};

EvBrowserPlugin *EvBrowserPlugin::create(NPP instance)
{
        return s_pluginClass.createObject(instance);
}

const char *EvBrowserPlugin::nameString()
{
        return "Evince Browser Plugin";
}

const char *EvBrowserPlugin::descriptionString()
{
        return "The <a href=\"http://wiki.gnome.org/Apps/Evince/\">Evince</a> " PACKAGE_VERSION " plugin handles documents inside the browser window.";
}

EvBrowserPlugin::EvBrowserPlugin(NPP instance)
        : m_NPP(instance)
        , m_window(nullptr)
        , m_model(nullptr)
        , m_view(nullptr)
        , m_toolbar(nullptr)
{
        m_NPP->pdata = this;
}

EvBrowserPlugin::~EvBrowserPlugin()
{
        if (m_window)
                gtk_widget_destroy(m_window);
        g_clear_object(&m_model);
        m_NPP->pdata = nullptr;
}

template <typename IntegerType>
static inline void parseInteger(const char *strValue, IntegerType &intValue)
{
        static const IntegerType intMax = std::numeric_limits<IntegerType>::max();
        static const bool isSigned = std::numeric_limits<IntegerType>::is_signed;

        if (!strValue)
                return;

        char *endPtr = nullptr;
        errno = 0;
        gint64 value = isSigned ? g_ascii_strtoll(strValue, &endPtr, 0) : g_ascii_strtoull(strValue, &endPtr, 0);
        if (endPtr != strValue && errno == 0 && value <= intMax)
                intValue = static_cast<IntegerType>(value);
}

static inline void parseDouble(const char *strValue, double &doubleValue)
{
        if (!strValue)
                return;

        char *endPtr = nullptr;
        errno = 0;
        double value = g_ascii_strtod(strValue, &endPtr);
        if (endPtr != strValue && errno == 0)
                doubleValue = value;
}

static inline void parseBoolean(const char *strValue, bool &boolValue)
{
        if (!strValue)
                return;

        unique_gptr<char> value(g_ascii_strdown(strValue, -1));
        if (g_ascii_strcasecmp(value.get(), "false") == 0 || g_ascii_strcasecmp(value.get(), "no") == 0)
                boolValue = false;
        else if (g_ascii_strcasecmp(value.get(), "true") == 0 || g_ascii_strcasecmp(value.get(), "yes") == 0)
                boolValue = true;
        else {
                int intValue = boolValue;
                parseInteger<int>(strValue, intValue);
                boolValue = intValue > 0;
        }
}

static inline void parseZoomMode(const char *strValue, EvSizingMode &sizingModeValue)
{
        if (!strValue)
                return;

        unique_gptr<char> value(g_ascii_strdown(strValue, -1));
        if (g_ascii_strcasecmp(value.get(), "none") == 0)
                sizingModeValue = EV_SIZING_FREE;
        else if (g_ascii_strcasecmp(value.get(), "fit-page") == 0)
                sizingModeValue = EV_SIZING_FIT_PAGE;
        else if (g_ascii_strcasecmp(value.get(), "fit-width") == 0)
                sizingModeValue = EV_SIZING_FIT_WIDTH;
        else if (g_ascii_strcasecmp(value.get(), "auto") == 0)
                sizingModeValue = EV_SIZING_AUTOMATIC;
}

NPError EvBrowserPlugin::initialize(NPMIMEType, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *)
{
        // Default values.
        bool toolbarVisible = true;
        unsigned currentPage = 1;
        EvSizingMode sizingMode = EV_SIZING_AUTOMATIC;
        bool continuous = true;
        bool dual = false;
        double zoom = 0;

        for (int16_t i = 0; i < argc; ++i) {
                if (g_ascii_strcasecmp(argn[i], "toolbar") == 0)
                        parseBoolean(argv[i], toolbarVisible);
                else if (g_ascii_strcasecmp(argn[i], "currentpage") == 0)
                        parseInteger<unsigned>(argv[i], currentPage);
                else if (g_ascii_strcasecmp(argn[i], "zoom") == 0)
                        parseDouble(argv[i], zoom);
                else if (g_ascii_strcasecmp(argn[i], "zoommode") == 0)
                        parseZoomMode(argv[i], sizingMode);
                else if (g_ascii_strcasecmp(argn[i], "continuous") == 0)
                        parseBoolean(argv[i], continuous);
                else if (g_ascii_strcasecmp(argn[i], "dual") == 0)
                        parseBoolean(argv[i], dual);
        }

        m_model = ev_document_model_new();
        if (currentPage > 0)
                ev_document_model_set_page(m_model, currentPage - 1);
        ev_document_model_set_continuous(m_model, continuous);
        ev_document_model_set_page_layout(m_model, dual ? EV_PAGE_LAYOUT_DUAL : EV_PAGE_LAYOUT_SINGLE);
        if (zoom) {
                ev_document_model_set_sizing_mode(m_model, EV_SIZING_FREE);
                ev_document_model_set_scale(m_model, zoom);
        } else
                ev_document_model_set_sizing_mode(m_model, sizingMode);

        m_view = EV_VIEW(ev_view_new());
        ev_view_set_model(m_view, m_model);

        m_toolbar = ev_browser_plugin_toolbar_new(this);
        if (toolbarVisible)
                gtk_widget_show(m_toolbar);

        return NPERR_NO_ERROR;
}

NPError EvBrowserPlugin::setWindow(NPWindow *window)
{
        if (!m_window) {
                m_window = gtk_plug_new(reinterpret_cast<Window>(window->window));
                gtk_widget_realize(m_window);

                GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_box_pack_start(GTK_BOX(vbox), m_toolbar, FALSE, FALSE, 0);

                GtkWidget *scrolledWindow = gtk_scrolled_window_new(nullptr, nullptr);
                gtk_container_add(GTK_CONTAINER(scrolledWindow), GTK_WIDGET(m_view));
                gtk_widget_show(GTK_WIDGET(m_view));

                gtk_box_pack_start(GTK_BOX(vbox), scrolledWindow, TRUE, TRUE, 0);
                gtk_widget_show(scrolledWindow);

                gtk_container_add(GTK_CONTAINER(m_window), vbox);
                gtk_widget_show(vbox);
        }

        gtk_widget_set_size_request(m_window, window->width, window->height);
        gtk_widget_show(m_window);

        return NPERR_NO_ERROR;
}

NPError EvBrowserPlugin::newStream(NPMIMEType, NPStream *stream, NPBool seekable, uint16_t *stype)
{
        m_url.reset(g_strdup(stream->url));
        *stype = NP_ASFILEONLY;
        return NPERR_NO_ERROR;
}

NPError EvBrowserPlugin::destroyStream(NPStream *, NPReason)
{
        return NPERR_NO_ERROR;
}

void EvBrowserPlugin::streamAsFile(NPStream *, const char *fname)
{
        GFile *file = g_file_new_for_commandline_arg(fname);
        unique_gptr<char> uri(g_file_get_uri(file));
        g_object_unref(file);

        // Load the document synchronously here because the temporary file created by the browser
        // is deleted when this function returns.
        GError *error = nullptr;
        EvDocument *document = ev_document_factory_get_document(uri.get(), &error);
        if (!document) {
                g_printerr("Error loading document %s: %s\n", uri.get(), error->message);
                g_error_free(error);
        } else {
                ev_document_model_set_document(m_model, document);
                g_object_unref(document);

                ev_view_set_loading(EV_VIEW(m_view), FALSE);
        }
}

int32_t EvBrowserPlugin::writeReady(NPStream *)
{
        return 0;
}

int32_t EvBrowserPlugin::write(NPStream *, int32_t /*offset*/, int32_t /*len*/, void */*buffer*/)
{
        return 0;
}

void EvBrowserPlugin::print(NPPrint *)
{

}

int16_t EvBrowserPlugin::handleEvent(XEvent *)
{
        return 0;
}

void EvBrowserPlugin::urlNotify(const char */*url*/, NPReason, void */*notifyData*/)
{

}

unsigned EvBrowserPlugin::currentPage() const
{
        g_return_val_if_fail(EV_IS_DOCUMENT_MODEL(m_model), 0);
        return ev_document_model_get_page(m_model);
}

unsigned EvBrowserPlugin::pageCount() const
{
        g_return_val_if_fail(EV_IS_DOCUMENT_MODEL(m_model), 0);
        EvDocument *document = ev_document_model_get_document(m_model);
        return document ? ev_document_get_n_pages(document) : 0;
}

double EvBrowserPlugin::zoom() const
{
        g_return_val_if_fail(EV_IS_DOCUMENT_MODEL(m_model), 1);
        return ev_document_model_get_scale(m_model);
}

void EvBrowserPlugin::setZoom(double scale)
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_sizing_mode(m_model, EV_SIZING_FREE);
        ev_document_model_set_scale(m_model, scale);
}

void EvBrowserPlugin::goToPreviousPage()
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_page(m_model, ev_document_model_get_page(m_model) - 1);
}

void EvBrowserPlugin::goToNextPage()
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_page(m_model, ev_document_model_get_page(m_model) + 1);
}

void EvBrowserPlugin::goToPage(unsigned page)
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_page(m_model, page - 1);
}

void EvBrowserPlugin::goToPage(const char *pageLabel)
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_page_by_label(m_model, pageLabel);
}

void EvBrowserPlugin::activateLink(EvLink *link)
{
        g_return_if_fail(EV_IS_VIEW(m_view));
        g_return_if_fail(EV_IS_LINK(link));
        ev_view_handle_link(m_view, link);
        gtk_widget_grab_focus(GTK_WIDGET(m_view));
}

bool EvBrowserPlugin::isContinuous() const
{
        g_return_val_if_fail(EV_IS_DOCUMENT_MODEL(m_model), false);
        return ev_document_model_get_continuous(m_model);
}

void EvBrowserPlugin::setContinuous(bool continuous)
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_continuous(m_model, continuous);
}

void EvBrowserPlugin::toggleContinuous()
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_continuous(m_model, !ev_document_model_get_continuous(m_model));
}

bool EvBrowserPlugin::isDual() const
{
        g_return_val_if_fail(EV_IS_DOCUMENT_MODEL(m_model), false);
        return ev_document_model_get_page_layout(m_model) == EV_PAGE_LAYOUT_DUAL;
}

void EvBrowserPlugin::setDual(bool dual)
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_page_layout(m_model, dual ? EV_PAGE_LAYOUT_DUAL : EV_PAGE_LAYOUT_SINGLE);
}

void EvBrowserPlugin::toggleDual()
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_page_layout(m_model, isDual() ? EV_PAGE_LAYOUT_SINGLE : EV_PAGE_LAYOUT_DUAL);
}

void EvBrowserPlugin::zoomIn()
{
        g_return_if_fail(EV_IS_VIEW(m_view));
        ev_document_model_set_sizing_mode(m_model, EV_SIZING_FREE);
        ev_view_zoom_in(m_view);
}

void EvBrowserPlugin::zoomOut()
{
        g_return_if_fail(EV_IS_VIEW(m_view));
        ev_document_model_set_sizing_mode(m_model, EV_SIZING_FREE);
        ev_view_zoom_out(m_view);
}

EvSizingMode EvBrowserPlugin::sizingMode() const
{
        g_return_val_if_fail(EV_IS_DOCUMENT_MODEL(m_model), EV_SIZING_FREE);
        return ev_document_model_get_sizing_mode(m_model);
}

void EvBrowserPlugin::setSizingMode(EvSizingMode sizingMode)
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));
        ev_document_model_set_sizing_mode(m_model, sizingMode);
}

void EvBrowserPlugin::download() const
{
        g_return_if_fail(m_url);
        // Since I don't know how to force a download in the browser, I use
        // a special frame name here that Epiphany will check in the new window policy
        // callback to start the download.
        NPN_GetURL(m_NPP, m_url.get(), "_evince_download");
}

void EvBrowserPlugin::print() const
{
        g_return_if_fail(EV_IS_DOCUMENT_MODEL(m_model));

        EvDocument *document = ev_document_model_get_document(m_model);
        if (!document)
                return;

        EvPrintOperation *printOperation = ev_print_operation_new(document);
        if (!printOperation)
                return;

        unique_gptr<char> outputBasename(g_path_get_basename(m_url.get()));
        if (char *dot = g_strrstr(outputBasename.get(), "."))
                dot[0] = '\0';

        unique_gptr<char> unescapedBasename(g_uri_unescape_string(outputBasename.get(), nullptr));
        // Set output basename for printing to file.
        GtkPrintSettings *printSettings = gtk_print_settings_new();
        gtk_print_settings_set(printSettings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, unescapedBasename.get());

        if (const char *title = ev_document_get_title(document))
                ev_print_operation_set_job_name(printOperation, title);
        ev_print_operation_set_current_page(printOperation, ev_document_model_get_page(m_model));
        ev_print_operation_set_embed_page_setup (printOperation, TRUE);
        ev_print_operation_set_print_settings(printOperation, printSettings);
        g_object_unref(printSettings);

        g_signal_connect(printOperation, "done", G_CALLBACK(g_object_unref), nullptr);

        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(m_view));
        ev_print_operation_run(printOperation, GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr);
}

bool EvBrowserPlugin::canDownload() const
{
        // Download is only available for Epiphany for now.
        return g_strrstr(NPN_UserAgent(m_NPP), "Epiphany");
}

bool EvBrowserPlugin::toolbarVisible() const
{
        g_return_val_if_fail(EV_IS_BROWSER_PLUGIN_TOOLBAR(m_toolbar), false);
        return gtk_widget_get_visible(m_toolbar);
}

void EvBrowserPlugin::setToolbarVisible(bool isVisible)
{
        g_return_if_fail(EV_IS_BROWSER_PLUGIN_TOOLBAR(m_toolbar));
        if (isVisible)
                gtk_widget_show(m_toolbar);
        else
                gtk_widget_hide(m_toolbar);
}

// Scripting interface
NPObject *EvBrowserPlugin::allocate(NPP instance, NPClass *)
{
        return new EvBrowserPlugin(instance);
}

void EvBrowserPlugin::deallocate(NPObject *npObject)
{
        delete static_cast<EvBrowserPlugin *>(npObject);
}

void EvBrowserPlugin::invalidate(NPObject *)
{
}

bool EvBrowserPlugin::hasMethod(NPObject *npObject, NPIdentifier name)
{
        for (unsigned i = 0; i < EvBrowserPluginClass::Methods::NumMethodIdentifiers; ++i) {
                if (name == s_pluginClass.methodIdentifiers[i]) {
                        if (i == EvBrowserPluginClass::Methods::Download)
                                return static_cast<EvBrowserPlugin *>(npObject)->canDownload();
                        return true;
                }
        }
        return false;
}

bool EvBrowserPlugin::invoke(NPObject *npObject, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
        EvBrowserPlugin *plugin = static_cast<EvBrowserPlugin *>(npObject);

        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::GoToPage]) {
                if (argCount != 1)
                        return false;

                if (NPVARIANT_IS_DOUBLE(args[0]))
                        plugin->goToPage(static_cast<unsigned>(NPVARIANT_TO_DOUBLE(args[0])));
                else if (NPVARIANT_IS_STRING(args[0])) {
                        unique_gptr<char> pageLabel(g_strndup(NPVARIANT_TO_STRING(args[0]).UTF8Characters, NPVARIANT_TO_STRING(args[0]).UTF8Length));
                        plugin->goToPage(pageLabel.get());
                } else
                        return false;

                VOID_TO_NPVARIANT(*result);
                return true;
        }
        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::ToggleContinuous]) {
                plugin->toggleContinuous();
                VOID_TO_NPVARIANT(*result);
                return true;
        }
        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::ToggleDual]) {
                plugin->toggleDual();
                VOID_TO_NPVARIANT(*result);
                return true;
        }
        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::ZoomIn]) {
                plugin->zoomIn();
                VOID_TO_NPVARIANT(*result);
                return true;
        }
        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::ZoomOut]) {
                plugin->zoomOut();
                VOID_TO_NPVARIANT(*result);
                return true;
        }
        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::Download]) {
                plugin->download();
                VOID_TO_NPVARIANT(*result);
                return true;
        }
        if (name == s_pluginClass.methodIdentifiers[EvBrowserPluginClass::Methods::Print]) {
                plugin->print();
                VOID_TO_NPVARIANT(*result);
                return true;
        }
        return false;
}

bool EvBrowserPlugin::hasProperty(NPObject *npObject, NPIdentifier name)
{
        for (unsigned i = 0; i < EvBrowserPluginClass::Properties::NumPropertyIdentifiers; ++i) {
                if (name == s_pluginClass.propertyIdentifiers[i])
                        return true;
        }
        return false;
}

bool EvBrowserPlugin::getProperty(NPObject *npObject, NPIdentifier name, NPVariant *value)
{
        EvBrowserPlugin *plugin = static_cast<EvBrowserPlugin *>(npObject);

        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::CurrentPage]) {
                INT32_TO_NPVARIANT(plugin->currentPage() + 1, *value);
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::PageCount]) {
                INT32_TO_NPVARIANT(plugin->pageCount(), *value);
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Zoom]) {
                DOUBLE_TO_NPVARIANT(plugin->zoom(), *value);
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::ZoomMode]) {
                const char *zoomMode;

                switch (plugin->sizingMode()) {
                case EV_SIZING_FREE:
                        zoomMode = "none";
                        break;
                case EV_SIZING_FIT_PAGE:
                        zoomMode = "fit-page";
                        break;
                case EV_SIZING_FIT_WIDTH:
                        zoomMode = "fit-width";
                        break;
                case EV_SIZING_AUTOMATIC:
                        zoomMode = "auto";
                        break;
                default:
                        return false;
                }

                size_t zoomModeLength = strlen(zoomMode);
                char *result = static_cast<char *>(NPN_MemAlloc(zoomModeLength + 1));
                memcpy(result, zoomMode, zoomModeLength);
                result[zoomModeLength] = '\0';

                STRINGZ_TO_NPVARIANT(result, *value);

                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Continuous]) {
                BOOLEAN_TO_NPVARIANT(plugin->isContinuous(), *value);
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Dual]) {
                BOOLEAN_TO_NPVARIANT(plugin->isDual(), *value);
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Toolbar]) {
                BOOLEAN_TO_NPVARIANT(plugin->toolbarVisible(), *value);
                return true;
        }

        return false;
}

bool EvBrowserPlugin::setProperty(NPObject *npObject, NPIdentifier name, const NPVariant *value)
{
        EvBrowserPlugin *plugin = static_cast<EvBrowserPlugin *>(npObject);

        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::CurrentPage]) {
                plugin->goToPage(static_cast<unsigned>(NPVARIANT_TO_DOUBLE(*value)));
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Zoom]) {
                plugin->setZoom(NPVARIANT_TO_DOUBLE(*value));
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::ZoomMode]) {
                unique_gptr<char> zoomMode(g_strndup(NPVARIANT_TO_STRING(*value).UTF8Characters, NPVARIANT_TO_STRING(*value).UTF8Length));

                if (g_strcmp0(zoomMode.get(), "none") == 0)
                        plugin->setSizingMode(EV_SIZING_FREE);
                else if (g_strcmp0(zoomMode.get(), "fit-page") == 0)
                        plugin->setSizingMode(EV_SIZING_FIT_PAGE);
                else if (g_strcmp0(zoomMode.get(), "fit-width") == 0)
                        plugin->setSizingMode(EV_SIZING_FIT_WIDTH);
                else if (g_strcmp0(zoomMode.get(), "auto") == 0)
                        plugin->setSizingMode(EV_SIZING_AUTOMATIC);
                else
                        return false;

                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Continuous]) {
                plugin->setContinuous(NPVARIANT_TO_BOOLEAN(*value));
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Dual]) {
                plugin->setDual(NPVARIANT_TO_BOOLEAN(*value));
                return true;
        }
        if (name == s_pluginClass.propertyIdentifiers[EvBrowserPluginClass::Properties::Toolbar]) {
                plugin->setToolbarVisible(NPVARIANT_TO_BOOLEAN(*value));
                return true;
        }

        return false;
}

EvBrowserPluginClass EvBrowserPlugin::s_pluginClass {
        {
                NP_CLASS_STRUCT_VERSION,
                allocate,
                deallocate,
                invalidate,
                hasMethod,
                invoke,
                nullptr, // NPClass::invokeDefault
                hasProperty,
                getProperty,
                setProperty,
                nullptr, // NPClass::removeProperty
                nullptr, // NPClass::enumerate
                nullptr // NPClass::construct
        },
        // methodIdentifierNames
        {
                "goToPage",
                "toggleContinuous",
                "toggleDual",
                "zoomIn",
                "zoomOut",
                "download",
                "print"
        },
        // propertyIdentifierNames
        {
                "currentPage",
                "pageCount",
                "zoom",
                "zoomMode",
                "continuous",
                "dual",
                "toolbar"
        },
        false // identifiersInitialized
};
