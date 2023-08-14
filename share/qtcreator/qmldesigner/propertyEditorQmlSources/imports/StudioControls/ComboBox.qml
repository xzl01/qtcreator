// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

import QtQuick
import QtQuick.Window
import QtQuick.Templates as T
import StudioTheme 1.0 as StudioTheme

T.ComboBox {
    id: control

    property StudioTheme.ControlStyle style: StudioTheme.Values.controlStyle

    property alias actionIndicator: actionIndicator
    property alias labelColor: comboBoxInput.color
    property alias textElidable: comboBoxInput.elidable

    // This property is used to indicate the global hover state
    property bool hover: (comboBoxInput.hover || actionIndicator.hover || popupIndicator.hover)
                         && control.enabled
    property bool edit: control.activeFocus && control.editable
    property bool open: comboBoxPopup.opened
    property bool hasActiveDrag: false // an item that can be dropped on the combobox is being dragged
    property bool hasActiveHoverDrag: false // an item that can be dropped on the combobox is being hovered on the combobox

    property bool dirty: false // user modification flag

    property alias actionIndicatorVisible: actionIndicator.visible
    property real __actionIndicatorWidth: control.style.actionIndicatorSize.width
    property real __actionIndicatorHeight: control.style.actionIndicatorSize.height

    property alias textInput: comboBoxInput
    property alias suffix: comboBoxInput.suffix

    property int maximumPopupHeight: control.style.maxComboBoxPopupHeight

    signal compressedActivated(int index, int reason)

    enum ActivatedReason { EditingFinished, Other }

    width: control.style.controlSize.width
    height: control.style.controlSize.height

    leftPadding: actionIndicator.width
    rightPadding: popupIndicator.width + control.style.borderWidth
    font.pixelSize: control.style.baseFontSize
    wheelEnabled: false

    onFocusChanged: {
        if (!control.focus)
            comboBoxPopup.close()
    }

    onActiveFocusChanged: {
        if (control.activeFocus)
            comboBoxInput.preFocusText = control.editText
    }

    ActionIndicator {
        id: actionIndicator
        style: control.style
        __parentControl: control
        x: 0
        y: 0
        width: actionIndicator.visible ? control.__actionIndicatorWidth : 0
        height: actionIndicator.visible ? control.__actionIndicatorHeight : 0
    }

    contentItem: ComboBoxInput {
        id: comboBoxInput

        property string preFocusText: ""

        style: control.style
        __parentControl: control
        text: control.editText

        onEditingFinished: {
            comboBoxInput.deselect()
            comboBoxInput.focus = false
            control.focus = false

            // Only trigger the signal, if the value was modified
            if (control.dirty) {
                timer.stop()
                control.dirty = false
                control.accepted()
            }
        }
        onTextEdited: control.dirty = true
    }

    indicator: CheckIndicator {
        id: popupIndicator
        style: control.style
        __parentControl: control
        __parentPopup: control.popup
        x: comboBoxInput.x + comboBoxInput.width
        y: control.style.borderWidth
        width: control.style.squareControlSize.width - control.style.borderWidth
        height: control.style.squareControlSize.height - control.style.borderWidth * 2
    }

    background: Rectangle {
        id: comboBoxBackground
        color: control.style.background.idle
        border.color: control.style.border.idle
        border.width: control.style.borderWidth
        x: actionIndicator.width
        width: control.width - actionIndicator.width
        height: control.height
    }

    Timer {
        id: timer
        property int activatedIndex
        repeat: false
        running: false
        interval: 100
        onTriggered: control.compressedActivated(timer.activatedIndex,
                                                 ComboBox.ActivatedReason.Other)
    }

    onActivated: function(index) {
        timer.activatedIndex = index
        timer.restart()
    }

    delegate: ItemDelegate {
        id: itemDelegate

        width: comboBoxPopup.width - comboBoxPopup.leftPadding - comboBoxPopup.rightPadding
               - (comboBoxPopupScrollBar.visible ? comboBoxPopupScrollBar.contentItem.implicitWidth
                                                   + 2 : 0) // TODO Magic number
        height: control.style.controlSize.height - 2 * control.style.borderWidth
        padding: 0
        enabled: model.enabled === undefined ? true : model.enabled

        contentItem: Text {
            leftPadding: itemDelegateIconArea.width
            text: control.textRole ? (Array.isArray(control.model)
                                      ? modelData[control.textRole]
                                      : model[control.textRole])
                                   : modelData
            color: {
                if (!itemDelegate.enabled)
                    return control.style.text.disabled

                return itemDelegate.highlighted ? control.style.text.selectedText
                                                : control.style.text.idle
            }
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Item {
            id: itemDelegateIconArea
            width: itemDelegate.height
            height: itemDelegate.height

            T.Label {
                id: itemDelegateIcon
                text: StudioTheme.Constants.tickIcon
                color: itemDelegate.highlighted ? control.style.text.selectedText
                                                : control.style.text.idle
                font.family: StudioTheme.Constants.iconFont.family
                font.pixelSize: control.style.smallIconFontSize
                visible: control.currentIndex === index
                anchors.fill: parent
                renderType: Text.NativeRendering
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        highlighted: control.highlightedIndex === index

        background: Rectangle {
            id: itemDelegateBackground
            x: 0
            y: 0
            width: itemDelegate.width
            height: itemDelegate.height
            color: itemDelegate.highlighted ? control.style.interaction : "transparent"
        }
    }

    popup: T.Popup {
        id: comboBoxPopup
        x: actionIndicator.width
        y: control.height
        width: control.width - actionIndicator.width
        // TODO Setting the height on the popup solved the problem with the popup of height 0,
        // but it has the problem that it sometimes extend over the border of the actual window
        // and is then cut off.
        height: Math.min(contentItem.implicitHeight + comboBoxPopup.topPadding
                         + comboBoxPopup.bottomPadding,
                         control.Window.height - topMargin - bottomMargin,
                         control.maximumPopupHeight)
        padding: control.style.borderWidth
        margins: 0 // If not defined margin will be -1
        closePolicy: T.Popup.CloseOnPressOutside | T.Popup.CloseOnPressOutsideParent
                     | T.Popup.CloseOnEscape | T.Popup.CloseOnReleaseOutside
                     | T.Popup.CloseOnReleaseOutsideParent

        contentItem: ListView {
            id: listView
            clip: true
            implicitHeight: listView.contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                id: comboBoxPopupScrollBar
                visible: listView.height < listView.contentHeight
            }
        }

        background: Rectangle {
            color: control.style.popup.background
            border.width: 0
        }

        enter: Transition {}
        exit: Transition {}
    }

    states: [
        State {
            name: "default"
            when: control.enabled && !control.hover && !control.edit && !control.open
                  && !control.activeFocus && !control.hasActiveDrag
            PropertyChanges {
                target: control
                wheelEnabled: false
            }
            PropertyChanges {
                target: comboBoxInput
                selectByMouse: false
            }
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.idle
            }
        },
        State {
            name: "hover"
            when: control.enabled && control.hover && !control.edit && !control.open
                  && !control.activeFocus && !control.hasActiveDrag
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.hover
            }
        },
        State {
            name: "acceptsDrag"
            when: control.enabled && control.hasActiveDrag && !control.hasActiveHoverDrag
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.interaction
            }
        },
        State {
            name: "dragHover"
            when: control.enabled && control.hasActiveHoverDrag
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.interaction
            }
        },
        // This state is intended for ComboBoxes which aren't editable, but have focus e.g. via
        // tab focus. It is therefor possible to use the mouse wheel to scroll through the items.
        State {
            name: "focus"
            when: control.enabled && control.activeFocus && !control.editable && !control.open
            PropertyChanges {
                target: control
                wheelEnabled: true
            }
            PropertyChanges {
                target: comboBoxInput
                focus: true
            }
        },
        State {
            name: "edit"
            when: control.enabled && control.edit && !control.open
            PropertyChanges {
                target: control
                wheelEnabled: true
            }
            PropertyChanges {
                target: comboBoxInput
                selectByMouse: true
                readOnly: false
            }
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.interaction
            }
            StateChangeScript {
                script: comboBoxPopup.close()
            }
        },
        State {
            name: "popup"
            when: control.enabled && control.open
            PropertyChanges {
                target: control
                wheelEnabled: true
            }
            PropertyChanges {
                target: comboBoxInput
                selectByMouse: false
                readOnly: true
            }
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.interaction
            }
        },
        State {
            name: "disable"
            when: !control.enabled
            PropertyChanges {
                target: comboBoxBackground
                border.color: control.style.border.disabled
            }
        }
    ]

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            control.editText = comboBoxInput.preFocusText
            control.dirty = true
            control.focus = false
        }
    }
}
