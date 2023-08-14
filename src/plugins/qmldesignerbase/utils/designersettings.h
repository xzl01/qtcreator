// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "../qmldesignerbase_global.h"

#include <QHash>
#include <QVariant>
#include <QByteArray>
#include <QMutex>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace QmlDesigner {

namespace DesignerSettingsKey {
inline constexpr char ITEMSPACING[] = "ItemSpacing";
inline constexpr char CONTAINERPADDING[] = "ContainerPadding";
inline constexpr char CANVASWIDTH[] = "CanvasWidth";
inline constexpr char CANVASHEIGHT[] = "CanvasHeight";
inline constexpr char ROOT_ELEMENT_INIT_WIDTH[] = "RootElementInitWidth";
inline constexpr char ROOT_ELEMENT_INIT_HEIGHT[] = "RootElementInitHeight";
inline constexpr char WARNING_FOR_FEATURES_IN_DESIGNER[] = "WarnAboutQtQuickFeaturesInDesigner";
inline constexpr char WARNING_FOR_QML_FILES_INSTEAD_OF_UIQML_FILES[]
    = "WarnAboutQmlFilesInsteadOfUiQmlFiles";
inline constexpr char WARNING_FOR_DESIGNER_FEATURES_IN_EDITOR[]
    = "WarnAboutQtQuickDesignerFeaturesInCodeEditor";
inline constexpr char SHOW_DEBUGVIEW[] = "ShowQtQuickDesignerDebugView";
inline constexpr char ENABLE_DEBUGVIEW[] = "EnableQtQuickDesignerDebugView";
inline constexpr char EDIT3DVIEW_BACKGROUND_COLOR[] = "Edit3DViewBackgroundColor";
inline constexpr char EDIT3DVIEW_GRID_COLOR[] = "Edit3DViewGridLineColor";
inline constexpr char ALWAYS_SAVE_IN_CRUMBLEBAR[] = "AlwaysSaveInCrumbleBar";
inline constexpr char USE_DEFAULT_PUPPET[] = "UseDefaultQml2Puppet";
inline constexpr char PUPPET_TOPLEVEL_BUILD_DIRECTORY[] = "PuppetToplevelBuildDirectory";
inline constexpr char PUPPET_DEFAULT_DIRECTORY[] = "PuppetDefaultDirectory";
inline constexpr char CONTROLS_STYLE[] = "ControlsStyle";
inline constexpr char TYPE_OF_QSTR_FUNCTION[] = "TypeOfQsTrFunction";
inline constexpr char SHOW_PROPERTYEDITOR_WARNINGS[] = "ShowPropertyEditorWarnings";
inline constexpr char ENABLE_MODEL_EXCEPTION_OUTPUT[] = "WarnException";
inline constexpr char PUPPET_KILL_TIMEOUT[] = "PuppetKillTimeout";
inline constexpr char DEBUG_PUPPET[] = "DebugPuppet";
inline constexpr char FORWARD_PUPPET_OUTPUT[] = "ForwardPuppetOutput";
inline constexpr char NAVIGATOR_SHOW_ONLY_VISIBLE_ITEMS[] = "NavigatorShowOnlyVisibleItems";
inline constexpr char NAVIGATOR_REVERSE_ITEM_ORDER[] = "NavigatorReverseItemOrder";
inline constexpr char REFORMAT_UI_QML_FILES[]
    = "ReformatUiQmlFiles"; /* These settings are not exposed in ui. */
inline constexpr char IGNORE_DEVICE_PIXEL_RATIO[]
    = "IgnoreDevicePixelRaio"; /* The settings can be used to turn off the feature, if there are serious issues */
inline constexpr char SHOW_DEBUG_SETTINGS[] = "ShowDebugSettings";
inline constexpr char ENABLE_TIMELINEVIEW[] = "EnableTimelineView";
inline constexpr char COLOR_PALETTE_RECENT[] = "ColorPaletteRecent";
inline constexpr char COLOR_PALETTE_FAVORITE[] = "ColorPaletteFavorite";
inline constexpr char ALWAYS_DESIGN_MODE[] = "AlwaysDesignMode";
inline constexpr char DISABLE_ITEM_LIBRARY_UPDATE_TIMER[] = "DisableItemLibraryUpdateTimer";
inline constexpr char ASK_BEFORE_DELETING_ASSET[] = "AskBeforeDeletingAsset";
inline constexpr char SMOOTH_RENDERING[] = "SmoothRendering";
inline constexpr char OLD_STATES_EDITOR[] = "ForceOldStatesEditor";
inline constexpr char EDITOR_ZOOM_FACTOR[] = "EditorZoomFactor";
inline constexpr char ACTIONS_MERGE_TEMPLATE_ENABLED[] = "ActionsMergeTemplateEnabled";
inline constexpr char DOWNLOADABLE_BUNDLES_URL[] = "DownloadableBundlesLocation";
}

class QMLDESIGNERBASE_EXPORT DesignerSettings
{
public:
    DesignerSettings(QSettings *settings);

    void insert(const QByteArray &key, const QVariant &value);
    void insert(const QHash<QByteArray, QVariant> &settingsHash);
    QVariant value(const QByteArray &key, const QVariant &defaultValue = {}) const;

private:
    void fromSettings(QSettings *);
    void toSettings(QSettings *) const;

    void restoreValue(QSettings *settings, const QByteArray &key,
        const QVariant &defaultValue = QVariant());
    void storeValue(QSettings *settings, const QByteArray &key, const QVariant &value) const;

    QSettings *m_settings;
    QHash<QByteArray, QVariant> m_cache;
    mutable QMutex m_mutex;
};

} // namespace QmlDesigner
