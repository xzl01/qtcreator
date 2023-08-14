// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include "itemlibraryinfo.h"
#include <qmldesignercomponents_global.h>

#include <abstractview.h>
#include <modelcache.h>

#include <QImage>
#include <QPointer>
#include <QSize>
#include <QTimer>
#include <QVariant>
#include <QVector>

QT_BEGIN_NAMESPACE
class QAction;
class QInputEvent;
QT_END_NAMESPACE

namespace QmlDesigner {

class BakeLights;
class Edit3DWidget;
class Edit3DAction;
class Edit3DBakeLightsAction;
class Edit3DCameraAction;

class QMLDESIGNERCOMPONENTS_EXPORT Edit3DView : public AbstractView
{
    Q_OBJECT

public:
    Edit3DView(ExternalDependenciesInterface &externalDependencies);

    WidgetInfo widgetInfo() override;

    Edit3DWidget *edit3DWidget() const;

    void selectedNodesChanged(const QList<ModelNode> &selectedNodeList, const QList<ModelNode> &lastSelectedNodeList) override;
    void renderImage3DChanged(const QImage &img) override;
    void updateActiveScene3D(const QVariantMap &sceneState) override;
    void modelAttached(Model *model) override;
    void modelAboutToBeDetached(Model *model) override;
    void importsChanged(const Imports &addedImports, const Imports &removedImports) override;
    void customNotification(const AbstractView *view, const QString &identifier, const QList<ModelNode> &nodeList, const QList<QVariant> &data) override;
    void nodeAtPosReady(const ModelNode &modelNode, const QVector3D &pos3d) override;

    void sendInputEvent(QInputEvent *e) const;
    void edit3DViewResized(const QSize &size) const;

    QSize canvasSize() const;

    void createEdit3DActions();
    QVector<Edit3DAction *> leftActions() const;
    QVector<Edit3DAction *> rightActions() const;
    QVector<Edit3DAction *> visibilityToggleActions() const;
    QVector<Edit3DAction *> backgroundColorActions() const;
    Edit3DAction *edit3DAction(View3DActionType type) const;
    Edit3DBakeLightsAction *bakeLightsAction() const;

    void addQuick3DImport();
    void startContextMenu(const QPoint &pos);
    void dropMaterial(const ModelNode &matNode, const QPointF &pos);
    void dropBundleMaterial(const QPointF &pos);
    void dropTexture(const ModelNode &textureNode, const QPointF &pos);
    void dropComponent(const ItemLibraryEntry &entry, const QPointF &pos);
    void dropAsset(const QString &file, const QPointF &pos);

    bool isBakingLightsSupported() const;

private slots:
    void onEntriesChanged();

private:
    enum class NodeAtPosReqType {
        BundleMaterialDrop,
        ComponentDrop,
        MaterialDrop,
        TextureDrop,
        ContextMenu,
        AssetDrop,
        None
    };

    void registerEdit3DAction(Edit3DAction *action);

    void createEdit3DWidget();
    void checkImports();
    void handleEntriesChanged();
    void showMaterialPropertiesView();

    Edit3DAction *createSelectBackgroundColorAction(QAction *syncBackgroundColorAction);
    Edit3DAction *createGridColorSelectionAction();
    Edit3DAction *createResetColorAction(QAction *syncBackgroundColorAction);
    Edit3DAction *createSyncBackgroundColorAction();
    Edit3DAction *createSeekerSliderAction();

    QPointer<Edit3DWidget> m_edit3DWidget;
    QVector<Edit3DAction *> m_leftActions;
    QVector<Edit3DAction *> m_rightActions;
    QVector<Edit3DAction *> m_visibilityToggleActions;
    QVector<Edit3DAction *> m_backgroundColorActions;

    QMap<View3DActionType, QSharedPointer<Edit3DAction>> m_edit3DActions;
    Edit3DAction *m_selectionModeAction = nullptr;
    Edit3DAction *m_moveToolAction = nullptr;
    Edit3DAction *m_rotateToolAction = nullptr;
    Edit3DAction *m_scaleToolAction = nullptr;
    Edit3DAction *m_fitAction = nullptr;
    Edit3DCameraAction *m_alignCamerasAction = nullptr;
    Edit3DCameraAction *m_alignViewAction = nullptr;
    Edit3DAction *m_cameraModeAction = nullptr;
    Edit3DAction *m_orientationModeAction = nullptr;
    Edit3DAction *m_editLightAction = nullptr;
    Edit3DAction *m_showGridAction = nullptr;
    Edit3DAction *m_showSelectionBoxAction = nullptr;
    Edit3DAction *m_showIconGizmoAction = nullptr;
    Edit3DAction *m_showCameraFrustumAction = nullptr;
    Edit3DAction *m_showParticleEmitterAction = nullptr;
    Edit3DAction *m_resetAction = nullptr;
    Edit3DAction *m_particleViewModeAction = nullptr;
    Edit3DAction *m_particlesPlayAction = nullptr;
    Edit3DAction *m_particlesRestartAction = nullptr;
    Edit3DAction *m_visibilityTogglesAction = nullptr;
    Edit3DAction *m_backgrondColorMenuAction = nullptr;
    Edit3DAction *m_seekerAction = nullptr;
    Edit3DBakeLightsAction *m_bakeLightsAction = nullptr;
    int particlemode;
    ModelCache<QImage> m_canvasCache;
    ModelNode m_droppedModelNode;
    ItemLibraryEntry m_droppedEntry;
    QString m_droppedFile;
    NodeAtPosReqType m_nodeAtPosReqType;
    QPoint m_contextMenuPos;
    QTimer m_compressionTimer;
    QPointer<BakeLights> m_bakeLights;
    bool m_isBakingLightsSupported = false;

    friend class Edit3DAction;
};

} // namespace QmlDesigner
