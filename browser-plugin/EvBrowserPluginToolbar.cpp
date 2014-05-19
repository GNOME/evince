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
#include "EvBrowserPluginToolbar.h"

#include "ev-page-action-widget.h"
#include <glib/gi18n-lib.h>

enum {
        PROP_0,
        PROP_PLUGIN
};

struct _EvBrowserPluginToolbarPrivate {
        EvBrowserPlugin *plugin;

        GtkWidget *continuousToggleButton;
        GtkWidget *dualToggleButton;
        GtkWidget *zoomFitPageRadioButton;
        GtkWidget *zoomFitWidthRadioButton;
        GtkWidget *zoomAutomaticRadioButton;
};

G_DEFINE_TYPE(EvBrowserPluginToolbar, ev_browser_plugin_toolbar, GTK_TYPE_TOOLBAR)

static void goToPreviousPage(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->goToPreviousPage();
}

static void goToNextPage(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->goToNextPage();
}

static void activateLink(EvBrowserPluginToolbar *toolbar, EvLink *link)
{
        toolbar->priv->plugin->activateLink(link);
}

static void toggleContinuous(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->toggleContinuous();
}

static void toggleDual(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->toggleDual();
}

static void zoomIn(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->zoomIn();
}

static void zoomOut(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->zoomOut();
}

static void zoomFitPageToggled(EvBrowserPluginToolbar *toolbar)
{
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toolbar->priv->zoomFitPageRadioButton)))
                toolbar->priv->plugin->setSizingMode(EV_SIZING_FIT_PAGE);
}

static void zoomFitWidthToggled(EvBrowserPluginToolbar *toolbar)
{
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toolbar->priv->zoomFitWidthRadioButton)))
                toolbar->priv->plugin->setSizingMode(EV_SIZING_FIT_WIDTH);
}

static void zoomAutomaticToggled(EvBrowserPluginToolbar *toolbar)
{
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toolbar->priv->zoomAutomaticRadioButton)))
                toolbar->priv->plugin->setSizingMode(EV_SIZING_AUTOMATIC);
}

static void printDocument(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->print();
}

static void downloadDocument(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv->plugin->download();
}

class SignalBlocker {
public:
        SignalBlocker(gpointer instance, void (* closure)(EvBrowserPluginToolbar *), gpointer data)
                : m_instance(instance)
                , m_closure(reinterpret_cast<gpointer>(closure))
                , m_data(data)
        {
                g_signal_handlers_block_by_func(m_instance, m_closure, m_data);
        }

        ~SignalBlocker()
        {
                g_signal_handlers_unblock_by_func(m_instance, m_closure, m_data);
        }

private:
        gpointer m_instance;
        gpointer m_closure;
        gpointer m_data;
};

static void continuousChanged(EvDocumentModel *model, GParamSpec *, EvBrowserPluginToolbar *toolbar)
{
        SignalBlocker blocker(toolbar->priv->continuousToggleButton, toggleContinuous, toolbar);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->priv->continuousToggleButton),
                                     toolbar->priv->plugin->isContinuous());
}

static void dualPageChanged(EvDocumentModel *model, GParamSpec *, EvBrowserPluginToolbar *toolbar)
{
        SignalBlocker blocker(toolbar->priv->dualToggleButton, toggleDual, toolbar);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->priv->dualToggleButton),
                                     toolbar->priv->plugin->isDual());
}

static void sizingModeChanged(EvDocumentModel *model, GParamSpec *, EvBrowserPluginToolbar *toolbar)
{
        {
                SignalBlocker fitPageBlocker(toolbar->priv->zoomFitPageRadioButton, zoomFitPageToggled, toolbar);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toolbar->priv->zoomFitPageRadioButton),
                                               toolbar->priv->plugin->sizingMode() == EV_SIZING_FIT_PAGE);
        }

        {
                SignalBlocker fitWidthBlocker(toolbar->priv->zoomFitPageRadioButton, zoomFitWidthToggled, toolbar);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toolbar->priv->zoomFitWidthRadioButton),
                                               toolbar->priv->plugin->sizingMode() == EV_SIZING_FIT_WIDTH);
        }

        {
                SignalBlocker automaticBlocker(toolbar->priv->zoomAutomaticRadioButton, zoomAutomaticToggled, toolbar);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toolbar->priv->zoomAutomaticRadioButton),
                                               toolbar->priv->plugin->sizingMode() == EV_SIZING_AUTOMATIC);
        }
}

static void evBrowserPluginToolbarSetProperty(GObject *object, guint propID, const GValue *value, GParamSpec *paramSpec)
{
        EvBrowserPluginToolbar *toolbar = EV_BROWSER_PLUGIN_TOOLBAR(object);

        switch (propID) {
        case PROP_PLUGIN:
                toolbar->priv->plugin = static_cast<EvBrowserPlugin *>(g_value_get_pointer(value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
        }
}

static GtkWidget *createButton(EvBrowserPluginToolbar *toolbar, const char *iconName, const char *description, GCallback callback)
{
        GtkWidget *button = gtk_button_new();

        gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(button, description);
        gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU));
        gtk_button_set_label(GTK_BUTTON(button), nullptr);
        gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
        g_signal_connect_swapped(button, "clicked", callback, toolbar);

        return button;
}

static GtkWidget *createToggleButton(EvBrowserPluginToolbar *toolbar, const char *iconName, const char *description, bool initialState, GCallback callback)
{
        GtkWidget *button = gtk_toggle_button_new();

        gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(button, description);
        gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU));
        gtk_button_set_label(GTK_BUTTON(button), nullptr);
        gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), initialState);
        g_signal_connect_swapped(button, "toggled", callback, toolbar);

        return button;
}

static GtkWidget *createMenuButton(EvBrowserPluginToolbar *toolbar, const gchar *iconName, GtkWidget *menu, GtkAlign menuAlign)
{
        GtkWidget *button = gtk_menu_button_new();

        gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
        gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU));
        gtk_widget_set_halign(menu, menuAlign);
        gtk_menu_button_set_popup(GTK_MENU_BUTTON(button), menu);

        return button;
}

static GtkWidget *createButtonGroup(EvBrowserPluginToolbar *toolbar)
{
        GtkWidget *box = gtk_box_new(gtk_orientable_get_orientation(GTK_ORIENTABLE(toolbar)), 0);

        GtkStyleContext *styleContext = gtk_widget_get_style_context(box);
        gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_RAISED);
        gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_LINKED);

        return box;
}

static GtkWidget *createSizingModeMenu(EvBrowserPluginToolbar *toolbar)
{
        GtkWidget *menu = gtk_menu_new();

        GtkWidget *menuItem = gtk_check_menu_item_new_with_mnemonic(_("Fit Pa_ge"));
        toolbar->priv->zoomFitPageRadioButton = menuItem;
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(menuItem), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuItem),
                                       toolbar->priv->plugin->sizingMode() == EV_SIZING_FIT_PAGE);
        g_signal_connect_swapped(menuItem, "toggled", G_CALLBACK(zoomFitPageToggled), toolbar);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
        gtk_widget_show(menuItem);

        menuItem = gtk_check_menu_item_new_with_mnemonic(_("Fit _Width"));
        toolbar->priv->zoomFitWidthRadioButton = menuItem;
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(menuItem), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuItem),
                                       toolbar->priv->plugin->sizingMode() == EV_SIZING_FIT_WIDTH);
        g_signal_connect_swapped(menuItem, "toggled", G_CALLBACK(zoomFitWidthToggled), toolbar);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
        gtk_widget_show(menuItem);

        menuItem = gtk_check_menu_item_new_with_mnemonic(_("_Automatic"));
        toolbar->priv->zoomAutomaticRadioButton = menuItem;
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(menuItem), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuItem),
                                       toolbar->priv->plugin->sizingMode() == EV_SIZING_AUTOMATIC);
        g_signal_connect_swapped(menuItem, "toggled", G_CALLBACK(zoomAutomaticToggled), toolbar);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
        gtk_widget_show(menuItem);

        g_signal_connect(toolbar->priv->plugin->model(), "notify::sizing-mode",
                         G_CALLBACK(sizingModeChanged), toolbar);

        return menu;
}

static void evBrowserPluginToolbarConstructed(GObject *object)
{
        G_OBJECT_CLASS(ev_browser_plugin_toolbar_parent_class)->constructed(object);

        EvBrowserPluginToolbar *toolbar = EV_BROWSER_PLUGIN_TOOLBAR(object);
        bool rtl = gtk_widget_get_direction(GTK_WIDGET(toolbar)) == GTK_TEXT_DIR_RTL;

        GtkWidget *hbox = createButtonGroup(toolbar);

        // Navigation buttons
        GtkWidget *button = createButton(toolbar, "go-up-symbolic", _("Go to the previous page"), G_CALLBACK(goToPreviousPage));
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        button = createButton(toolbar, "go-down-symbolic", _("Go to the next page"), G_CALLBACK(goToNextPage));
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        GtkWidget *toolItem = GTK_WIDGET(gtk_tool_item_new());
        if (rtl)
                gtk_widget_set_margin_left(toolItem, 12);
        else
                gtk_widget_set_margin_right(toolItem, 12);
        gtk_container_add(GTK_CONTAINER(toolItem), hbox);
        gtk_widget_show(hbox);

        gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
        gtk_widget_show(toolItem);

        // Page Selector
        toolItem = GTK_WIDGET(g_object_new(EV_TYPE_PAGE_ACTION_WIDGET, nullptr));
        ev_page_action_widget_set_model(EV_PAGE_ACTION_WIDGET(toolItem), toolbar->priv->plugin->model());
        g_signal_connect_swapped(toolItem, "activate-link", G_CALLBACK(activateLink), toolbar);
        if (rtl)
                gtk_widget_set_margin_left(toolItem, 12);
        else
                gtk_widget_set_margin_right(toolItem, 12);
        gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
        gtk_widget_show(toolItem);

        // Separator
        toolItem = GTK_WIDGET(gtk_tool_item_new());
        gtk_tool_item_set_expand(GTK_TOOL_ITEM(toolItem), TRUE);
        gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
        gtk_widget_show(toolItem);

        // View mode
        hbox = createButtonGroup(toolbar);

        button = createToggleButton(toolbar, "view-continuous-symbolic", _("Show the entire document"),
                                    toolbar->priv->plugin->isContinuous(), G_CALLBACK(toggleContinuous));
        toolbar->priv->continuousToggleButton = button;
        g_signal_connect(toolbar->priv->plugin->model(), "notify::continuous",
                         G_CALLBACK(continuousChanged), toolbar);
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        button = createToggleButton(toolbar, "view-dual-symbolic", _("Show two pages at once"),
                                    toolbar->priv->plugin->isDual(), G_CALLBACK(toggleDual));
        toolbar->priv->dualToggleButton = button;
        g_signal_connect(toolbar->priv->plugin->model(), "notify::dual-page",
                         G_CALLBACK(dualPageChanged), toolbar);
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        toolItem = GTK_WIDGET(gtk_tool_item_new());
        if (rtl)
                gtk_widget_set_margin_left(toolItem, 12);
        else
                gtk_widget_set_margin_right(toolItem, 12);
        gtk_container_add(GTK_CONTAINER(toolItem), hbox);
        gtk_widget_show(hbox);

        gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
        gtk_widget_show(toolItem);

        // Zoom
        hbox = createButtonGroup(toolbar);

        button = createButton(toolbar, "zoom-in-symbolic", _("Enlarge the document"), G_CALLBACK(zoomIn));
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        button = createButton(toolbar, "zoom-out-symbolic", _("Shrink the document"), G_CALLBACK(zoomOut));
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        button = createMenuButton(toolbar, "go-down-symbolic", createSizingModeMenu(toolbar), GTK_ALIGN_END);
        gtk_container_add(GTK_CONTAINER(hbox), button);
        gtk_widget_show(button);

        toolItem = GTK_WIDGET(gtk_tool_item_new());
        if (rtl)
                gtk_widget_set_margin_left(toolItem, 12);
        else
                gtk_widget_set_margin_right(toolItem, 12);
        gtk_container_add(GTK_CONTAINER(toolItem), hbox);
        gtk_widget_show(hbox);

        gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
        gtk_widget_show(toolItem);

        // Actions
        // Only add download button if browser is Epiphany for now.
        if (toolbar->priv->plugin->canDownload()) {
                button = createButton(toolbar, "folder-download-symbolic", _("Download document"), G_CALLBACK(downloadDocument));
                toolItem = GTK_WIDGET(gtk_tool_item_new());
                gtk_container_add(GTK_CONTAINER(toolItem), button);
                gtk_widget_show(button);
                if (rtl)
                        gtk_widget_set_margin_left(toolItem, 6);
                else
                        gtk_widget_set_margin_right(toolItem, 6);

                gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
                gtk_widget_show(toolItem);
        }

        button = createButton(toolbar, "printer-symbolic", _("Print document"), G_CALLBACK(printDocument));
        toolItem = GTK_WIDGET(gtk_tool_item_new());
        gtk_container_add(GTK_CONTAINER(toolItem), button);
        gtk_widget_show(button);

        gtk_container_add(GTK_CONTAINER(toolbar), toolItem);
        gtk_widget_show(toolItem);
}

static void ev_browser_plugin_toolbar_class_init(EvBrowserPluginToolbarClass *klass)
{
        GObjectClass *gObjectClass = G_OBJECT_CLASS(klass);
        gObjectClass->set_property = evBrowserPluginToolbarSetProperty;
        gObjectClass->constructed = evBrowserPluginToolbarConstructed;

        g_object_class_install_property(gObjectClass,
                                         PROP_PLUGIN,
                                         g_param_spec_pointer("plugin",
                                                              "Plugin",
                                                              "The plugin",
                                                              static_cast<GParamFlags>(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));

        g_type_class_add_private(gObjectClass, sizeof(EvBrowserPluginToolbarPrivate));
}

static void ev_browser_plugin_toolbar_init(EvBrowserPluginToolbar *toolbar)
{
        toolbar->priv = G_TYPE_INSTANCE_GET_PRIVATE(toolbar, EV_TYPE_BROWSER_PLUGIN_TOOLBAR, EvBrowserPluginToolbarPrivate);
}

GtkWidget *ev_browser_plugin_toolbar_new(EvBrowserPlugin *plugin)
{
        return GTK_WIDGET(g_object_new(EV_TYPE_BROWSER_PLUGIN_TOOLBAR, "plugin", plugin, nullptr));
}
